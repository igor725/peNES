#include "../ines.hh"
#include "mapper.hh"

#include <cstdint>
#include <memory>

class MMC1: public Mapper {
  static constexpr size_t PROG_BANK_SIZE = 0x4000;
  static constexpr size_t CHAR_BANK_SIZE = 0x2000;

  public:
  MMC1(iNES* c): Mapper(c, PROG_BANK_SIZE, CHAR_BANK_SIZE) { updateOffsets(); }

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (addr >= 0x6000 && addr <= 0x7FFF) return handleBattery(isWrite, addr & 0x1FFF, value);

    if (isWrite) {
      if (addr >= 0x8000) {
        if (value & 0x80) {
          m_shiftReg   = 0;
          m_shiftCount = 0;
          m_ctlReg |= 0x0C;
          updateOffsets();
        } else {
          m_shiftReg |= ((value & 1) << m_shiftCount);
          m_shiftCount++;

          if (m_shiftCount == 5) {
            uint8_t target = (addr >> 13) & 0x03;
            switch (target) {
              case 0: m_ctlReg = m_shiftReg; break;
              case 1: m_charBank0 = m_shiftReg; break;
              case 2: m_charBank1 = m_shiftReg; break;
              case 3: m_prgBank = m_shiftReg; break;
            }
            m_shiftReg   = 0;
            m_shiftCount = 0;
            updateOffsets();
          }
        }

        return value;
      }

      throw;
    }

    if (addr >= 0x8000 && addr <= 0xBFFF) {
      return m_cartridge->data[m_progBaseOff + m_prgOff0 + (addr & 0x3FFF)];
    } else if (addr >= 0xC000 && addr <= 0xFFFF) {
      return m_cartridge->data[m_progBaseOff + m_prgOff1 + (addr & 0x3FFF)];
    }

    throw;
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (m_cartridge->hdr.getCharNum() == 0) {
      auto const chrRam = prepareCHRMemory(0x2000);
      if (isWrite) return chrRam[addr & 0x1FFF] = value;
      return chrRam[addr & 0x1FFF];
    }

    if (isWrite) throw;
    return m_cartridge->data[m_charBaseOff + m_chrOff[(addr >> 12) & 0x1] + (addr & 0x0FFF)];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {m_cartridge->hdr.flags.battery ? 0x6000 : 0x8000, 0xFFFF}; }

  private:
  uint8_t m_shiftReg   = 0;
  uint8_t m_shiftCount = 0;

  uint8_t m_ctlReg    = 0x0C;
  uint8_t m_charBank0 = 0;
  uint8_t m_charBank1 = 0;
  uint8_t m_prgBank   = 0;

  uint32_t                m_prgOff0 = 0;
  uint32_t                m_prgOff1 = 0;
  std::array<uint32_t, 2> m_chrOff  = {0x0000, 0x1000};

  uint8_t m_memory[8192];

  void updateOffsets() {
    if (((m_ctlReg >> 4) & 0x01) /* chrMode */ == 0) {
      uint8_t bank = m_charBank0 & 0xFE;
      m_chrOff[0]  = bank * 0x1000;
      m_chrOff[1]  = (bank + 1) * 0x1000;
    } else {
      m_chrOff[0] = m_charBank0 * 0x1000;
      m_chrOff[1] = m_charBank1 * 0x1000;
    }

    uint8_t currBank = m_prgBank & 0x0F;

    switch ((m_ctlReg >> 2) & 0x03) {
      case 0x00:
      case 0x01: {
        m_prgOff0 = (currBank & 0xFE) * 0x4000;
        m_prgOff1 = ((currBank & 0xFE) + 1) * 0x4000;
      } break;
      case 0x02: {
        m_prgOff0 = 0;
        m_prgOff1 = currBank * 0x4000;
      } break;
      case 0x03: {
        m_prgOff0 = currBank * 0x4000;
        m_prgOff1 = (m_cartridge->hdr.getProgNum() - 1) * 0x4000;
      } break;
    }
  }
};

std::unique_ptr<Mapper> createMMC1(iNES* c) {
  return std::make_unique<MMC1>(c);
}
