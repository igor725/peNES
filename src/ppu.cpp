#include "ppu.hh"

#include "cpu.hh"

PPU::PPU(CPU6502& c): m_cpu(c), m_cycle(0), m_scanline(0), m_frameReady(false), m_vblank(false) {}

PPU::~PPU() {}

void PPU::pixelEval() {
  if (m_scanline >= 0 && m_scanline <= 239) {
  } else if (m_scanline == 240) {
    // NOOP
  } else if (m_scanline >= 241 && m_scanline <= 260) {
    if (m_scanline == 241 && m_cycle == 1) m_cpu.writeRam(0x2002, m_cpu.readRam(0x2002) | 0x80);
  } else if (m_scanline == 261 && m_cycle == 1) {
    m_cpu.writeRam(0x2002, m_cpu.readRam(0x2002) & ~0x80);
  }
}

void PPU::step() {
  pixelEval();

  if (++m_cycle > 340) {
    m_cycle = 0;
    if (++m_scanline > 261) {
      m_scanline   = 0;
      m_frameReady = true;
    }
  }
}

void PPU::run(uint8_t cycles) {
  for (uint8_t i = 0; i < cycles; ++i) {
    step();
  }
}
