#include "../ines.hh"
#include "mapper.hh"

#include <memory>

class MMC0: public Mapper {
  public:
  MMC0(iNES* c): Mapper(c) {}

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final { // We're ignoring writes here completely
    auto const romAddr = m_cartridge->resolveCPU(addr);
    return (*m_cartridge)->data[romAddr];
  }

  uint32_t resolvePPU(uint16_t addr) const final { return (*m_cartridge).resolvePPU(addr); }
};

std::unique_ptr<Mapper> createMMC0(iNES* c) {
  return std::make_unique<MMC0>(c);
}
