#include "ppu.hh"

#include "cpu.hh"

PPU::PPU(CPU6502& c): m_cpu(c), m_cycle(0), m_scanline(0), m_frameReady(false), m_mirrorVertically(false) {}

PPU::~PPU() {}

uint16_t PPU::getNametableMirroringOffset(uint16_t address) {
  uint16_t normalized = address - 0x2000;

  if (m_mirrorVertically) {
    return normalized % 1024 + (normalized >= 1024 && normalized < 2048 ? 1024 : 0) + (normalized >= 3072 ? 1024 : 0);
  } else {
    if (normalized < 2048) {
      return normalized % 1024;
    } else {
      return (normalized % 1024) + 1024;
    }
  }
}

uint8_t PPU::readInternal(uint16_t addr) {
  addr &= 0x3FFF;
  if (auto it = findHandler(addr); isValidHandler(it)) {
    return it->second(false, addr, 0);
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

uint8_t PPU::cpuRead(uint16_t addr) {
  uint8_t data = 0;

  switch ((addr - 0x2000) % 8) {
    case 0x02: {
      data = m_reg.status;
      m_reg.status &= ~0x80;
      m_addrLatch = 0;
    } break;
    case 0x07 /* PPUDATA */: {
      data         = m_readBuffer;
      m_readBuffer = readInternal(m_vramAddr);

      if (m_vramAddr >= 0x3F00) {
        data         = readInternal(m_vramAddr);
        m_readBuffer = readInternal(m_vramAddr - 0x1000);
      }

      m_vramAddr += (m_reg.ctrl & 0x04) ? 32 : 1;
    } break;
  }

  return data;
}

uint8_t PPU::dmaWrite(uint16_t addr, uint8_t value) {
  if (addr == 0x4014) {
    uint16_t cpuPageAddress = static_cast<uint16_t>(value) << 8;

    for (uint32_t i = 0; i < 256; i++) {
      m_oam[(m_oamAddr + i) & 0xFF] = m_cpu.readRam<uint8_t>(cpuPageAddress + i);
    }
  }

  return 1;
}

uint8_t PPU::cpuWrite(uint16_t addr, uint8_t value) {
  switch ((addr - 0x2000) % 8) {
    case 0x00: m_reg.ctrl = value; break;
    case 0x01: m_reg.mask = value; break;

    case 0x03: m_oamAddr = value; break;
    case 0x04: m_oam[m_oamAddr++] = value; break;

    case 0x05 /* PPUSCROLL */: {
      if (m_addrLatch == 0) {
        m_fineX     = value & 0x07;
        m_tramAddr  = (m_tramAddr & 0xFFE0) | (value >> 3);
        m_addrLatch = 1;
      } else {
        m_tramAddr  = (m_tramAddr & 0x8FFF) | ((value & 0x07) << 12);
        m_tramAddr  = (m_tramAddr & 0xFC1F) | ((value & 0xF8) << 2);
        m_addrLatch = 0;
      }
    } break;

    case 0x06 /* PPUADDR */: {
      if (m_addrLatch == 0) {
        m_tramAddr  = (m_tramAddr & 0x80FF) | ((value & 0x3F) << 8);
        m_addrLatch = 1;
      } else {
        m_tramAddr  = (m_tramAddr & 0xFF00) | value;
        m_vramAddr  = m_tramAddr;
        m_addrLatch = 0;
      }
    } break;

    case 0x07: {
      writeInternal(m_vramAddr, value);
      m_vramAddr += (m_reg.ctrl & 0x04) ? 32 : 1;
    } break;
  }

  return value;
}

void PPU::pixelEval() {
  if (m_scanline >= 0 && m_scanline <= 239) {
    if (m_cycle <= 255) {
      uint16_t tileX = m_cycle / 8;
      uint16_t tileY = m_scanline / 8;

      uint8_t fineX = m_cycle % 8;
      uint8_t fineY = m_scanline % 8;

      uint16_t nametableBase = 0x2000 + ((m_reg.ctrl & 0x03) * 0x0400);

      uint16_t tileAddress = nametableBase + (tileY * 32) + tileX;
      uint8_t  tileID      = readInternal(tileAddress);

      uint16_t patternBase = (m_reg.ctrl & 0x10) ? 0x1000 : 0x0000;

      uint16_t plane0Address = patternBase + (tileID * 16) + fineY;
      uint8_t  plane0Byte    = readInternal(plane0Address);
      uint8_t  plane1Byte    = readInternal(plane0Address + 8);

      uint8_t bitShift        = 7 - fineX;
      uint8_t pixelColorIndex = (((plane1Byte >> bitShift) & 0x01) << 1) | ((plane0Byte >> bitShift) & 0x01);

      bool bgEnabled     = (m_reg.mask & 0x08) != 0;
      bool spriteEnabled = (m_reg.mask & 0x10) != 0;

      uint16_t attributeTableBase = nametableBase + 0x03C0;
      uint16_t attrAddress        = attributeTableBase + ((tileY / 4) * 8) + (tileX / 4);
      uint8_t  attrByte           = readInternal(attrAddress);
      uint8_t  quadrantX          = (tileX % 4) / 2;
      uint8_t  quadrantY          = (tileY % 4) / 2;
      uint8_t  attrShift          = (quadrantY * 4) + (quadrantX * 2);
      uint8_t  paletteSelect      = (attrByte >> attrShift) & 0x03;

      uint8_t bgPixel   = bgEnabled ? pixelColorIndex : 0;
      uint8_t bgPalette = paletteSelect;

      uint8_t fgPixel               = 0;
      uint8_t fgPalette             = 0;
      uint8_t fgPriority            = 0;
      bool    spriteZeroHitPossible = false;

      if (spriteEnabled) {
        for (int i = 0; i < 64; ++i) {
          uint8_t spriteY    = m_oam[i * 4 + 0];
          uint8_t sprTile    = m_oam[i * 4 + 1];
          uint8_t attributes = m_oam[i * 4 + 2];
          uint8_t spriteX    = m_oam[i * 4 + 3];

          uint8_t spriteHeight = (m_reg.ctrl & 0x20) ? 16 : 8;

          if (m_scanline >= (spriteY + 1) && m_scanline < (spriteY + 1 + spriteHeight)) {
            if (m_cycle >= spriteX && m_cycle < (spriteX + 8)) {
              uint8_t spriteFineY = m_scanline - (spriteY + 1);
              uint8_t spriteFineX = m_cycle - spriteX;

              if (attributes & 0x80) spriteFineY = (spriteHeight - 1) - spriteFineY;
              if (attributes & 0x40) spriteFineX = 7 - spriteFineX;

              uint16_t sprPatternBase = 0;

              if (spriteHeight == 16) {
                sprPatternBase = (sprTile & 0x01) ? 0x1000 : 0x0000;
                sprTile &= 0xFE;
                if (spriteFineY >= 8) {
                  sprTile |= 0x01;
                  spriteFineY -= 8;
                }
              } else {
                sprPatternBase = (m_reg.ctrl & 0x08) ? 0x1000 : 0x0000;
              }

              uint16_t sprPlane0Addr = sprPatternBase + (sprTile * 16) + spriteFineY;
              uint8_t  sprPlane0     = readInternal(sprPlane0Addr);
              uint8_t  sprPlane1     = readInternal(sprPlane0Addr + 8);

              uint8_t sprBitShift   = 7 - spriteFineX;
              uint8_t sprColorIndex = (((sprPlane1 >> sprBitShift) & 0x01) << 1) | ((sprPlane0 >> sprBitShift) & 0x01);

              if (sprColorIndex != 0) {
                fgPixel    = sprColorIndex;
                fgPalette  = (attributes & 0x03) + 4;
                fgPriority = (attributes & 0x20) == 0;
                if (i == 0) spriteZeroHitPossible = true;
                break;
              }
            }
          }
        }
      }

      uint8_t finalPixel   = 0;
      uint8_t finalPalette = 0;

      if (bgPixel == 0 && fgPixel == 0) {
        finalPixel   = 0;
        finalPalette = 0;
      } else if (bgPixel == 0 && fgPixel > 0) {
        finalPixel   = fgPixel;
        finalPalette = fgPalette;
      } else if (bgPixel > 0 && fgPixel == 0) {
        finalPixel   = bgPixel;
        finalPalette = bgPalette;
      } else if (bgPixel > 0 && fgPixel > 0) {
        if (spriteZeroHitPossible) {
          bool clipBG      = !(m_reg.mask & 0x02);
          bool clipSprites = !(m_reg.mask & 0x04);
          if (!(m_cycle < 8 && (clipBG || clipSprites)) && m_cycle < 255) {
            m_reg.status |= 0x40;
          }
        }

        if (fgPriority) {
          finalPixel   = fgPixel;
          finalPalette = fgPalette;
        } else {
          finalPixel   = bgPixel;
          finalPalette = bgPalette;
        }
      }

      uint16_t finalPaletteAddress = 0x3F00 + (finalPalette << 2) + finalPixel;
      if (finalPixel == 0) finalPaletteAddress = 0x3F00;
      m_frameBuffer[(m_scanline * 256) + m_cycle] = m_colorPalette[readInternal(finalPaletteAddress) & 0x3F];
    }
  } else if (m_scanline == 240) {
    // NOOP
  } else if (m_scanline >= 241 && m_scanline <= 260) {
    if (m_scanline == 241 && m_cycle == 1) {
      m_reg.status |= 0x80;
      if (m_reg.ctrl & 0x80) m_cpu.triggerNmi();
    }
  } else if (m_scanline == 261 && m_cycle == 1) {
    m_reg.status &= ~0xE0;
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

std::span<uint32_t const> PPU::getFrame() {
  m_frameReady = false;
  return m_frameBuffer;
}

std::span<uint8_t> PPU::prepareCHRMemory() {
  if (m_chrRam.empty()) {
    auto a   = new uint8_t[0x2000];
    m_chrRam = {a, 0x2000};
  }

  return m_chrRam;
}
