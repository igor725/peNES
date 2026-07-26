#include "cmdline.hh"
#include "console.hh"
#include "gui/base.hh"
#include "ines.hh"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>

#if PENES_MICROPROFILE
#include <microprofile.h>
#endif

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Unsupported byte order"
#endif

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
  SDL_AudioStream* stream = nullptr;
  SDL_AudioSpec    spec;
  int32_t          batchSize = 0;
  if (auto const volume = args.getNamedArg<"volume">(0.3).value(); volume > 0.0) {
    if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &batchSize)) {
      spec.channels = 1, spec.format = SDL_AUDIO_F32LE;
      if ((stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr)) != nullptr) {
        SDL_SetAudioStreamGain(stream, volume);
        SDL_ResumeAudioStreamDevice(stream);
      }
    }
  }

  auto const nesRom = args.getSeqArg<std::string>(0);

  if (!nesRom.has_value()) {
    std::cerr << "Usage: " << argv[0] << " </path/to/application.nes>" << std::endl;
    return 6;
  }

  Console nes;
  GUI     gui;
  gui.init(window, rend, stream);
  nes.setAudioOut(batchSize, spec.freq, [&](std::span<float const> sample) { SDL_PutAudioStreamData(stream, sample.data(), sample.size_bytes()); });

  try {
    nes.put(nesRom.value(), !args.getNamedArg<"skipvalid">(false).value());
  } catch (CartridgeException const& ex) {
    std::cerr << "Failed to load specified cartridge file: " << ex.what() << std::endl;
    return 7;
  }

  std::array<Console::PadState, 2> currPadState;

  auto lastTime = Console::Clock::now();

  bool stopped = false, guiActive = false;
  while (!stopped) {
#if PENES_MICROPROFILE
    MICROPROFILE_SCOPEI("Main", "Tick", MP_GREEN);
#endif
    auto const currentTime = Console::Clock::now();
    auto const delta       = currentTime - lastTime;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      gui.forwardEvent(&ev);
      switch (ev.type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST: nes._apu.setOutputEnabled(ev.type == SDL_EVENT_WINDOW_FOCUS_GAINED); break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
          stopped = true;
        } break;
        case SDL_EVENT_KEY_DOWN:
          if (ev.key.scancode == SDL_SCANCODE_ESCAPE) nes._cpu.reset();
          if (ev.key.scancode == SDL_SCANCODE_S) nes.saveState();
          if (ev.key.scancode == SDL_SCANCODE_R) nes.restoreState();
          if (ev.key.scancode == SDL_SCANCODE_G) guiActive ^= true;
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
            default: break;
          }
        } break;
        case SDL_EVENT_GAMEPAD_ADDED: {
          if (SDL_IsGamepad(ev.gdevice.which)) {
            SDL_OpenGamepad(ev.gdevice.which);
          }
        } break;
        case SDL_EVENT_GAMEPAD_REMOVED: {
          if (auto const pad = SDL_GetGamepadFromID(ev.gdevice.which)) {
            SDL_CloseGamepad(pad);
          }
        } break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
          if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) nes.reset();
          if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) nes.saveState();
          if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) nes.restoreState();
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
      /*
       * TODO: Adjust this lock to the monitor's flip rate?
       * Something like WaitDuration = (1 / RefreshRate) - 30us (ImGui's drawing headroom)
       */
      auto const lock = nes.mkLock(std::chrono::milliseconds(4));

      if (lock.owns_lock()) nes._padBtns = currPadState;
      if (auto frame = nes.step(lock); !frame.empty()) {
#if PENES_MICROPROFILE
        MICROPROFILE_SCOPEI("Main", "Frame Pull", MP_BLACK);
#endif
        SDL_UpdateTexture(tex, nullptr, frame.data(), frame.pitchBytes());
      }

      nes._wait.notify_one();
    }

    {
#if PENES_MICROPROFILE
      MICROPROFILE_SCOPEI("Main", "Present", MP_BISQUE);
#endif
      SDL_RenderClear(rend);
      SDL_RenderTexture(rend, tex, nullptr, nullptr);
      if (guiActive && !gui.produceFrame(nes)) guiActive = false;
      SDL_RenderPresent(rend);
    }
#if PENES_MICROPROFILE
    MicroProfileFlip(nullptr);
#endif

    lastTime = currentTime;
  }
  nes.stop();
  gui.deinit();

  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(window);
  SDL_Quit();

#if PENES_MICROPROFILE
  MicroProfileOnThreadExit();
#endif
  return 0;
}
