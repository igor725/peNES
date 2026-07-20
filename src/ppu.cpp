#include "ppu.hh"

#include "cpu.hh"

#include <cstdint>

PPU::PPU(CPU6502& c): m_cpu(c), m_cycle(0), m_scanline(0), m_frameReady(false), m_mirrorVertically(false) {}

PPU::~PPU() {
  delete[] m_chrRam.data();
}

uint16_t PPU::getNametableMirroringOffset(uint16_t addr) {
  addr &= 0x1FFF;

  if (m_mirrorVertically) {
    return addr % 0x400 + (addr >= 0x400 && addr < 0x800 ? 0x400 : 0) + (addr >= 0xC00 ? 0x400 : 0);
  } else {
    return addr < 0x800 ? addr % 0x400 : (addr % 0x400) + 0x400;
  }
}

uint8_t PPU::readInternal(uint16_t addr) {
  addr &= 0x3FFF;
  if (auto it = findHandler(addr); isValidHandler(it)) {
    if (auto ret = it->second(false, addr, 0); ret.has_value()) return ret.value();
  }

  if (addr >= 0x2000 && addr <= 0x3EFF) {
    uint16_t nt_address = addr;
    if (nt_address >= 0x3000) nt_address -= 0x1000;
    uint16_t vram_offset = getNametableMirroringOffset(nt_address);
    return m_vram[vram_offset];
  } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
    uint16_t palette_address = (addr - 0x3F00) & 0x001F;
    if ((palette_address & 0x0003) == 0 && (palette_address & 0x0010)) palette_address &= 0x000F;
    return m_palette[palette_address];
  }

  return 0;
}

void PPU::writeInternal(uint16_t addr, uint8_t value) {
  addr &= 0x3FFF;
  if (auto it = findHandler(addr); isValidHandler(it)) {
    it->second(true, addr, value);
    return;
  }

  if (addr >= 0x2000 && addr <= 0x3EFF) {
    uint16_t nt_address = addr;
    if (nt_address >= 0x3000) {
      nt_address -= 0x1000;
    }

    uint16_t vram_offset = getNametableMirroringOffset(nt_address);
    m_vram[vram_offset]  = value;
  } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
    uint16_t palette_address = (addr - 0x3F00) & 0x001F;

    if ((palette_address & 0x0003) == 0 && (palette_address & 0x0010)) {
      palette_address &= 0x000F;
    }

    m_palette[palette_address] = value;
  }
}

std::optional<uint8_t> PPU::cpuRead(uint16_t addr) {
  uint8_t data;

  switch ((addr - 0x2000) % 8) {
    case 0x02 /* PPUSTATUS */: {
      data = m_regs.S;
      m_regs.S &= ~STATUS_VBLANK;
      m_addrLatch = 0;
    } break;
    case 0x04 /* OAMDATA */: data = m_oam[m_oamAddr]; break;
    case 0x07 /* PPUDATA */: {
      data         = m_readBuffer;
      m_readBuffer = readInternal(m_vramAddr);

      if (m_vramAddr >= 0x3F00) {
        data         = readInternal(m_vramAddr);
        m_readBuffer = readInternal(m_vramAddr - 0x1000);
      }

      m_vramAddr += (m_regs.C & CTRL_VRAM_INCREMENT) ? 32 : 1;
    } break;

    default: return {};
  }

  return data;
}

uint8_t PPU::dmaWrite(uint8_t value) {
  uint16_t cpuPageAddress = static_cast<uint16_t>(value) << 8;

  for (uint32_t i = 0; i < 256; i++) {
    m_oam[(m_oamAddr + i) & 0xFF] = m_cpu.readMem<uint8_t>(cpuPageAddress + i);
  }

  return 0;
}

std::optional<uint8_t> PPU::cpuWrite(uint16_t addr, uint8_t value) {
  switch ((addr - 0x2000) % 8) {
    case 0x00: /* PPUCTRL */ {
      if (m_regs.S & STATUS_VBLANK) /* CTRL happend inside VBLANK */ {
        if ((m_regs.C & CTRL_GEN_NMI) == 0 && (value & CTRL_GEN_NMI) > 0) {
          m_cpu.triggerNMI();
        }
      }
      m_regs.C   = value;
      m_tramAddr = (m_tramAddr & 0xF3FF) | ((value & 0x03) << 10);
    } break;
    case 0x01: /* PPUMASK */ m_regs.M = value; break;

    case 0x03 /*  OAMADDR */: m_oamAddr = value; break;
    case 0x04 /*  PPUDATA */: m_oam[m_oamAddr++] = value; break;

    case 0x05 /* PPUSCROLL */: {
      if (m_addrLatch == 0) {
        m_fineX    = value & 0x07;
        m_tramAddr = (m_tramAddr & 0xFFE0) | (value >> 3);
      } else {
        m_tramAddr = (m_tramAddr & 0x8FFF) | ((value & 0x07) << 12);
        m_tramAddr = (m_tramAddr & 0xFC1F) | ((value & 0xF8) << 2);
      }

      m_addrLatch ^= 1;
    } break;

    case 0x06 /* PPUADDR */: {
      if (m_addrLatch == 0) {
        m_tramAddr = (m_tramAddr & 0x80FF) | ((value & 0x3F) << 8);
      } else {
        m_tramAddr = (m_tramAddr & 0xFF00) | value;
        m_vramAddr = m_tramAddr;
      }

      m_addrLatch ^= 1;
    } break;

    case 0x07 /*  PPUDATA */: {
      writeInternal(m_vramAddr, value);
      m_vramAddr += (m_regs.C & 0x04) ? 32 : 1;
    } break;
  }

  return {};
}

void PPU::step() {
  if (m_regs.M & (MASK_DRAW_SPRITE | MASK_DRAW_BG)) {
    if (m_scanline < 240) {
      if (m_cycle <= 255 || (m_cycle >= 320 && m_cycle <= 340)) {
        if (m_cycle <= 255) {
          uint8_t bgColor = 0;
          uint8_t palette = 0;

          if (m_regs.M & MASK_DRAW_BG) {
            if (m_cycle >= 8 || (m_regs.M & MASK_BG_LC_CLIP)) {
              bgColor = (m_shiftHigh >> (14 - m_fineX) & 2) | (m_shiftLow >> (15 - m_fineX) & 1);
              palette = m_shiftAt >> (28 - m_fineX * 2) & 12;
            }
          }

          uint8_t renderColor = bgColor;
          if (m_regs.M & MASK_DRAW_SPRITE) {
            for (uint8_t i = 0; i < 64; ++i) {
              uint16_t spriteHeight  = m_regs.C & CTRL_TALL_SPRITE ? 16 : 8;
              uint16_t spriteY       = m_scanline - m_oam[i * 4 + 0] - 1;
              uint16_t spriteAttribs = m_oam[i * 4 + 2];
              uint16_t spriteX       = m_cycle - m_oam[i * 4 + 3];

              if (spriteX < 8 && spriteY < spriteHeight) {
                uint16_t sx      = spriteX ^ !(spriteAttribs & SPRITE_FLIP_HORIZ) * 7;
                uint16_t sy      = spriteY ^ (spriteAttribs & SPRITE_FLIP_VERTI ? spriteHeight - 1 : 0);
                uint16_t sprTile = m_oam[i * 4 + 1];
                uint16_t sprAddr;

                if (spriteHeight == 16) {
                  uint16_t tableBit  = (sprTile % 2) << 12;
                  uint16_t tileIndex = (sprTile << 4) & 0xFFE0;
                  uint16_t rowOffset = (sy & 8) << 1;

                  sprAddr = tableBit | tileIndex | rowOffset;
                } else {
                  uint16_t tableBit  = (m_regs.C & CTRL_SPR_TAB_ADDR) << 9;
                  uint16_t tileIndex = sprTile << 4;

                  sprAddr = tableBit | tileIndex;
                }
                sprAddr |= (sy & 7);

                if (uint16_t spriteColor = (readInternal(sprAddr + 8) >> sx << 1 & 2) | (readInternal(sprAddr) >> sx & 1)) {
                  if (i == 0 && bgColor != 0 && m_cycle < 255) m_regs.S |= STATUS_SPRITE_ZERO_HIT;
                  if (++m_spritesPerScan >= 9) m_regs.S |= STATUS_SPRITE_OVERFLOW;
                  if (!(m_cycle < 8 && !(m_regs.M & MASK_SPRITE_LC_CLIP))) {
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

          m_frameBuffer[(m_scanline * PPU_FRAMEBUFFER_PITCH) + m_cycle] =
              m_colorPalette[readInternal(0x3F00 + (renderColor ? palette | renderColor : 0)) & 0x3F];
        }

        if (m_cycle < 336) {
          m_shiftAt *= 4;
          m_shiftHigh *= 2;
          m_shiftLow *= 2;
        }

        uint16_t const nameTabAddr = ((m_regs.C & CTRL_BG_TAB_ADDR) ? 0x1000 : 0x0000) | (m_bgTile << 4) | (m_vramAddr >> 12);
        switch (m_cycle & 7) {
          case 1: m_bgTile = readInternal(0x2000 | (m_vramAddr & 0x0FFF)); break;
          case 3: {
            uint16_t const attributeAddress = 0x23C0 | (m_vramAddr & 0x0C00) | ((m_vramAddr >> 4) & 0x38) | ((m_vramAddr >> 2) & 0x07);
            uint32_t const shiftAmount      = (((m_vramAddr >> 5) & 0x02) | ((m_vramAddr >> 1) & 0x01)) * 2;
            m_bgAttr                        = ((readInternal(attributeAddress) >> shiftAmount) & 0x03) * 0x5555;
          } break;
          case 5: m_bgLow = readInternal(nameTabAddr); break;
          case 7: {
            m_bgHigh = readInternal(nameTabAddr | 8);
            if ((m_vramAddr & 0x1F) == 0x1f) {
              m_vramAddr &= ~0x001F;
              m_vramAddr ^= 0x0400;
            } else {
              m_vramAddr += 1;
            }
            m_shiftHigh |= m_bgHigh;
            m_shiftLow |= m_bgLow;
            m_shiftAt |= m_bgAttr;
          } break;
        }
      }

      if (m_cycle == PPU_FRAMEBUFFER_PITCH) {
        if ((m_vramAddr & 0x7000) != 0x7000) {
          m_vramAddr = m_vramAddr + 0x1000;
        } else if ((m_vramAddr & 0x3E0) == 0x3A0) {
          m_vramAddr = (m_vramAddr & 0x8C1F) ^ 0x800;
        } else if ((m_vramAddr & 0x3E0) == 0x3E0) {
          m_vramAddr = m_vramAddr & 0x8C1F;
        } else {
          m_vramAddr = (m_vramAddr & 0x8C1F) | ((m_vramAddr + 0x20) & 0x03E0);
        }

        m_vramAddr &= ~0x41F;
        m_vramAddr |= m_tramAddr & 0x41F;
      }
    }

    if (m_scanline == m_timing->preRenderScanline && m_cycle >= 280 && m_cycle <= 304) m_vramAddr = (m_vramAddr & 0x841F) | (m_tramAddr & 0x7BE0);
  }

  if (m_cycle == 1) {
    if (m_scanline == m_timing->vblankScanline) {
      m_regs.S |= STATUS_VBLANK;
      if (m_regs.C & CTRL_GEN_NMI) m_cpu.triggerNMI();
      m_frameReady = true;
    }
    if (m_scanline == m_timing->preRenderScanline) m_regs.S = 0;
  }

  if (++m_cycle >= 341) {
    m_cycle = 0, m_spritesPerScan = 0;
    if (++m_scanline >= m_timing->totalScanlines) m_scanline = 0;
  }
}

void PPU::run(uint8_t cycles) {
  for (uint8_t i = 0; i < cycles; ++i) {
    step();
  }
}

PPU::Frame<uint32_t> PPU::getFrame() {
  m_frameReady = false;
  return {m_frameBuffer, PPU_FRAMEBUFFER_PITCH};
}
