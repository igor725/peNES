#include "console.hh"

#include <filesystem>
#include <stop_token>

#if PENES_MICROPROFILE
#include <microprofile.h>
#endif

Console::Console(): _ppu(_cpu), _apu(_cpu) {
  // PPU handler
  _cpu.addRangeHandler({0x2000, 0x3FFF}, [&](bool isWrite, uint16_t addr, uint8_t value) -> std::optional<uint8_t> {
    if (isWrite) return _ppu.cpuWrite(addr, value);
    return _ppu.cpuRead(addr);
  });

  // PPU, APU, I/O handler
  _cpu.addRangeHandler({0x4000, 0x401F}, [&, latch = false](bool isWrite, uint16_t addr, uint8_t value) mutable -> std::optional<uint8_t> {
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

        return latch;
      }

      throw;
    }

    if (auto const res = _apu.handleRead(addr); res.has_value()) {
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
    if ((state.regs.M & (PPU::MASK_DRAW_BG | PPU::MASK_DRAW_SPRITE)) && (state.scanline < 240 || state.scanline == 261)) {
      auto const& mapper = _cartridge.getMapper();
      mapper->nextScanline();
      _cpu.setInterrupt(CPU6502::INTRMASK_MAPPERIRQ, mapper->isIRQAsserted() ? CPU6502::INTRMASK_MAPPERIRQ : 0);
    }
  });
}

void Console::put(std::string const& path, bool doValidation) {
  if (_thread.joinable()) throw;

  if (path == "-")
    _cartridge.piped(doValidation);
  else
    _cartridge.insert(path, doValidation);

  // SRAM, PRG-RAM, PRG-ROM handler
  _cpu.addRangeHandler(_cartridge.getMapper()->getMappedRegion(), [&](bool isWrite, uint16_t addr, uint8_t value) -> uint8_t {
    // Let mapper handle this stuff
    auto const ret = _cartridge.getMapper()->cpuOperation(isWrite, addr, value);
    if (isWrite) {
      auto const& mapper = _cartridge.getMapper();
      _ppu.setMirroring(mapper->getMirroringMode());
      _cpu.setInterrupt(CPU6502::INTRMASK_MAPPERIRQ, mapper->isIRQAsserted() ? CPU6502::INTRMASK_MAPPERIRQ : 0);
    }
    return ret;
  });

  _ppu.setMirroring(_cartridge.getMapper()->getMirroringMode());
  _cpu.reset();

  _thread = setupThread();
}

void Console::setAudioOut(uint32_t batchSize, double rate, APU::APUHandler&& handler) {
  std::lock_guard const lock(_sync);
  _apu.onData(batchSize, std::move(handler));
  _apu.setSamplingRate(rate);
}

std::jthread Console::setupThread() {
  return std::jthread([this](std::stop_token stop) {
#if PENES_MICROPROFILE
    MicroProfileOnThreadCreate("NES processor");
#endif
    constexpr double AVG_ALPHA = 0.2;

    auto lastFramePush = Clock::now(), lastCPUTick = Clock::now();

    double cyclesDept = 0;
    while (!stop.stop_requested()) {
#if PENES_MICROPROFILE
      MICROPROFILE_SCOPEI("NES", "Tick", MP_YELLOW);
#endif

      std::unique_lock lock(_sync);

      auto const pred = [&] {
        if (_cpu.isHalted()) {
          _speed = 0.0;
          return false;
        }
        if (cyclesDept < 0) return false;
        if (stop.stop_requested()) return false;
        return !_ppu.isFrameReady();
      };

      auto const currCPUTick = Clock::now();
      while (pred()) {
        auto const cyclesMade = _cpu.step();
        if (cyclesMade >= 1) {
          _apu.step(cyclesMade);
          _ppu.step(cyclesMade);
        }

        cyclesDept -= cyclesMade;
      }
      /* We start skipping cycles if we run on <= 50% of the emulation speed */
      cyclesDept += CPU6502::BASE_CLOCK_FREQUENCY * std::min(std::chrono::duration_cast<Delta>(currCPUTick - lastCPUTick), TARGET_FRAMETIME * 2).count();
      lastCPUTick = currCPUTick;

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

        if (auto const timeElapsed = currentTime - lastFramePush; timeElapsed < TARGET_FRAMETIME) {
          std::this_thread::sleep_for(TARGET_FRAMETIME - timeElapsed);
        }

        lastFramePush = currentTime;
        _wait.wait(lock, [&] { return !_ppu.isFrameReady() || stop.stop_requested(); });

        _ticks += 1;
        continue;
      } else {
        std::this_thread::yield();
        _wait.wait(lock, [&] { return !_cpu.isHalted() || stop.stop_requested(); });
      }
    }
#if PENES_MICROPROFILE
    MicroProfileOnThreadExit();
#endif
  });
}

void Console::stop() {
  std::lock_guard const lock(_sync);
  _thread.request_stop();
  _wait.notify_one();
}

void Console::reset() {
  std::lock_guard const lock(_sync);
  _cpu.reset();
}

PPU::Frame<uint32_t> Console::step(std::unique_lock<Mutex> const& lock) {
  if (!lock.owns_lock() || !_ppu.isFrameReady()) return {};
  return _ppu.getFrame();
}
