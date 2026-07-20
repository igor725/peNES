#include "../ines.hh"
#include "mapper.hh"

Mapper::Mapper(iNES* c, uint16_t progBankSize, uint16_t charBankSize): m_cartridge(*c) {
  if (m_cartridge->hdr.flags.trainer) m_progBaseOff = iNES::TRAINER_BLOCK_SIZE;
  m_charBaseOff = m_progBaseOff + (m_cartridge->hdr.getProgNum() * progBankSize);
  if (m_charBaseOff + (m_cartridge->hdr.getCharNum() * charBankSize) > m_cartridge.getFileSize())
    throw CartridgeException(CartridgeException::Type::IncompleteFile);
}

std::span<uint8_t> Mapper::prepareCHRMemory(size_t size) {
  if (m_chrRam == nullptr)
    m_chrRam = std::make_unique<std::vector<uint8_t>>(size, 0);
  else if (m_chrRam->size() != size)
    m_chrRam->resize(size);
  return *m_chrRam;
}

std::span<uint8_t> Mapper::preparePRGMemory(size_t size) {
  if (m_prgRam == nullptr)
    m_prgRam = std::make_unique<std::vector<uint8_t>>(size, 0);
  else if (m_prgRam->size() != size)
    m_prgRam->resize(size);
  return *m_prgRam;
}

uint8_t Mapper::handleBattery(bool isWrite, uint16_t addr, uint8_t value) {
  auto const memory = preparePRGMemory(0x2000);
  if (isWrite) return memory[addr] = value;
  return memory[addr];
}
