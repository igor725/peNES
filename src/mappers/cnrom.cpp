#include "../ines.hh"
#include "mapper.hh"

#include <cstdint>
#include <memory>

class CNROM: public Mapper {
  public:
  CNROM(iNES* c): Mapper(c) { setBank(0); }

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (addr >= 0x6000 && addr <= 0x7FFF) return handleBattery(isWrite, addr & 0x1FFF, value);

    if (isWrite) {
      setBank(value);
      return value;
    }

    return m_cartridge->data[m_progBaseOff + (addr & (m_cartridge->hdr.getProgNum() == 1 ? (PROG_BANK_SIZE - 1) : 0x7FFF))];
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (isWrite || m_cartridge->hdr.getCharNum() == 0) throw;
    return m_cartridge->data[m_charBaseOff + m_chrOff + (addr & 0x1FFF)];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {m_cartridge->hdr.flags.battery ? 0x6000 : 0x8000, 0xFFFF}; }

  std::vector<uint8_t> dumpState() const final {
    auto dmp = prepareMapperDumper();

    dmp.push(m_chrOff);

    return dmp.extract();
  }

  void restoreState(std::vector<uint8_t>& state) final {
    auto rst = prepareMapperDumper(state);

    m_chrOff = rst.pop<decltype(m_chrOff)>();
  }

  private:
  uint32_t m_chrOff = 0;

  void setBank(uint8_t bnk) { m_chrOff = (bnk % (uint32_t)m_cartridge->hdr.getCharNum()) * 0x2000; }
};

std::unique_ptr<Mapper> createCNROM(iNES* c) {
  return std::make_unique<CNROM>(c);
}
