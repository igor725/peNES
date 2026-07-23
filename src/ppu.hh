#pragma once

#include "mmu.hh"
#include "rgbpalette.hh"

#include <cstdint>
#include <span>

class CPU6502;

class PPU: public MMU {
  static constexpr bool PRE_RP2C02G_BEHAVIOR = false;

  struct RegionTiming {
    uint16_t vblankScanline;
    uint16_t hblankCycle;
    uint16_t preRenderScanline;
    uint16_t totalScanlines;
  };

  static constexpr std::array<RegionTiming, 3> REGION_TIMINGS = {{
      {241, 260, 261, 262}, // NTSC
      {241, 260, 311, 312}, // PAL
      {291, 260, 311, 312}  // Dendy
  }};

  public:
  static constexpr uint16_t PPU_FRAMEBUFFER_PITCH  = 0x100;
  static constexpr uint8_t  CTRL_NAMETAB_SELECT    = 0x03;
  static constexpr uint8_t  CTRL_VRAM_INCREMENT    = 0x04;
  static constexpr uint8_t  CTRL_SPR_TAB_ADDR      = 0x08;
  static constexpr uint8_t  CTRL_BG_TAB_ADDR       = 0x10;
  static constexpr uint8_t  CTRL_TALL_SPRITE       = 0x20;
  static constexpr uint8_t  CTRL_MASTER_SELECT     = 0x40;
  static constexpr uint8_t  CTRL_GEN_NMI           = 0x80;
  static constexpr uint8_t  STATUS_OPEN_BUS        = 0x0F;
  static constexpr uint8_t  STATUS_SPRITE_OVERFLOW = 0x20;
  static constexpr uint8_t  STATUS_SPRITE_ZERO_HIT = 0x40;
  static constexpr uint8_t  STATUS_VBLANK          = 0x80;
  static constexpr uint8_t  MASK_GREYSCALE         = 0x01;
  static constexpr uint8_t  MASK_BG_LC_CLIP        = 0x02;
  static constexpr uint8_t  MASK_SPRITE_LC_CLIP    = 0x04;
  static constexpr uint8_t  MASK_DRAW_BG           = 0x08;
  static constexpr uint8_t  MASK_DRAW_SPRITE       = 0x10;
  static constexpr uint8_t  MASK_EMP_RED           = 0x20;
  static constexpr uint8_t  MASK_EMP_GREEN         = 0x40;
  static constexpr uint8_t  MASK_EMP_BLUE          = 0x80;
  static constexpr uint8_t  SPRITE_PALETTE         = 0x03;
  static constexpr uint8_t  SPRITE_PRIO            = 0x20;
  static constexpr uint8_t  SPRITE_FLIP_HORIZ      = 0x40;
  static constexpr uint8_t  SPRITE_FLIP_VERTI      = 0x80;

  struct PPUState {
    struct {
      uint8_t C = 0;
      uint8_t M = 0;
      uint8_t S = 0;
    } regs;

    uint8_t  fineX          = 0;
    uint8_t  oamAddr        = 0;
    bool     writeLatch     = 0;
    uint8_t  readBuffer     = 0;
    uint8_t  bgTile         = 0;
    uint8_t  bgLow          = 0;
    uint8_t  bgHigh         = 0;
    uint8_t  spritesPerScan = 0;
    uint8_t  decay          = 0;
    uint16_t shiftHigh      = 0;
    uint16_t shiftLow       = 0;
    uint16_t tramAddr       = 0;
    uint16_t vramAddr       = 0;
    uint16_t cycle          = 0;
    uint16_t scanline       = 0;
    uint16_t bgAttr         = 0;
    uint32_t shiftAt        = 0;
    int32_t  nextDecay      = 0;

    uint8_t vram[2048];
    uint8_t palette[32];
    uint8_t oam[256];
  };

  template <typename P>
  struct Frame {

    std::span<P const> pixels;
    uint32_t           pitch;

    bool empty() const { return pixels.empty() || pitch == 0; }

    auto data() const { return pixels.data(); }

    auto pitch_bytes() const { return pitch * sizeof(P); }
  };

  enum class RegionMode : uint8_t { NTSC = 0, PAL = 1, Dendy = 2 };

  using ScanlineHook = std::function<void(PPUState const&)>;

  PPU(CPU6502& c);
  ~PPU();

  std::optional<uint8_t> cpuRead(uint16_t addr);
  std::optional<uint8_t> cpuWrite(uint16_t addr, uint8_t value);
  uint8_t                dmaWrite(uint8_t value);

  void            step(uint8_t cycles);
  Frame<uint32_t> getFrame();

  bool isFrameReady() const { return m_frameReady; };

  void setRegionMode(RegionMode rg) { m_timing = &REGION_TIMINGS[static_cast<uint8_t>(rg)]; }

  void setMirroring(bool value) { m_mirrorVertically = value; }

  void setScanlineHook(ScanlineHook&& hook) { m_scanlineHook = std::move(hook); }

  PPUState dumpState() const { return m_state; }

  void restoreState(PPUState&& state) { m_state = std::move(state); }

  protected:
  uint16_t getNametableMirroringOffset(uint16_t address);
  void     writeInternal(uint16_t addr, uint8_t value);
  uint8_t  readInternal(uint16_t addr);
  void     pixelEval();

  private:
  CPU6502& m_cpu;

  ScanlineHook m_scanlineHook;

  PaletteRGB m_colorPaletteRGB;

  PPUState m_state;

  bool m_frameReady       = false;
  bool m_mirrorVertically = false;

  uint32_t m_frameBuffer[256 * 240];

  RegionTiming const* m_timing = &REGION_TIMINGS[0];
};
