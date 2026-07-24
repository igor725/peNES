#include "../ines.hh"
#include "mapper.hh"

#include <cstdint>
#include <memory>

class MMC1: public Mapper {
  public:
  MMC1(iNES* c): Mapper(c) { updateOffsets(); }

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (addr >= 0x6000 && addr <= 0x7FFF) return handleBattery(isWrite, addr & 0x1FFF, value);

    if (isWrite) {
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

    return m_cartridge->data[m_progBaseOff + m_prgOff[(addr & 0x7FFF) / PROG_BANK_SIZE] + (addr & 0x3FFF)];
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (m_cartridge->hdr.getCharNum() == 0) {
      auto const chrRam = prepareCHRMemory(m_cartridge->hdr.getCharRamSize());
      if (isWrite) return chrRam[addr & 0x1FFF] = value;
      return chrRam[addr & 0x1FFF];
    }

    return m_cartridge->data[m_charBaseOff + m_chrOff[(addr >> 12) & 0x1] + (addr & 0x0FFF)];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {m_cartridge->hdr.flags.battery ? 0x6000 : 0x8000, 0xFFFF}; }

  std::vector<uint8_t> dumpState() const final {
    auto dmp = prepareMapperDumper();

    dmp.push(m_shiftReg);
    dmp.push(m_shiftCount);
    dmp.push(m_ctlReg);
    dmp.push(m_charBank0);
    dmp.push(m_charBank1);
    dmp.push(m_prgBank);
    dmp.push(m_prgOff);
    dmp.push(m_chrOff);

    return dmp.extract();
  }

  void restoreState(std::vector<uint8_t>& state) final {
    auto rst = prepareMapperDumper(state);

    m_shiftReg   = rst.pop<decltype(m_shiftReg)>();
    m_shiftCount = rst.pop<decltype(m_shiftCount)>();
    m_ctlReg     = rst.pop<decltype(m_ctlReg)>();
    m_charBank0  = rst.pop<decltype(m_charBank0)>();
    m_charBank1  = rst.pop<decltype(m_charBank1)>();
    m_prgBank    = rst.pop<decltype(m_prgBank)>();
    m_prgOff     = rst.pop<decltype(m_prgOff)>();
    m_chrOff     = rst.pop<decltype(m_chrOff)>();
  }

  private:
  uint8_t m_shiftReg   = 0;
  uint8_t m_shiftCount = 0;

  uint8_t m_ctlReg    = 0x0C;
  uint8_t m_charBank0 = 0;
  uint8_t m_charBank1 = 0;
  uint8_t m_prgBank   = 0;

  std::array<uint32_t, 2> m_prgOff = {0x0000, 0x0000};
  std::array<uint32_t, 2> m_chrOff = {0x0000, 0x1000};

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
        m_prgOff[0] = (currBank & 0xFE) * 0x4000;
        m_prgOff[1] = ((currBank & 0xFE) + 1) * 0x4000;
      } break;
      case 0x02: {
        m_prgOff[0] = 0;
        m_prgOff[1] = currBank * 0x4000;
      } break;
      case 0x03: {
        m_prgOff[0] = currBank * 0x4000;
        m_prgOff[1] = (m_cartridge->hdr.getProgNum() - 1) * 0x4000;
      } break;
    }

    switch (m_ctlReg & 0x03) {
      case 0x00: {
        m_mirroring = PPU::MirroringMode::OneScreenLow;
      } break;
      case 0x01: {
        m_mirroring = PPU::MirroringMode::OneScreenUp;
      } break;
      case 0x02: {
        m_mirroring = PPU::MirroringMode::Vertical;
      } break;
      case 0x03: {
        m_mirroring = PPU::MirroringMode::Horizontal;
      } break;
    }
  }
};

std::unique_ptr<Mapper> createMMC1(iNES* c) {
  return std::make_unique<MMC1>(c);
}
