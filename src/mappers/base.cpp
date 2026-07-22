#include "../ines.hh"
#include "dumper.hh"
#include "mapper.hh"

Mapper::Mapper(iNES* c, bool subMappersAware): m_cartridge(*c) {
  if (auto const sm = m_cartridge->hdr.getSubMapperId(); sm != 0 && !subMappersAware)
    throw CartridgeException(CartridgeException::Type::UnsupportedSubMapper, sm);
  if (m_cartridge->hdr.flags.trainer) m_progBaseOff = TRAINER_BLOCK_SIZE;
  m_charBaseOff = m_progBaseOff + (m_cartridge->hdr.getProgNum() * PROG_BANK_SIZE);
  if (m_charBaseOff + (m_cartridge->hdr.getCharNum() * CHAR_BANK_SIZE) > m_cartridge.getFileSize())
    throw CartridgeException(CartridgeException::Type::IncompleteFile);
}

std::span<uint8_t> Mapper::prepareCHRMemory(size_t size) {
  if (m_chrRam.size() != size) m_chrRam.resize(size);
  return m_chrRam;
}

std::span<uint8_t> Mapper::preparePRGMemory(size_t size) {
  if (m_prgRam.size() != size) m_prgRam.resize(size);
  return m_prgRam;
}

uint8_t Mapper::handleBattery(bool isWrite, uint16_t addr, uint8_t value) {
  auto const memory = preparePRGMemory(m_cartridge->hdr.getPrgRamSize());
  if (isWrite) return memory[addr] = value;
  return memory[addr];
}

MapperDumper Mapper::prepareMapperDumper() const {
  MapperDumper dump;

  dump.push(m_chrRam);
  dump.push(m_prgRam);

  return dump;
}
