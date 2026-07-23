#include "ppu.hh"

#include "cpu.hh"

#include <cstdint>

#if PENES_MICROPROFILE
#include <microprofile.h>
#endif

PPU::PPU(CPU6502& c): MMU(0), m_cpu(c) {}

PPU::~PPU() {}

uint16_t PPU::getNametableMirroringOffset(uint16_t addr) {
  addr &= 0x1FFF;

  if (m_mirrorVertically) {
    return addr & 0x3FF + (addr >= 0x400 && addr < 0x800 ? 0x400 : 0) + (addr >= 0xC00 ? 0x400 : 0);
  } else {
    return addr < 0x800 ? addr & 0x3FF : (addr & 0x3FF) + 0x400;
  }
}

uint8_t PPU::readInternal(uint16_t addr) {
  addr &= 0x3FFF;
  if (auto const han = findHandler(addr)) {
    if (auto ret = (*han)(false, addr, 0); ret.has_value()) return ret.value();
  }

  if (addr >= 0x2000 && addr <= 0x3EFF) {
    uint16_t nt_address = addr;
    if (nt_address >= 0x3000) nt_address -= 0x1000;
    uint16_t vram_offset = getNametableMirroringOffset(nt_address);
    return m_state.vram[vram_offset];
  } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
    uint16_t palette_address = (addr - 0x3F00) & 0x001F;
    if ((palette_address & 0x0003) == 0 && (palette_address & 0x0010)) palette_address &= 0x000F;
    auto const paletteMask = m_state.regs.M & MASK_GREYSCALE ? 0b00110000 : 0b00111111;
    return (m_state.palette[palette_address] & paletteMask) | (m_state.decay & 0xC0);
  }

  return 0;
}

void PPU::writeInternal(uint16_t addr, uint8_t value) {
  addr &= 0x3FFF;
  if (auto const han = findHandler(addr)) {
    (*han)(true, addr, value);
    return;
  }

  if (addr >= 0x2000 && addr <= 0x3EFF) {
    uint16_t nt_address = addr;
    if (nt_address >= 0x3000) {
      nt_address -= 0x1000;
    }

    uint16_t vram_offset      = getNametableMirroringOffset(nt_address);
    m_state.vram[vram_offset] = value;
  } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
    uint16_t palette_address = (addr - 0x3F00) & 0x001F;

    if ((palette_address & 0x0003) == 0 && (palette_address & 0x0010)) {
      palette_address &= 0x000F;
    }

    m_state.palette[palette_address] = value;
  }
}

std::optional<uint8_t> PPU::cpuRead(uint16_t addr) {
  uint8_t data;

  switch ((addr - 0x2000) & 0x07) {
    case 0x00: data = m_state.decay; break;
    case 0x02 /* PPUSTATUS */: {
      if constexpr (PRE_RP2C02G_BEHAVIOR) {
        data = m_state.regs.S | (m_state.decay & 0b00011111);
      } else {
        m_state.decay &= 0b00011111, m_state.nextDecay = CPU6502::BASE_CLOCK_FREQUENCY;
        data = m_state.regs.S | m_state.decay;
      }
      m_state.regs.S &= ~STATUS_VBLANK;
      m_state.writeLatch = 0;
    } break;
    case 0x04 /* OAMDATA */: {
      data = m_state.oam[m_state.oamAddr] & (PRE_RP2C02G_BEHAVIOR ? 0b11100001 : 0b11111111); // According to AccuracyCoin this is a pre-PPU rev.G behavior
      if (m_state.oamAddr > 0 && ((m_state.oamAddr & 1) == 0)) {
        m_state.decay = 0, m_state.nextDecay = CPU6502::BASE_CLOCK_FREQUENCY;
      }
    } break;
    case 0x07 /* PPUDATA */: {
      if (m_state.vramAddr >= 0x3F00) /* Palette RAM */ {
        data               = readInternal(m_state.vramAddr);
        m_state.readBuffer = readInternal(m_state.vramAddr - 0x1000);
      } else {
        data               = m_state.readBuffer;
        m_state.readBuffer = readInternal(m_state.vramAddr);
      }

      m_state.vramAddr += (m_state.regs.C & CTRL_VRAM_INCREMENT) ? 32 : 1;
      m_state.decay = data, m_state.nextDecay = CPU6502::BASE_CLOCK_FREQUENCY;
    } break;

    default: return m_state.decay;
  }

  return data;
}

uint8_t PPU::dmaWrite(uint8_t value) {
  uint16_t cpuPageAddress = static_cast<uint16_t>(value) << 8;

  for (uint32_t i = 0; i < 256; i++) {
    m_state.oam[(m_state.oamAddr + i) & 0xFF] = m_cpu.readMem<uint8_t>(cpuPageAddress + i);
  }

  return 0;
}

std::optional<uint8_t> PPU::cpuWrite(uint16_t addr, uint8_t value) {
  switch ((addr - 0x2000) & 0x07) {
    case 0x00: /* PPUCTRL */ {
      if (m_state.regs.S & STATUS_VBLANK) /* CTRL happend inside VBLANK */ {
        if ((m_state.regs.C & CTRL_GEN_NMI) == 0 && (value & CTRL_GEN_NMI) > 0) {
          m_cpu.triggerNMI();
        }
      }
      m_state.regs.C   = value;
      m_state.tramAddr = (m_state.tramAddr & 0xF3FF) | ((value & 0x03) << 10);
    } break;
    case 0x01: /* PPUMASK */ m_state.regs.M = value; break;

    case 0x03 /*  OAMADDR */: m_state.oamAddr = value; break;
    case 0x04 /*  PPUDATA */: m_state.oam[m_state.oamAddr++] = value; break;

    case 0x05 /* PPUSCROLL */: {
      if (m_state.writeLatch == 0) {
        m_state.fineX    = value & 0x07;
        m_state.tramAddr = (m_state.tramAddr & 0xFFE0) | (value >> 3);
      } else {
        m_state.tramAddr = (m_state.tramAddr & 0x8FFF) | ((value & 0x07) << 12);
        m_state.tramAddr = (m_state.tramAddr & 0xFC1F) | ((value & 0xF8) << 2);
      }

      m_state.writeLatch ^= 1;
    } break;

    case 0x06 /* PPUADDR */: {
      if (m_state.writeLatch == 0) {
        m_state.tramAddr = (m_state.tramAddr & 0x80FF) | ((value & 0x3F) << 8);
      } else {
        m_state.tramAddr = (m_state.tramAddr & 0xFF00) | value;
        m_state.vramAddr = m_state.tramAddr;
      }

      m_state.writeLatch ^= 1;
    } break;

    case 0x07 /*  PPUDATA */: {
      writeInternal(m_state.vramAddr, value);
      m_state.vramAddr += (m_state.regs.C & 0x04) ? 32 : 1;
    } break;
  }

  m_state.decay = value, m_state.nextDecay = CPU6502::BASE_CLOCK_FREQUENCY;
  return {};
}

void PPU::pixelEval() {
  if (m_state.regs.M & (MASK_DRAW_SPRITE | MASK_DRAW_BG)) {
    if (m_state.scanline < 240) {
      if (m_state.cycle <= 255 || (m_state.cycle >= 320 && m_state.cycle <= 340)) {
        if (m_state.cycle <= 255) {
          uint8_t bgColor = 0;
          uint8_t palette = 0;

          if (m_state.regs.M & MASK_DRAW_BG) {
            if (m_state.cycle >= 8 || (m_state.regs.M & MASK_BG_LC_CLIP)) {
              bgColor = (m_state.shiftHigh >> (14 - m_state.fineX) & 2) | (m_state.shiftLow >> (15 - m_state.fineX) & 1);
              palette = m_state.shiftAt >> (28 - m_state.fineX * 2) & 12;
            }
          }

          uint8_t renderColor = bgColor;
          if (m_state.regs.M & MASK_DRAW_SPRITE) {
            for (uint8_t i = 0; i < 64; ++i) {
              uint16_t spriteHeight  = m_state.regs.C & CTRL_TALL_SPRITE ? 16 : 8;
              uint16_t spriteY       = m_state.scanline - m_state.oam[i * 4 + 0] - 1;
              uint16_t spriteAttribs = m_state.oam[i * 4 + 2];
              uint16_t spriteX       = m_state.cycle - m_state.oam[i * 4 + 3];

              if (spriteX < 8 && spriteY < spriteHeight) {
                uint16_t sx      = spriteX ^ !(spriteAttribs & SPRITE_FLIP_HORIZ) * 7;
                uint16_t sy      = spriteY ^ (spriteAttribs & SPRITE_FLIP_VERTI ? spriteHeight - 1 : 0);
                uint16_t sprTile = m_state.oam[i * 4 + 1];
                uint16_t sprAddr;

                if (spriteHeight == 16) {
                  uint16_t tableBit  = (sprTile % 2) << 12;
                  uint16_t tileIndex = (sprTile << 4) & 0xFFE0;
                  uint16_t rowOffset = (sy & 8) << 1;

                  sprAddr = tableBit | tileIndex | rowOffset;
                } else {
                  uint16_t tableBit  = (m_state.regs.C & CTRL_SPR_TAB_ADDR) << 9;
                  uint16_t tileIndex = sprTile << 4;

                  sprAddr = tableBit | tileIndex;
                }
                sprAddr |= (sy & 7);

                if (uint16_t spriteColor = (readInternal(sprAddr + 8) >> sx << 1 & 2) | (readInternal(sprAddr) >> sx & 1)) {
                  if (i == 0 && bgColor != 0 && m_state.cycle < 255) m_state.regs.S |= STATUS_SPRITE_ZERO_HIT;
                  if (++m_state.spritesPerScan >= 9) m_state.regs.S |= STATUS_SPRITE_OVERFLOW;
                  if (!(m_state.cycle < 8 && !(m_state.regs.M & MASK_SPRITE_LC_CLIP))) {
                    if (!((spriteAttribs & SPRITE_PRIO) && bgColor != 0)) {
                      renderColor = spriteColor;
                      palette     = 0x10 | ((spriteAttribs & SPRITE_PALETTE) * 4);
                    }
                    break;
                  }
                }
              }
            }
          }

          m_frameBuffer[(m_state.scanline * PPU_FRAMEBUFFER_PITCH) + m_state.cycle] =
              m_colorPaletteRGB[readInternal(0x3F00 + (renderColor ? palette | renderColor : 0)) & 0x3F];
        }

        if (m_state.cycle < 336) {
          m_state.shiftAt *= 4;
          m_state.shiftHigh *= 2;
          m_state.shiftLow *= 2;
        }

        uint16_t const nameTabAddr = ((m_state.regs.C & CTRL_BG_TAB_ADDR) ? 0x1000 : 0x0000) | (m_state.bgTile << 4) | (m_state.vramAddr >> 12);
        switch (m_state.cycle & 7) {
          case 1: m_state.bgTile = readInternal(0x2000 | (m_state.vramAddr & 0x0FFF)); break;
          case 3: {
            uint16_t const attributeAddress = 0x23C0 | (m_state.vramAddr & 0x0C00) | ((m_state.vramAddr >> 4) & 0x38) | ((m_state.vramAddr >> 2) & 0x07);
            uint32_t const shiftAmount      = (((m_state.vramAddr >> 5) & 0x02) | ((m_state.vramAddr >> 1) & 0x01)) * 2;
            m_state.bgAttr                  = ((readInternal(attributeAddress) >> shiftAmount) & 0x03) * 0x5555;
          } break;
          case 5: m_state.bgLow = readInternal(nameTabAddr); break;
          case 7: {
            m_state.bgHigh = readInternal(nameTabAddr | 8);
            if ((m_state.vramAddr & 0x1F) == 0x1f) {
              m_state.vramAddr &= ~0x001F;
              m_state.vramAddr ^= 0x0400;
            } else {
              m_state.vramAddr += 1;
            }
            m_state.shiftHigh |= m_state.bgHigh;
            m_state.shiftLow |= m_state.bgLow;
            m_state.shiftAt |= m_state.bgAttr;
          } break;
        }
      }

      if (m_state.cycle == PPU_FRAMEBUFFER_PITCH) {
        if ((m_state.vramAddr & 0x7000) != 0x7000) {
          m_state.vramAddr = m_state.vramAddr + 0x1000;
        } else if ((m_state.vramAddr & 0x3E0) == 0x3A0) {
          m_state.vramAddr = (m_state.vramAddr & 0x8C1F) ^ 0x800;
        } else if ((m_state.vramAddr & 0x3E0) == 0x3E0) {
          m_state.vramAddr = m_state.vramAddr & 0x8C1F;
        } else {
          m_state.vramAddr = (m_state.vramAddr & 0x8C1F) | ((m_state.vramAddr + 0x20) & 0x03E0);
        }

        m_state.vramAddr &= ~0x41F;
        m_state.vramAddr |= m_state.tramAddr & 0x41F;
      }
    }

    if (m_state.scanline == m_timing->preRenderScanline && m_state.cycle >= 280 && m_state.cycle <= 304)
      m_state.vramAddr = (m_state.vramAddr & 0x841F) | (m_state.tramAddr & 0x7BE0);
  }

  if (m_state.cycle == 1) {
    if (m_state.scanline == m_timing->vblankScanline) {
      m_state.regs.S |= STATUS_VBLANK;
      if (m_state.regs.C & CTRL_GEN_NMI) m_cpu.triggerNMI();
      m_frameReady = true;
    } else if (m_state.scanline == m_timing->preRenderScanline) {
      m_state.regs.S = 0;
    }
  } else if (m_state.cycle == m_timing->hblankCycle) {
    if (m_scanlineHook) m_scanlineHook(m_state);
  }

  if (++m_state.cycle >= 341) {
    m_state.cycle = 0, m_state.spritesPerScan = 0;
    if (++m_state.scanline >= m_timing->totalScanlines) m_state.scanline = 0;
  }
}

void PPU::step(uint8_t cycles) {
#if PENES_MICROPROFILE
  MICROPROFILE_SCOPEI("NES", "PPU Run", MP_DEEPSKYBLUE);
#endif

  if (m_state.decay != 0 && (m_state.nextDecay -= cycles) <= 0) {
    m_state.decay = 0;
  }

  m_frameReady = false; // Drop previous frame immediately if it was there, main loop ignored it

  for (uint8_t i = 0; i < cycles * 3; ++i) {
    pixelEval();
  }
}

PPU::Frame<uint32_t> PPU::getFrame() {
  m_frameReady = false;
  return {m_frameBuffer, PPU_FRAMEBUFFER_PITCH};
}
