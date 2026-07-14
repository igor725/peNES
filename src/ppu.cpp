#include "ppu.hh"

#include "cpu.hh"
#include "ines.hh"

struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static const RGB NES_SYSTEM_PALETTE[64] = {
    {0x54, 0x54, 0x54}, {0x00, 0x1E, 0x74}, {0x08, 0x10, 0x90}, {0x30, 0x00, 0x88}, {0x44, 0x00, 0x64}, {0x50, 0x00, 0x30}, {0x40, 0x04, 0x00},
    {0x30, 0x18, 0x00}, {0x12, 0x2C, 0x00}, {0x00, 0x3A, 0x00}, {0x00, 0x3E, 0x00}, {0x00, 0x3C, 0x14}, {0x00, 0x32, 0x60}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x98, 0x96, 0x96}, {0x08, 0x4C, 0xC4}, {0x30, 0x32, 0xEC}, {0x5C, 0x1E, 0xE4}, {0x88, 0x14, 0xB4},
    {0xA0, 0x14, 0x68}, {0x9E, 0x22, 0x00}, {0x7A, 0x42, 0x00}, {0x48, 0x5C, 0x00}, {0x00, 0x72, 0x00}, {0x00, 0x7C, 0x00}, {0x00, 0x76, 0x44},
    {0x00, 0x66, 0x94}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0xEC, 0xEE, 0xEE}, {0x4C, 0x9C, 0xFF}, {0x7C, 0x78, 0xFF},
    {0xB4, 0x62, 0xFF}, {0xE4, 0x5D, 0xFF}, {0xFC, 0x5E, 0xB4}, {0xFB, 0x6E, 0x54}, {0xDF, 0x8C, 0x14}, {0xB4, 0xA6, 0x00}, {0x7A, 0xBC, 0x00},
    {0x44, 0xCA, 0x2A}, {0x44, 0xC4, 0x7C}, {0x34, 0xB4, 0xCC}, {0x30, 0x30, 0x30}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0xEF, 0xEF, 0xEF},
    {0xB6, 0xDA, 0xFF}, {0xC8, 0xCA, 0xFF}, {0xDF, 0xC2, 0xFF}, {0xF4, 0xC0, 0xFF}, {0xFF, 0xC0, 0xE4}, {0xFF, 0xC8, 0xB6}, {0xF4, 0xD6, 0x98},
    {0xDF, 0xE2, 0x88}, {0xC4, 0xEB, 0x88}, {0xA6, 0xEF, 0x9C}, {0xA4, 0xEE, 0xC2}, {0x9E, 0xE7, 0xE4}, {0xA4, 0xA4, 0xA4}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
};

PPU::PPU(CPU6502& c, iNES& i): m_cpu(c), m_cartridge(i), m_cycle(0), m_scanline(0), m_frameReady(false) {}

PPU::~PPU() {}

uint16_t PPU::getNametableMirroringOffset(uint16_t address) {
  uint16_t normalized = address - 0x2000;

  if (m_cartridge.isVerticalMirror()) {
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

  if (addr >= 0x0000 && addr <= 0x1FFF) {
    return m_cartridge.readChr(addr);
  } else if (addr >= 0x2000 && addr <= 0x3EFF) {
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

  if (addr >= 0x0000 && addr <= 0x1FFF) {
    m_cartridge.writeChr(addr, value);
  } else if (addr >= 0x2000 && addr <= 0x3EFF) {
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
    case 2:
      data = m_reg.status;
      m_reg.status &= ~0x80;
      m_addrLatch = 0;
      break;

    case 7:
      data = readInternal(m_vramAddr);
      m_vramAddr += (m_reg.ctrl & 0x04) ? 32 : 1;
      break;
  }
  return data;
}

uint8_t PPU::dmaWrite(uint16_t addr, uint8_t value) {
  if (addr == 0x4014) {
    uint16_t cpuPageAddress = static_cast<uint16_t>(value) << 8;

    for (uint32_t i = 0; i < 256; i++) {
      m_oam[m_oamAddr + i] = m_cpu.readRam(cpuPageAddress + i);
    }
  }

  return 1;
}

uint8_t PPU::cpuWrite(uint16_t addr, uint8_t value) {
  switch ((addr - 0x2000) % 8) {
    case 0: m_reg.ctrl = value; break;
    case 1: m_reg.mask = value; break;

    case 3: m_oamAddr = value; break;
    case 4: m_oam[m_oamAddr++] = value; break;

    case 6:
      if (m_addrLatch == 0) {
        m_tramAddr  = (m_tramAddr & 0x00FF) | ((value & 0x3F) << 8);
        m_addrLatch = 1;
      } else {
        m_tramAddr  = (m_tramAddr & 0xFF00) | value;
        m_vramAddr  = m_tramAddr;
        m_addrLatch = 0;
      }
      break;

    case 7:
      writeInternal(m_vramAddr, value);
      m_vramAddr += (m_reg.ctrl & 0x04) ? 32 : 1;
      break;
  }

  return value;
}

void PPU::pixelEval() {
  if (m_scanline >= 0 && m_scanline <= 239) {
    if (m_cycle >= 0 && m_cycle <= 255) {
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

      if (bgEnabled && spriteEnabled && !(m_reg.status & 0x40)) {
        uint8_t spriteY     = m_oam[0];
        uint8_t tileID0     = m_oam[1];
        uint8_t attributes0 = m_oam[2];
        uint8_t spriteX     = m_oam[3];

        uint8_t spriteHeight = (m_reg.ctrl & 0x20) ? 16 : 8;

        if (m_scanline >= (spriteY + 1) && m_scanline < (spriteY + 1 + spriteHeight)) {
          if (m_cycle >= spriteX && m_cycle < (spriteX + 8)) {
            uint8_t spriteFineY = m_scanline - (spriteY + 1);
            uint8_t spriteFineX = m_cycle - spriteX;

            if (attributes0 & 0x80) {
              spriteFineY = (spriteHeight - 1) - spriteFineY;
            }
            if (attributes0 & 0x40) {
              spriteFineX = 7 - spriteFineX;
            }

            uint16_t sprPatternBase = 0;
            uint8_t  sprTile        = tileID0;

            if (spriteHeight == 16) {
              sprPatternBase = (tileID0 & 0x01) ? 0x1000 : 0x0000;
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

            if (sprColorIndex != 0 && pixelColorIndex != 0) {
              bool clipBG      = !(m_reg.mask & 0x02);
              bool clipSprites = !(m_reg.mask & 0x04);

              if (!(m_cycle < 8 && (clipBG || clipSprites))) {
                if (m_cycle < 255) {
                  m_reg.status |= 0x40;
                }
              }
            }
          }
        }
      }

      uint16_t attributeTableBase = nametableBase + 0x03C0;

      uint16_t attrAddress = attributeTableBase + ((tileY / 4) * 8) + (tileX / 4);
      uint8_t  attrByte    = readInternal(attrAddress);

      uint8_t quadrantX     = (tileX % 4) / 2;
      uint8_t quadrantY     = (tileY % 4) / 2;
      uint8_t attrShift     = (quadrantY * 4) + (quadrantX * 2);
      uint8_t paletteSelect = (attrByte >> attrShift) & 0x03;

      uint16_t finalPaletteAddress = 0x3F00 + (paletteSelect << 2) + pixelColorIndex;
      if (pixelColorIndex == 0) finalPaletteAddress = 0x3F00;

      uint8_t nesColorByte = readInternal(finalPaletteAddress);

      RGB color = NES_SYSTEM_PALETTE[nesColorByte & 0x3F];

      m_frame_buffer[(m_scanline * 256) + m_cycle] = 0xFF000000 | (color.r << 16) | (color.g << 8) | color.b;
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

std::span<uint32_t const> PPU::getFrame() const {
  m_frameReady = false;
  return m_frame_buffer;
}
