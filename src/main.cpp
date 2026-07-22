#include "apu.hh"
#include "cmdline.hh"
#include "cpu.hh"
#include "ines.hh"
#include "mappers/mapper.hh"
#include "ppu.hh"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>

#if PENES_MICROPROFILE
#include <microprofile.h>
#endif

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Unsupported byte order"
#endif

struct Console {
  using Clock = std::chrono::steady_clock;
  using Delta = std::chrono::duration<double>;

  static constexpr auto TARGET_FRAMETIME = Delta(1.0 / 62.0988);

  union PadState {
    struct {
      uint8_t a      : 1;
      uint8_t b      : 1;
      uint8_t select : 1;
      uint8_t start  : 1;
      uint8_t up     : 1;
      uint8_t down   : 1;
      uint8_t left   : 1;
      uint8_t right  : 1;
    };

    uint8_t _raw = 0;

    uint8_t shift() {
      uint8_t ret = _raw & 0x01;
      _raw >>= 1;
      _raw |= 0x80;
      return ret;
    }
  };

  CPU6502 _cpu;
  PPU     _ppu;
  APU     _apu;
  iNES    _cartridge;

  std::array<PadState, 2> _padBtns;
  std::array<PadState, 2> _padShift;

  int32_t          _batchSize = 0;
  SDL_AudioStream* _stream    = nullptr;

  std::atomic<double> _cyclesDebt = 0;
  std::atomic<double> _speed      = 100.0; // We're starting at 100%
  Clock::time_point   _nextCheck  = {};
  uint8_t             _ticks      = 0;

  std::mutex              _sync;
  std::condition_variable _wait;
  std::jthread            _thread;

  Console(CmdlineParser const& cmdline): _ppu(_cpu), _apu(_cpu) {
#ifndef PENES_NO_SDL
    SDL_AudioSpec spec;

    if (auto const volume = cmdline.getNamedArg<"volume">(0.3).value(); volume > 0.0) {
      if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &_batchSize)) {
        spec.channels = 1, spec.format = SDL_AUDIO_F32LE;
        if ((_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr)) != nullptr) {
          SDL_SetAudioStreamGain(_stream, volume);
          SDL_ResumeAudioStreamDevice(_stream);
          _apu.setSamplingRate(spec.freq);
        }
      }
    }

    if (_stream == nullptr) _apu.setOutputEnabled(false);
#endif

    if (auto hook = cmdline.getNamedArg<"hook">()) {
      if (hook == "test")
        _cpu.setHook(CPU6502::TesterHook);
      else if (hook == "verbosetest")
        _cpu.setHook(CPU6502::VerboseTesterHook);
      else if (hook == "heatmap")
        _cpu.setHook(CPU6502::HeatMapHook);
      else if (hook == "verbosetest")
        _cpu.setHook(CPU6502::VerboseTesterHook);
      else if (hook == "used")
        _cpu.setHook(CPU6502::UsedInstructionsHook);
    }

    // PPU handler
    _cpu.addRangeHandler({0x2000, 0x3FFF}, [&](bool isWrite, uint16_t addr, uint8_t value) -> std::optional<uint8_t> {
      if (isWrite) return _ppu.cpuWrite(addr, value);
      return _ppu.cpuRead(addr);
    });

    // PPU, APU, I/O handler
    _cpu.addRangeHandler({0x4000, 0x4017}, [&, latch = false](bool isWrite, uint16_t addr, uint8_t value) mutable -> std::optional<uint8_t> {
      if (isWrite) {
        if (_apu.handleWrite(addr, value)) {
          return 0;
        } else if (addr == 0x4014) {
          return _ppu.dmaWrite(value);
        } else if (addr == 0x4016) {
          if ((latch = (value & 0x01)) == 0x01) {
            _padShift[0] = _padBtns[0];
            _padShift[1] = _padBtns[1];
          }

          return 0;
        }

        throw;
      }

      if (auto res = _apu.handleRead(addr); res.has_value()) {
        return res.value();
      } else if (addr >= 0x4016 && addr <= 0x4017) { // Read gamepad
        auto const pNum = addr == 0x4016 ? 0 : 1;
        if (latch) {
          _padShift[pNum].a = 1;
          return _padShift[pNum].a & 0b1;
        }
        return _padShift[pNum].shift();
      }

      return {};
    });

    // PPU CHR handler
    _ppu.addRangeHandler({0x0000, 0x1FFF}, [&](bool isWrite, uint16_t addr, uint8_t value) mutable -> uint8_t {
      return _cartridge.getMapper()->ppuOperation(isWrite, addr, value);
    });

    _ppu.setScanlineHook([&](PPU::PPUState const& state) {
      if (state.regs.M && state.scanline < 240 && state.cycle == 260) {
        if (_cartridge.getMapper()->nextScanline()) _cpu.triggerIRQ();
      }
    });

#ifndef PENES_NO_SDL
    // Audio data pusher
    _apu.onData(_batchSize * 2, [&](std::span<float const> sample) { SDL_PutAudioStreamData(_stream, sample.data(), sample.size_bytes()); });
#endif
  }

  void put(std::string const& path, bool doValidation) {
    if (_thread.joinable()) throw;

    if (path == "-")
      _cartridge.piped(doValidation);
    else
      _cartridge.insert(path, doValidation);

    // SRAM, PRG-RAM, PRG-ROM handler
    _cpu.addRangeHandler(_cartridge.getMapper()->getMappedRegion(), [&](bool isWrite, uint16_t addr, uint8_t value) -> uint8_t {
      // Let mapper handle this stuff
      auto const ret = _cartridge.getMapper()->cpuOperation(isWrite, addr, value);
      if (isWrite) /* Temporary hack (probably), just in case if Mapper switched mirroring*/ {
        _ppu.setMirroring(_cartridge->hdr.isVerticalMirror());
      }
      return ret;
    });

    _ppu.setMirroring(_cartridge->hdr.isVerticalMirror());
    _cpu.reset();

    _thread = setupThread();
  }

  std::jthread setupThread() {
    return std::jthread([this](std::stop_token stop) {
#if PENES_MICROPROFILE
      MicroProfileOnThreadCreate("NES processor");
#endif
      constexpr double AVG_ALPHA             = 0.1;
      constexpr double CYCLES_DEPT_THRESHOLD = (double)CPU6502::BASE_CLOCK_FREQUENCY * 0.20 /* 20% speed loss is a big deal */;

      auto lastTime = Clock::now();

      uint16_t resetDept = 0;
      while (!stop.stop_requested()) {
#if PENES_MICROPROFILE
        MICROPROFILE_SCOPEI("NES", "Tick", MP_YELLOW);
#endif
        auto const pred = [&] -> bool {
          if (stop.stop_requested()) return false;
          if (_cyclesDebt <= 0) return false;
          if (_cyclesDebt > CYCLES_DEPT_THRESHOLD) {
            if (++resetDept >= 2048) /* We got systematic performance loss on our hands, bad-bad */ {
              SDL_ClearAudioStream(_stream);
              _cyclesDebt = 0, resetDept = 0;
            }
            return true;
          }
          resetDept = 0;
          return !_ppu.isFrameReady();
        };
        while (pred()) {
          auto const cyclesMade = _cpu.step();
          if (cyclesMade >= 1) {
            _apu.step(cyclesMade);
            _ppu.run(cyclesMade * 3);
          }
          _cyclesDebt -= (double)cyclesMade;
        }

        if (_ppu.isFrameReady()) {
#if PENES_MICROPROFILE
          MICROPROFILE_SCOPEI("NES", "Frame Push", MP_MAGENTA);
#endif
          auto const currentTime = Clock::now();

          if (currentTime > _nextCheck) {
            _speed     = AVG_ALPHA * ((_ticks * TARGET_FRAMETIME.count()) * 100.0) + (1.0 - AVG_ALPHA) * _speed;
            _nextCheck = currentTime + std::chrono::seconds(1);
            _ticks     = 0;
          }

          if (auto const timeElapsed = currentTime - lastTime; timeElapsed < TARGET_FRAMETIME) {
            std::this_thread::sleep_for(TARGET_FRAMETIME - timeElapsed);
          }

          std::unique_lock lock(_sync);

          {
#if PENES_MICROPROFILE
            MICROPROFILE_SCOPEI("NES", "Pull Wait", MP_DEEPSKYBLUE);
#endif
            _wait.wait(lock, [&] { return !_ppu.isFrameReady() || stop.stop_requested(); });
          }

          lastTime = currentTime;
          _ticks += 1;
        } else {
          std::this_thread::yield();
        }
      }
#if PENES_MICROPROFILE
      MicroProfileOnThreadExit();
#endif
    });
  }

  ~Console() { stop(); }

  void stop() {
    _sync.lock();
    _thread.request_stop();
    _wait.notify_one();
    _sync.unlock();
  }

  auto tryLock() { return std::unique_lock(_sync, std::try_to_lock); }

  PPU::Frame<uint32_t> step(std::unique_lock<std::mutex> const& lock, Delta time) {
    _cyclesDebt += CPU6502::BASE_CLOCK_FREQUENCY * std::min(time.count(), 20.0);
    if (!lock.owns_lock() || !_ppu.isFrameReady()) return {};
    return _ppu.getFrame();
  }
};

int32_t main(int32_t argc, char* argv[]) {
#if PENES_MICROPROFILE
  MicroProfileInit();
  std::cerr << "Running profiler on http://localhost:" << MicroProfileWebServerPort() << std::endl;
  MicroProfileOnThreadCreate("Main");
#endif
  CmdlineParser args;

  try {
    args.parse(argc, argv);
  } catch (UnknownCmdlineParameter const& ex) {
    std::cerr << "Unknown command line parameter specified" << std::endl;
    return 1;
  }

#ifndef PENES_NO_SDL
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
    std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
    return 2;
  }

  auto window = SDL_CreateWindow("peNES", 800, 600, SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    std::cerr << "Failed to create a window" << SDL_GetError() << std::endl;
    return 3;
  }
  auto rend = SDL_CreateRenderer(window, nullptr);
  if (rend == nullptr) {
    std::cerr << "Failed to create a renderer" << SDL_GetError() << std::endl;
    return 4;
  }
  auto tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
  if (tex == nullptr) {
    std::cerr << "Failed to create a texture" << SDL_GetError() << std::endl;
    return 5;
  }
  SDL_SetRenderVSync(rend, 1);
#endif

  auto const nesRom = args.getSeqArg<std::string>(0);

  if (!nesRom.has_value()) {
    std::cerr << "Usage: " << argv[0] << " </path/to/application.nes>" << std::endl;
    return 6;
  }

  Console nes(args);

  try {
    nes.put(nesRom.value(), !args.getNamedArg<"skipvalid">(false).value());
  } catch (CartridgeException const& ex) {
    std::cerr << "Failed to load specified cartridge file: " << ex.what() << std::endl;
    return 7;
  }

#ifndef PENES_NO_SDL
  std::array<Console::PadState, 2> currPadState;

  auto lastTime    = Console::Clock::now();
  auto nextMeasure = lastTime;

  bool stopped = false;
  while (!stopped) {
#if PENES_MICROPROFILE
    MICROPROFILE_SCOPEI("Main", "Tick", MP_GREEN);
#endif
    auto const currentTime = Console::Clock::now();
    auto const delta       = currentTime - lastTime;

    if (currentTime >= nextMeasure) {
      SDL_SetWindowTitle(window, std::format("peNES speed: {:0.2f}%", nes._speed.load()).c_str());
      nextMeasure = Console::Clock::now() + std::chrono::seconds(1);
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      switch (ev.type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST: nes._apu.setOutputEnabled(ev.type == SDL_EVENT_WINDOW_FOCUS_GAINED); break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
          stopped = true;
        } break;
        case SDL_EVENT_KEY_DOWN:
          if (ev.key.scancode == SDL_SCANCODE_ESCAPE) nes._cpu.reset();
        case SDL_EVENT_KEY_UP: {
          switch (ev.key.scancode) {
            case SDL_SCANCODE_LEFT: currPadState[0].left = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_RIGHT: currPadState[0].right = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_UP: currPadState[0].up = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_DOWN: currPadState[0].down = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_X: currPadState[0].a = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_Z: currPadState[0].b = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_SPACE: currPadState[0].select = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_RETURN: currPadState[0].start = ev.type == SDL_EVENT_KEY_DOWN; break;
            case SDL_SCANCODE_F1: CPU6502::SetHeatMapReportThreshold(1); break;
            case SDL_SCANCODE_F2: CPU6502::SetHeatMapReportThreshold(10); break;
            case SDL_SCANCODE_F3: CPU6502::SetHeatMapReportThreshold(100); break;
            default: break;
          }
        } break;
        case SDL_EVENT_GAMEPAD_ADDED: {
          if (SDL_IsGamepad(ev.gdevice.which)) {
            SDL_OpenGamepad(ev.gdevice.which);
          }
        } break;
        case SDL_EVENT_GAMEPAD_REMOVED: {
          if (auto pad = SDL_GetGamepadFromID(ev.gdevice.which)) {
            SDL_CloseGamepad(pad);
          }
        } break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
          if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) nes._cpu.reset();
        } /* Intentional fallthrough */
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
          switch (ev.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: currPadState[0].left = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: currPadState[0].right = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_DPAD_UP: currPadState[0].up = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: currPadState[0].down = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_SOUTH: currPadState[0].a = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_EAST: currPadState[0].b = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_BACK: currPadState[0].select = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            case SDL_GAMEPAD_BUTTON_START: currPadState[0].start = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
          }
        } break;
      }
    }

    {
      auto const lock = nes.tryLock();

      if (lock.owns_lock()) nes._padBtns = currPadState;
      if (auto frame = nes.step(lock, delta); !frame.empty()) {
#if PENES_MICROPROFILE
        MICROPROFILE_SCOPEI("Main", "Frame Pull", MP_BLACK);
#endif
        SDL_RenderClear(rend);
        SDL_UpdateTexture(tex, nullptr, frame.data(), frame.pitch_bytes());
      }

      nes._wait.notify_one();
    }

    {
#if PENES_MICROPROFILE
      MICROPROFILE_SCOPEI("Main", "Present", MP_BISQUE);
#endif
      SDL_RenderTexture(rend, tex, nullptr, nullptr);
      SDL_RenderPresent(rend);
    }
#if PENES_MICROPROFILE
    MicroProfileFlip(nullptr);
#endif

    lastTime = currentTime;
  }
  nes.stop();

  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(window);
  SDL_Quit();
#else
  while (true)
    nes.step(nes.tryLock(), Console::TARGET_FRAMETIME);
#endif

#if PENES_MICROPROFILE
  MicroProfileOnThreadExit();
#endif
  return 0;
}
