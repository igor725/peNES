#include "../ines.hh"
#include "mapper.hh"

#include <array>
#include <cstdint>
#include <memory>

class MMC3: public Mapper {
  static constexpr size_t MMC3_PROG_BANK_SIZE = 0x2000;
  static constexpr size_t MMC3_CHAR_BANK_SIZE = 0x2000;

  public:
  MMC3(iNES* c): Mapper(c) { updateOffsets(); }

  uint8_t cpuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (addr >= 0x6000 && addr <= 0x7FFF) return handleBattery(isWrite, addr & 0x1FFF, value);

    auto const bankNumber = (addr & 0x7FFF) / MMC3_PROG_BANK_SIZE;
    auto const bankOffset = (addr & 0x7FFF) % MMC3_PROG_BANK_SIZE;

    if (isWrite) {
      bool const isEven = (addr % 2) == 0;

      if (addr <= 0x9FFF) {
        if (isEven) {
          m_targetRegister = value & 0x07;
          m_prgMode        = (value >> 6) & 0x01;
          m_chrMode        = (value >> 7) & 0x01;
        } else {
          m_registers[m_targetRegister] = value;
        }
        updateOffsets();
      } else if (addr >= 0xC000) {
        if (addr <= 0xDFFF) {
          if (isEven) {
            m_irqLatch = value;
          } else {
            m_irqReload = true;
          }
        } else {
          m_irqEnable = !isEven;
        }
      } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (isEven) /* Swap mirroring */ {
          m_cartridge->hdr.flags.mirroring = !value;
        } else {
          // ???
        }
      }

      return value;
    }

    return m_cartridge->data[m_progBanks[bankNumber] + bankOffset];
  }

  uint8_t ppuOperation(bool isWrite, uint16_t addr, uint8_t value) final {
    if (m_cartridge->hdr.getCharNum() == 0) {
      auto const chrRam = prepareCHRMemory(m_cartridge->hdr.getCharRamSize());
      if (isWrite) return chrRam[addr & 0x1FFF] = value;
      return chrRam[addr & 0x1FFF];
    }

    uint16_t const bankNumber = addr / 0x0400;
    uint16_t const bankOffset = addr % 0x0400;

    return m_cartridge->data[m_charBaseOff + m_chrBanks[bankNumber] + bankOffset];
  }

  std::pair<uint16_t, uint16_t> getMappedRegion() const final { return {m_cartridge->hdr.flags.battery ? 0x6000 : 0x8000, 0xFFFF}; }

  bool nextScanline() final {
    if (m_irqCounter == 0 || m_irqReload) {
      m_irqCounter = m_irqLatch;
      m_irqReload  = false;
    } else {
      m_irqCounter--;
    }

    return m_irqCounter == 0 && m_irqEnable;
  }

  std::vector<uint8_t> dumpState() const final {
    auto dmp = prepareMapperDumper();

    return dmp.extract();
  }

  void restoreState(std::vector<uint8_t>& state) final {
    auto rst = prepareMapperDumper(state);

    m_irqLatch   = rst.pop<decltype(m_irqLatch)>();
    m_irqCounter = rst.pop<decltype(m_irqCounter)>();
    m_irqEnable  = rst.pop<decltype(m_irqEnable)>();
    m_irqReload  = rst.pop<decltype(m_irqReload)>();

    m_targetRegister = rst.pop<decltype(m_targetRegister)>();
    m_prgMode        = rst.pop<decltype(m_prgMode)>();
    m_chrMode        = rst.pop<decltype(m_chrMode)>();
    m_registers      = rst.pop<decltype(m_registers)>();

    m_progBanks = rst.pop<decltype(m_progBanks)>();
    m_chrBanks  = rst.pop<decltype(m_chrBanks)>();
  }

  private:
  uint8_t m_irqLatch   = 0;
  uint8_t m_irqCounter = 0;
  bool    m_irqEnable  = false;
  bool    m_irqReload  = false;

  uint8_t                m_targetRegister = 0;
  uint8_t                m_prgMode        = 0;
  uint8_t                m_chrMode        = 0;
  std::array<uint8_t, 8> m_registers      = {0, 0, 0, 0, 0, 0, 0, 0};

  std::array<uint32_t, 4> m_progBanks = {0, 0, 0, 0};
  std::array<uint32_t, 8> m_chrBanks  = {0, 0, 0, 0, 0, 0, 0, 0};

  void updateOffsets() {
    uint32_t prgBanksTotal  = m_cartridge->hdr.getProgNum() * 2;
    uint32_t chrBlocksTotal = m_cartridge->hdr.getCharNum() * 8;
    if (chrBlocksTotal == 0) chrBlocksTotal = 8;

    uint32_t prgLast       = (prgBanksTotal > 0) ? (prgBanksTotal - 1) * MMC3_PROG_BANK_SIZE : 0;
    uint32_t prgSecondLast = (prgBanksTotal > 1) ? (prgBanksTotal - 2) * MMC3_PROG_BANK_SIZE : 0;

    uint32_t r6 = (prgBanksTotal > 0) ? (m_registers[6] % prgBanksTotal) * MMC3_PROG_BANK_SIZE : 0;
    uint32_t r7 = (prgBanksTotal > 0) ? (m_registers[7] % prgBanksTotal) * MMC3_PROG_BANK_SIZE : 0;

    if (m_prgMode == 0) {
      m_progBanks[0] = m_progBaseOff + r6;
      m_progBanks[1] = m_progBaseOff + r7;
      m_progBanks[2] = m_progBaseOff + prgSecondLast;
      m_progBanks[3] = m_progBaseOff + prgLast;
    } else {
      m_progBanks[0] = m_progBaseOff + prgSecondLast;
      m_progBanks[1] = m_progBaseOff + r7;
      m_progBanks[2] = m_progBaseOff + r6;
      m_progBanks[3] = m_progBaseOff + prgLast;
    }

    uint32_t r0 = ((m_registers[0] & 0xFE) % chrBlocksTotal) * 0x0400;
    uint32_t r1 = ((m_registers[1] & 0xFE) % chrBlocksTotal) * 0x0400;
    uint32_t r2 = (m_registers[2] % chrBlocksTotal) * 0x0400;
    uint32_t r3 = (m_registers[3] % chrBlocksTotal) * 0x0400;
    uint32_t r4 = (m_registers[4] % chrBlocksTotal) * 0x0400;
    uint32_t r5 = (m_registers[5] % chrBlocksTotal) * 0x0400;

    if (m_chrMode == 0) {
      m_chrBanks[0] = r0;
      m_chrBanks[1] = r0 + 0x0400;
      m_chrBanks[2] = r1;
      m_chrBanks[3] = r1 + 0x0400;
      m_chrBanks[4] = r2;
      m_chrBanks[5] = r3;
      m_chrBanks[6] = r4;
      m_chrBanks[7] = r5;
    } else {
      m_chrBanks[0] = r2;
      m_chrBanks[1] = r3;
      m_chrBanks[2] = r4;
      m_chrBanks[3] = r5;
      m_chrBanks[4] = r0;
      m_chrBanks[5] = r0 + 0x0400;
      m_chrBanks[6] = r1;
      m_chrBanks[7] = r1 + 0x0400;
    }
  }
};

std::unique_ptr<Mapper> createMMC3(iNES* c) {
  return std::make_unique<MMC3>(c);
}
