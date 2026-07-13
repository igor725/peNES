#pragma once

#include "cpu.hh"

#include <cstdint>

class PPU {
  public:
  PPU(CPU6502& c);
  ~PPU();

  void run(uint8_t cycles);

  protected:
  void pixelEval();
  void step();

  private:
  CPU6502& m_cpu;

  uint16_t m_cycle, m_scanline;
  bool     m_frameReady, m_vblank;
};
