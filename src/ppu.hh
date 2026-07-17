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
  uint8_t dmaWrite(uint8_t value);

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
  static constexpr uint8_t CTRL_NAMETAB_SELECT    = 0x03;
  static constexpr uint8_t CTRL_VRAM_INCREMENT    = 0x04;
  static constexpr uint8_t CTRL_SPR_TAB_ADDR      = 0x08;
  static constexpr uint8_t CTRL_BG_TAB_ADDR       = 0x10;
  static constexpr uint8_t CTRL_TALL_SPRITE       = 0x20;
  static constexpr uint8_t CTRL_MASTER_SELECT     = 0x40;
  static constexpr uint8_t CTRL_GEN_NMI           = 0x80;
  static constexpr uint8_t STATUS_OPEN_BUS        = 0x0F;
  static constexpr uint8_t STATUS_SPRITE_OVERFLOW = 0x10;
  static constexpr uint8_t STATUS_SPRITE_ZERO_HIT = 0x40;
  static constexpr uint8_t STATUS_VBLANK          = 0x80;
  static constexpr uint8_t MASK_GREYSCALE         = 0x01;
  static constexpr uint8_t MASK_BG_LC_CLIP        = 0x02;
  static constexpr uint8_t MASK_SPRITE_LC_CLIP    = 0x04;
  static constexpr uint8_t MASK_DRAW_BG           = 0x08;
  static constexpr uint8_t MASK_DRAW_SPRITE       = 0x10;
  static constexpr uint8_t MASK_EMP_RED           = 0x20;
  static constexpr uint8_t MASK_EMP_GREEN         = 0x40;
  static constexpr uint8_t MASK_EMP_BLUE          = 0x80;
  static constexpr uint8_t SPRITE_PALETTE         = 0x03;
  static constexpr uint8_t SPRITE_PRIO            = 0x20;
  static constexpr uint8_t SPRITE_FLIP_HORIZ      = 0x40;
  static constexpr uint8_t SPRITE_FLIP_VERTI      = 0x80;

  CPU6502& m_cpu;

  Palette m_colorPalette;

  struct {
    uint8_t C = 0;
    uint8_t M = 0;
    uint8_t S = 0;
  } m_regs;

  uint16_t m_shiftHigh  = 0;
  uint16_t m_shiftLow   = 0;
  uint16_t m_tramAddr   = 0;
  uint16_t m_vramAddr   = 0;
  uint16_t m_cycle      = 0;
  uint16_t m_scanline   = 0;
  uint16_t m_bgAttr     = 0;
  uint8_t  m_fineX      = 0;
  uint8_t  m_oamAddr    = 0;
  uint8_t  m_addrLatch  = 0;
  uint8_t  m_readBuffer = 0;
  uint8_t  m_bgTile     = 0;
  uint8_t  m_bgLow      = 0;
  uint8_t  m_bgHigh     = 0;
  uint32_t m_shiftAt    = 0;

  bool m_frameReady       = false;
  bool m_mirrorVertically = false;

  uint32_t m_frameBuffer[256 * 240];
  uint8_t  m_vram[2048];
  uint8_t  m_palette[32];
  uint8_t  m_oam[256];

  std::span<uint8_t> m_chrRam;
};
