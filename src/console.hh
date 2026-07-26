#pragma once

#include "apu.hh"
#include "cpu.hh"
#include "ines.hh"
#include "mappers/mapper.hh"
#include "ppu.hh"

#include <chrono>
#include <condition_variable>
#include <shared_mutex>
#include <thread>

struct Console {
  using Clock      = std::chrono::steady_clock;
  using Delta      = std::chrono::duration<double>;
  using Mutex      = std::shared_timed_mutex;
  using SharedLock = std::shared_lock<Mutex>;
  using UniqueLock = std::unique_lock<Mutex>;
  using GuardLock  = std::lock_guard<Mutex>;
  using LockTmRes  = std::chrono::milliseconds;

  static constexpr auto TARGET_FRAMETIME = Delta(1.0 / 60.0988);

  struct FullState {
    CPU6502::CPUState    cpuState;
    PPU::PPUState        ppuState;
    APU::APUState        apuState;
    std::vector<uint8_t> mapperState;
  };

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

  double            _speed     = 100.0; // We're starting at 100%
  Clock::time_point _nextCheck = {};
  uint8_t           _ticks     = 0;

  Mutex                       _sync;
  std::condition_variable_any _wait;
  std::jthread                _thread;

  std::optional<FullState> _fullState;

  Console();

  ~Console() { stop(); }

  void put(std::string const& path, bool doValidation);

  void setAudioOut(uint32_t batchSize, double rate, APU::APUHandler&& handler);

  std::jthread setupThread();

  void saveState();

  void restoreState();

  void stop();

  template <typename LockType = std::unique_lock<Mutex>>
  auto mkLock(LockTmRes wait = LockTmRes::max()) {
    if (wait == LockTmRes(0)) return LockType(_sync, std::try_to_lock);
    if (wait != LockTmRes::max()) return LockType(_sync, wait);
    return LockType(_sync);
  }

  PPU::Frame<uint32_t> step(std::unique_lock<Mutex> const& lock);
};
