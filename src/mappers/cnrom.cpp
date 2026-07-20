#include "../ines.hh"
#include "mapper.hh"

#include <cstdint>
#include <memory>

class CNROM: public Mapper {
  public:
  CNROM(iNES* c): Mapper(c) {
    if ((*c)->hdr.flags.trainer) m_progBaseOff = iNES::TRAINER_BLOCK_SIZE;

    m_charBaseOff = m_progBaseOff + ((*c)->hdr.getProgNum() * iNES::PRG_BLOCK_SIZE);
    setBank(0);
  }

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
      addr -= 0x6000;
      if (isWrite) return m_memory[addr] = value;
      return m_memory[addr];
    }

    if (isWrite) {
      if (addr >= 0x8000) {
        setBank(value);
        return value;
      }

      throw;
    }

    if (addr >= 0x8000 && addr <= 0xFFFF) {
      uint32_t prgSize = (*m_cartridge)->hdr.getProgNum() * iNES::PRG_BLOCK_SIZE;

      if (prgSize == 16384) {
        return (*m_cartridge)->data[m_progBaseOff + (addr & 0x3FFF)];
      } else {
        return (*m_cartridge)->data[m_progBaseOff + (addr - 0x8000)];
      }
    }

    throw;
  }

  std::optional<uint32_t> resolvePPU(uint16_t addr) const final {
    if ((*m_cartridge)->hdr.getCharNum() == 0) return {};

    if (addr <= 0x1FFF) {
      return m_charBaseOff + m_chrOff + (addr & 0x1FFF);
    }

    return {};
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {(*m_cartridge)->hdr.flags.battery ? 0x6000 : 0x8000, 0xFFFF}; }

  private:
  uint32_t m_progBaseOff = 0;
  uint32_t m_charBaseOff = 0;
  uint8_t  m_chrBank     = 0;
  uint32_t m_chrOff      = 0;

  uint8_t m_memory[8192];

  void setBank(uint8_t bnk) {
    m_chrOff  = (bnk % (uint32_t)(*m_cartridge)->hdr.getCharNum()) * 0x2000;
    m_chrBank = bnk;
  }
};

std::unique_ptr<Mapper> createCNROM(iNES* c) {
  return std::make_unique<CNROM>(c);
}
