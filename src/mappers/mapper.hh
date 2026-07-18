#pragma once

#include <cstdint>
#include <memory>
#include <optional>

class iNES;

class Mapper {
  public:
  Mapper(iNES* c): m_cartridge(c) {}

  virtual ~Mapper() = default;

  virtual uint8_t                       cpuOperation(bool isWrite, uint16_t addr, uint8_t value) = 0;
  virtual std::optional<uint32_t>       resolvePPU(uint16_t addr) const                          = 0;
  virtual std::pair<uint16_t, uint16_t> getMappedRegion() const                                  = 0;

  protected:
  iNES* const m_cartridge;
};

std::unique_ptr<Mapper> createMMC0(iNES* c);
std::unique_ptr<Mapper> createMMC1(iNES* c);
