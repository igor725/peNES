#include "cpu.hh"
#include "ines.hh"
#include "ppu.hh"

#include <SDL3/SDL.h>
#include <cassert>
#include <iostream>

constexpr double TARGET_FRAMETIME = 1.0 / 60.0988;

int main(int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
    return 1;
  }

  auto window = SDL_CreateWindow("peNES", 800, 600, 0);
  auto rend   = SDL_CreateRenderer(window, nullptr);
  auto tex    = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
  SDL_SetRenderVSync(rend, 1);

  iNES cartridge(argv[1]);
  if (!cartridge.get()->isNTSC()) {
    std::cerr << "Only NTSC cartridges are supported atm" << std::endl;
    return 2;
  }
  if (cartridge->getMapperId() != 0x00) {
    std::cerr << "Only Mapper 0 cartridges are supported atm" << std::endl;
    return 3;
  }
  CPU6502 cpu;
  PPU     ppu(cpu, cartridge);

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

    uint8_t _raw;

    uint8_t getFirstBtnState() const { return a; }

    uint8_t shift() {
      uint8_t ret = _raw & 0x01;
      _raw >>= 1;
      _raw |= 0x80;
      return ret;
    }
  };

  PadState padBtns[2], padShift[2];

  // PPU handler
  cpu.addRangeHandler({0x2000, 0x2007}, [&](bool isWrite, uint16_t addr, uint8_t value) -> uint8_t {
    if (isWrite) return ppu.cpuWrite(addr, value);
    return ppu.cpuRead(addr);
  });

  // PPU DMA handler
  cpu.addRangeHandler({0x4014, 0x4014}, [&](bool isWrite, uint16_t addr, uint8_t value) -> uint8_t {
    if (isWrite) return ppu.dmaWrite(addr, value);
    return value;
  });

  // PPU CHR handler
  ppu.addRangeHandler({0x0000, 0x1FFF}, [&, chr = nullptr](bool isWrite, uint16_t addr, uint8_t value) mutable -> uint8_t {
    auto const romAddr = cartridge.resolvePPU(addr);
    if (romAddr == 0) /* No CHR data in cartridge */ {
      auto const chrRam = ppu.prepareCHRMemory();
      if (isWrite) return chrRam[addr] = value;
      return chrRam[addr];
    }

    // Skip all writes
    return cartridge->data[romAddr];
  });

  // Gamepad handler
  cpu.addRangeHandler({0x4016, 0x4017}, [&padBtns, &padShift, latch = false](bool isWrite, uint16_t addr, uint8_t value) mutable -> uint8_t {
    if (isWrite) {
      if (addr == 0x4016) {
        if ((latch = (value & 0x01)) == 0x01) {
          padShift[0] = padBtns[0];
          padShift[1] = padBtns[1];
        }
      }

      return value;
    }

    auto const pNum = addr == 0x4016 ? 0 : 1;
    if (latch) {
      return padBtns[pNum].getFirstBtnState();
    } else {
      return padShift[pNum].shift();
    }

    return value;
  });

  // PRG-ROM handler
  cpu.addRangeHandler({0x8000, 0xFFFF}, [&](bool isWrite, uint16_t addr, uint8_t value) -> uint8_t {
    // We're ignoring writes here completely
    auto const romAddr = cartridge.resolveCPU(addr);
    return cartridge->data[romAddr];
  });

  if (cartridge->isVerticalMirror()) ppu.setVerticalMirroring();
  cpu.reset();

  auto       lastTime = SDL_GetPerformanceCounter();
  auto const perfFreq = static_cast<double>(SDL_GetPerformanceFrequency());

  bool   stopped    = false;
  double nextUpdate = 0.0, lag = 0.0;
  while (!stopped) {
    Uint64 currentTime = SDL_GetPerformanceCounter();
    lag += static_cast<double>(currentTime - lastTime) / SDL_GetPerformanceFrequency();
    lastTime = currentTime;

    while (lag >= TARGET_FRAMETIME) {
      while (!ppu.isFrameReady()) {
        auto cycles = cpu.step();
        if (cycles >= 1) ppu.run(cycles * 3);
      }

      SDL_Event ev;
      while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
          case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            stopped = true;
          } break;
          case SDL_EVENT_GAMEPAD_ADDED: {
            if (SDL_IsGamepad(ev.gdevice.which)) {
              SDL_OpenGamepad(ev.gdevice.which);
            }
          } break;
          case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
          case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            switch (ev.gbutton.button) {
              case SDL_GAMEPAD_BUTTON_DPAD_LEFT: padBtns[0].left = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: padBtns[0].right = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_DPAD_UP: padBtns[0].up = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_DPAD_DOWN: padBtns[0].down = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_LABEL_A: padBtns[0].a = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_LABEL_B: padBtns[0].b = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_BACK: padBtns[0].select = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
              case SDL_GAMEPAD_BUTTON_START: padBtns[0].start = ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; break;
            }
          } break;
        }
      }

      lag -= TARGET_FRAMETIME;
    }

    auto frame = ppu.getFrame();
    SDL_UpdateTexture(tex, nullptr, frame.data(), 256 * 4);
    SDL_RenderClear(rend);
    SDL_RenderTexture(rend, tex, nullptr, nullptr);
    SDL_RenderPresent(rend);
  }
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
