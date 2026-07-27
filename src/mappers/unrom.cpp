#include "../ines.hh"
#include "mapper.hh"

#include <array>
#include <cstdint>
#include <memory>

class UNROM: public Mapper {
  public:
  UNROM(iNES* c): Mapper(c) { updateOffsets(); }

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (addr >= 0x6000 && addr <= 0x7FFF) return handleBattery(isWrite, addr & 0x1FFF, value);

    if (isWrite) {
      m_bankSelect = value;
      updateOffsets();
      return value;
    }

    return m_cartridge->data[m_progBanks[(addr >= 0xC000) ? 1 : 0] + (addr & 0x3FFF)];
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (m_cartridge->hdr.getCharNum() == 0) {
      auto const chrRam = prepareCHRMemory(m_cartridge->hdr.getCharRamSize());
      if (isWrite) return chrRam[addr & 0x1FFF] = value;
      return chrRam[addr & 0x1FFF];
    }

    return m_cartridge->data[m_charBaseOff + (addr & 0x1FFF)];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {0x6000, 0xFFFF}; }

  std::vector<uint8_t> dumpState() const final {
    auto dmp = prepareMapperDumper();

    dmp.push(m_bankSelect);
    dmp.push(m_progBanks);

    return dmp.extract();
  }

  void restoreState(std::vector<uint8_t>& state) final {
    auto rst = prepareMapperDumper(state);

    m_bankSelect = rst.pop<decltype(m_bankSelect)>();
    m_progBanks  = rst.pop<decltype(m_progBanks)>();
  }

  private:
  uint8_t                 m_bankSelect = 0;
  std::array<uint32_t, 2> m_progBanks  = {0, 0};

  void updateOffsets() {
    uint32_t const prgBanksTotal = m_cartridge->hdr.getProgNum();

    m_progBanks[0] = m_progBaseOff + ((m_bankSelect & 0x07) % prgBanksTotal) * PROG_BANK_SIZE;
    if (m_progBanks[1] == 0) m_progBanks[1] = m_progBaseOff + ((prgBanksTotal - 1) * PROG_BANK_SIZE);
  }
};

std::unique_ptr<Mapper> createUNROM(iNES* c) {
  return std::make_unique<UNROM>(c);
}
