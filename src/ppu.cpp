#include "ppu.hh"

#include "cpu.hh"
#include "ines.hh"

#include <fstream>
#include <vector>

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

void saveFramebufferToBmp(const uint32_t* framebuffer, const std::string& filename) {
  const uint32_t width  = 256;
  const uint32_t height = 240;

  const uint32_t rowSize       = width * 3;
  const uint32_t pixelDataSize = rowSize * height;
  const uint32_t fileSize      = 54 + pixelDataSize;

  std::ofstream file(filename, std::ios::out | std::ios::binary);
  if (!file) return;

  // 1. Standard 14-byte Bitmap File Header
  uint8_t fileHeader[14] = {
      'B',
      'M',
      static_cast<uint8_t>(fileSize & 0xFF),
      static_cast<uint8_t>((fileSize >> 8) & 0xFF),
      static_cast<uint8_t>((fileSize >> 16) & 0xFF),
      static_cast<uint8_t>((fileSize >> 24) & 0xFF),
      0,
      0,
      0,
      0,
      54,
      0,
      0,
      0,
  };
  file.write(reinterpret_cast<char*>(fileHeader), sizeof(fileHeader));

  uint8_t dibHeader[40] = {
      40,
      0,
      0,
      0,
      static_cast<uint8_t>(width & 0xFF),
      static_cast<uint8_t>((width >> 8) & 0xFF),
      0,
      0,
      static_cast<uint8_t>(height & 0xFF),
      static_cast<uint8_t>((height >> 8) & 0xFF),
      0,
      0,
      1,
      0,
      24,
      0,
      0,
      0,
      0,
      0,
      static_cast<uint8_t>(pixelDataSize & 0xFF),
      static_cast<uint8_t>((pixelDataSize >> 8) & 0xFF),
      static_cast<uint8_t>((pixelDataSize >> 16) & 0xFF),
      static_cast<uint8_t>((pixelDataSize >> 24) & 0xFF),
      0x13,
      0x0B,
      0,
      0,
      0x13,
      0x0B,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  };
  file.write(reinterpret_cast<char*>(dibHeader), sizeof(dibHeader));

  std::vector<uint8_t> rowBuffer(rowSize);

  for (int y = static_cast<int>(height) - 1; y >= 0; --y) {
    size_t bufferIndex = 0;
    for (uint32_t x = 0; x < width; ++x) {
      uint32_t packedPixel = framebuffer[y * width + x];

      uint8_t r = (packedPixel >> 16) & 0xFF;
      uint8_t g = (packedPixel >> 8) & 0xFF;
      uint8_t b = packedPixel & 0xFF;

      rowBuffer[bufferIndex++] = b;
      rowBuffer[bufferIndex++] = g;
      rowBuffer[bufferIndex++] = r;
    }
    file.write(reinterpret_cast<char*>(rowBuffer.data()), rowSize);
  }
}

PPU::PPU(CPU6502& c, iNES& i): m_cpu(c), m_cartridge(i), m_cycle(0), m_scanline(0) {
  c.setPPU(this);
}

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
      data = m_reg_status;
      m_reg_status &= ~0x80;
      m_address_latch = 0;
      break;

    case 7:
      data = readInternal(m_vram_address);
      m_vram_address += (m_reg_ctrl & 0x04) ? 32 : 1;
      break;
  }
  return data;
}

uint8_t PPU::cpuWrite(uint16_t addr, uint8_t value) {
  switch ((addr - 0x2000) % 8) {
    case 0: m_reg_ctrl = value; break;
    case 1: m_reg_mask = value; break;

    case 6:
      if (m_address_latch == 0) {
        m_tram_address  = (m_tram_address & 0x00FF) | ((value & 0x3F) << 8);
        m_address_latch = 1;
      } else {
        m_tram_address  = (m_tram_address & 0xFF00) | value;
        m_vram_address  = m_tram_address;
        m_address_latch = 0;
      }
      break;

    case 7:
      writeInternal(m_vram_address, value);
      m_vram_address += (m_reg_ctrl & 0x04) ? 32 : 1;
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

      uint16_t nametableBase = 0x2000 + ((m_reg_ctrl & 0x03) * 0x0400);

      uint16_t tileAddress = nametableBase + (tileY * 32) + tileX;
      uint8_t  tileID      = readInternal(tileAddress);

      uint16_t patternBase = (m_reg_ctrl & 0x10) ? 0x1000 : 0x0000;

      uint16_t plane0Address = patternBase + (tileID * 16) + fineY;
      uint8_t  plane0Byte    = readInternal(plane0Address);
      uint8_t  plane1Byte    = readInternal(plane0Address + 8);

      uint8_t bitShift        = 7 - fineX;
      uint8_t pixelColorIndex = (((plane1Byte >> bitShift) & 0x01) << 1) | ((plane0Byte >> bitShift) & 0x01);

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

      uint8_t colorIndex = nesColorByte & 0x3F;

      RGB color = NES_SYSTEM_PALETTE[colorIndex];

      m_frame_buffer[(m_scanline * 256) + m_cycle] = 0xFF000000 | (color.r << 16) | (color.g << 8) | color.b;
    }
  } else if (m_scanline == 240) {
    // NOOP
  } else if (m_scanline >= 241 && m_scanline <= 260) {
    if (m_scanline == 241 && m_cycle == 1) {
      m_reg_status |= 0x80;

      if (m_reg_ctrl & 0x80) m_cpu.triggerNmi();
    }
  } else if (m_scanline == 261 && m_cycle == 1) {
    m_reg_status &= ~0x80;
  }
}

void PPU::step() {
  pixelEval();

  if (++m_cycle > 340) {
    m_cycle = 0;
    if (++m_scanline > 261) {
      m_scanline = 0;
      saveFramebufferToBmp(m_frame_buffer, "/tmp/test.bmp");
    }
  }
}

void PPU::run(uint8_t cycles) {
  for (uint8_t i = 0; i < cycles; ++i) {
    step();
  }
}
