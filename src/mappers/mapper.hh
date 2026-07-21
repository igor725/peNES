#pragma once

#include "dumper.hh"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class iNES;

class Mapper {
  public:
  Mapper(iNES* c, uint16_t progBankSize, uint16_t charBankSize);

  virtual ~Mapper() = default;

  virtual uint8_t                       cpuOperation(bool isWrite, uint16_t addr, uint8_t value) = 0;
  virtual uint8_t                       ppuOperation(bool isWrite, uint16_t addr, uint8_t value) = 0;
  virtual std::pair<uint16_t, uint16_t> getMappedRegion() const                                  = 0;

  protected:
  MapperDumper       prepareMapperDumper() const;
  std::span<uint8_t> prepareCHRMemory(size_t size);
  std::span<uint8_t> preparePRGMemory(size_t size);
  uint8_t            handleBattery(bool isWrite, uint16_t addr, uint8_t value);

  iNES const& m_cartridge;

  std::vector<uint8_t> m_chrRam = {};
  std::vector<uint8_t> m_prgRam = {};

  uint32_t m_progBaseOff = 0;
  uint32_t m_charBaseOff = 0;
};

std::unique_ptr<Mapper> createMMC0(iNES* c);
std::unique_ptr<Mapper> createMMC1(iNES* c);
std::unique_ptr<Mapper> createMMC3(iNES* c);
std::unique_ptr<Mapper> createCNROM(iNES* c);
