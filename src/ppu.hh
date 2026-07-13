#pragma once

#include "cpu.hh"
#include "ines.hh"

#include <cstdint>

class PPU {
  public:
  PPU(CPU6502& c, iNES& i);
  ~PPU();

  uint8_t cpuRead(uint16_t addr);
  uint8_t cpuWrite(uint16_t addr, uint8_t value);

  void run(uint8_t cycles);

  protected:
  uint16_t getNametableMirroringOffset(uint16_t address);
  void     writeInternal(uint16_t addr, uint8_t value);
  uint8_t  readInternal(uint16_t addr);
  void     pixelEval();
  void     step();

  private:
  CPU6502& m_cpu;
  iNES&    m_cartridge;

  uint8_t m_vram[2048];
  uint8_t m_palette[32];

  uint8_t m_reg_ctrl   = 0;
  uint8_t m_reg_mask   = 0;
  uint8_t m_reg_status = 0;

  uint16_t m_vram_address  = 0;
  uint16_t m_tram_address  = 0;
  uint8_t  m_address_latch = 0;
  uint8_t  m_read_buffer   = 0;

  uint16_t m_cycle, m_scanline;

  uint32_t m_frame_buffer[256 * 240];
};
