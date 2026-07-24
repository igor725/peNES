#include "../ines.hh"
#include "mapper.hh"

#include <memory>

class MMC0: public Mapper {
  public:
  MMC0(iNES* c): Mapper(c) {}

  uint8_t cpuOperation(bool /* isWrite */, uint16_t addr, uint8_t /* value */) final { // We're ignoring writes here completely
    return m_cartridge->data[m_progBaseOff + (addr & (m_cartridge->hdr.getProgNum() == 1 ? (PROG_BANK_SIZE - 1) : 0x7FFF))];
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    addr &= 0x1FFF;
    if (m_cartridge->hdr.getCharNum() == 0) {
      auto const chrRam = prepareCHRMemory(m_cartridge->hdr.getCharRamSize());
      if (isWrite) return chrRam[addr] = value;
      return chrRam[addr];
    }

    return m_cartridge->data[m_charBaseOff + addr];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {0x8000, 0xFFFF}; }

  std::vector<uint8_t> dumpState() const final { return prepareMapperDumper().extract(); }

  void restoreState(std::vector<uint8_t>& state) final { prepareMapperDumper(state); }
};

std::unique_ptr<Mapper> createMMC0(iNES* c) {
  return std::make_unique<MMC0>(c);
}
