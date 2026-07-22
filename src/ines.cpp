#include "ines.hh"

#include "mappers/mapper.hh"

#include <string>

CartridgeException::CartridgeException(int32_t errorCode) {
  m_what = PlatImpl::getErrorString(errorCode);
}

CartridgeException::CartridgeException(Type type, int32_t additionalData) {
  switch (type) {
    case Type::IncompleteFile: m_what = "Incomplete or damaged dump"; break;
    case Type::ValidateFail: m_what = "Faulty or unsupported cartridge dump"; break;
    case Type::UnsupportedMapper: m_what = "Unsupported mapper: " + std::to_string(additionalData); break;
    case Type::UnsupportedSubMapper: m_what = "Unsupported submapper: " + std::to_string(additionalData); break;
    case Type::PALDump: m_what = "PAL cartridges are unsupported atm"; break;
  }
}

void iNES::performSetup() {
  if (m_mappedSize < sizeof(File::Header)) throw CartridgeException(CartridgeException::Type::IncompleteFile);

  m_file = static_cast<File*>(m_mappedData);

  if (m_validation) {
    if (!m_file->hdr.validate(m_mappedSize)) throw CartridgeException(CartridgeException::Type::ValidateFail);
    if (auto const r = m_file->hdr.getRegion(); r != File::Header::Region::NTSC && r != File::Header::Region::Multi)
      throw CartridgeException(CartridgeException::Type::PALDump);
  }

  switch (auto const m = m_file->hdr.getMapperId()) {
    case 0x0000: m_mapper = createMMC0(this); break;
    case 0x0001: m_mapper = createMMC1(this); break;
    case 0x0003: m_mapper = createCNROM(this); break;
    case 0x0004: m_mapper = createMMC3(this); break;

    default: throw CartridgeException(CartridgeException::Type::UnsupportedMapper, m);
  }
}

void iNES::insert(std::filesystem::path const& path) {
  try {
    unmapPlatform();
    mapPlatformFile(path);
    performSetup();
  } catch (int32_t errorCode) {
    throw CartridgeException(errorCode);
  }
}

void iNES::piped() {
  try {
    unmapPlatform();
    mapPlatformStdin();
    performSetup();
  } catch (int32_t errorCode) {
    throw CartridgeException(errorCode);
  }
}

iNES::~iNES() {
  unmapPlatform();
}
