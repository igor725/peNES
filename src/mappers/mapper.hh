#pragma once

#include "../ppu.hh"
#include "dumper.hh"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class iNES;

class Mapper {
  public:
  static constexpr size_t TRAINER_BLOCK_SIZE = 0x200;
  static constexpr size_t PROG_BANK_SIZE     = 0x4000;
  static constexpr size_t CHAR_BANK_SIZE     = 0x2000;

  Mapper(iNES* c, bool subMappersAware = false);

  virtual ~Mapper() = default;

  virtual uint8_t                       cpuOperation(bool isWrite, uint16_t addr, uint8_t value) = 0;
  virtual uint8_t                       ppuOperation(bool isWrite, uint16_t addr, uint8_t value) = 0;
  virtual std::pair<uint16_t, uint16_t> getMappedRegion() const                                  = 0;
  virtual std::vector<uint8_t>          dumpState() const                                        = 0;
  virtual void                          restoreState(std::vector<uint8_t>& state)                = 0;

  virtual bool nextScanline() { return false; };

  PPU::MirroringMode getMirroringMode() const;

  protected:
  MapperDumper       prepareMapperDumper() const;
  MapperDumper       prepareMapperDumper(std::vector<uint8_t>& state);
  std::span<uint8_t> prepareCHRMemory(size_t size);
  std::span<uint8_t> preparePRGMemory(size_t size);
  uint8_t            handleBattery(bool isWrite, uint16_t addr, uint8_t value);

  iNES const& m_cartridge;

  std::vector<uint8_t>              m_chrRam    = {};
  std::vector<uint8_t>              m_prgRam    = {};
  std::optional<PPU::MirroringMode> m_mirroring = {};

  uint32_t m_progBaseOff = 0;
  uint32_t m_charBaseOff = 0;
};

std::unique_ptr<Mapper> createMMC0(iNES* c);
std::unique_ptr<Mapper> createMMC1(iNES* c);
std::unique_ptr<Mapper> createMMC3(iNES* c);
std::unique_ptr<Mapper> createCNROM(iNES* c);
