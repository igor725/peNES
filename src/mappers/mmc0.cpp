#include "../ines.hh"
#include "mapper.hh"

#include <memory>

class MMC0: public Mapper {
  static constexpr size_t PROG_BANK_SIZE = 0x4000;
  static constexpr size_t CHAR_BANK_SIZE = 0x2000;

  public:
  MMC0(iNES* c): Mapper(c, PROG_BANK_SIZE, CHAR_BANK_SIZE) {}

  uint8_t cpuOperation(bool /* isWrite */, uint16_t addr, uint8_t /* value */) final { // We're ignoring writes here completely
    if (addr >= 0x8000 /* Start of PRG */) {
      addr &= m_cartridge->hdr.getProgNum() == 1 ? (PROG_BANK_SIZE - 1) : 0x7FFF;
      return m_cartridge->data[m_progBaseOff + addr];
    }

    throw;
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    addr &= 0x1FFF;
    if (m_cartridge->hdr.getCharNum() == 0) {
      auto const chrRam = prepareCHRMemory(0x2000);
      if (isWrite) return chrRam[addr] = value;
      return chrRam[addr];
    }

    if (isWrite) throw;
    return m_cartridge->data[m_charBaseOff + addr];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {0x8000, 0xFFFF}; }
};

std::unique_ptr<Mapper> createMMC0(iNES* c) {
  return std::make_unique<MMC0>(c);
}
