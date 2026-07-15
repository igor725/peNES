#pragma once

#include "cpu.hh"
#include "mmu.hh"
#include "palette.hh"

#include <cstdint>
#include <span>

class PPU: public MMU {
  public:
  PPU(CPU6502& c);
  ~PPU();

  uint8_t cpuRead(uint16_t addr);
  uint8_t cpuWrite(uint16_t addr, uint8_t value);
  uint8_t dmaWrite(uint16_t addr, uint8_t value);

  void                      run(uint8_t cycles);
  std::span<uint32_t const> getFrame();

  bool isFrameReady() const { return m_frameReady; };

  void setVerticalMirroring() { m_mirrorVertically = true; }

  std::span<uint8_t> prepareCHRMemory();

  protected:
  uint16_t getNametableMirroringOffset(uint16_t address);
  void     writeInternal(uint16_t addr, uint8_t value);
  uint8_t  readInternal(uint16_t addr);
  void     pixelEval();
  void     step();

  private:
  CPU6502& m_cpu;

  Palette m_colorPalette;

  uint8_t m_vram[2048];
  uint8_t m_palette[32];
  uint8_t m_oam[256] = {0};

  struct {
    uint8_t ctrl   = 0;
    uint8_t mask   = 0;
    uint8_t status = 0;
  } m_reg;

  uint8_t  m_oamAddr    = 0;
  uint16_t m_vramAddr   = 0;
  uint16_t m_tramAddr   = 0;
  uint8_t  m_addrLatch  = 0;
  uint8_t  m_fineX      = 0;
  uint8_t  m_readBuffer = 0;

  uint16_t m_cycle, m_scanline;

  uint32_t m_frameBuffer[256 * 240];

  std::span<uint8_t> m_chrRam;

  bool m_frameReady, m_mirrorVertically;
};
