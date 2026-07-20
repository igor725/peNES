#include "../ines.hh"
#include "mapper.hh"

#include <memory>

class MMC0: public Mapper {
  public:
  MMC0(iNES* c): Mapper(c) {}

  uint8_t cpuOperation(bool /* isWrite */, uint16_t addr, uint8_t /* value */) final { // We're ignoring writes here completely
    if (addr >= 0x8000 /* Start of PRG */) {
      addr &= 0x7FFF;
      if ((*m_cartridge)->hdr.getProgNum() == 1) addr %= iNES::PRG_BLOCK_SIZE; // Mirror PRG if only one bank available
      if ((*m_cartridge)->hdr.flags.trainer) addr += iNES::TRAINER_BLOCK_SIZE;
      return (*m_cartridge)->data[addr];
    }

    throw;
  }

  std::optional<uint32_t> resolvePPU(uint16_t addr) const final {
    if (addr <= 0x1FFF /* End of CHR */ && (*m_cartridge)->hdr.getCharNum() > 0) {
      addr += (*m_cartridge)->hdr.getProgNum() * iNES::PRG_BLOCK_SIZE;
      if ((*m_cartridge)->hdr.flags.trainer) addr += iNES::TRAINER_BLOCK_SIZE;
      return addr;
    }

    return {};
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {0x8000, 0xFFFF}; }
};

std::unique_ptr<Mapper> createMMC0(iNES* c) {
  return std::make_unique<MMC0>(c);
}
