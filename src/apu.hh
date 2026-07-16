#pragma once

#include "cpu.hh"

#include <cstdint>
#include <optional>

class APU {
  public:
  APU(CPU6502& c);
  ~APU();

  std::optional<uint8_t> handleRead(uint16_t addr);

  bool handleWrite(uint16_t addr, uint8_t value);

  private:
  CPU6502& m_cpu;
};
