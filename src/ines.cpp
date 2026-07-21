#include "ines.hh"

#include "mappers/mapper.hh"

#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

CartridgeException::CartridgeException(int32_t errorCode) {
  m_what = ::strerror(errorCode);
}

CartridgeException::CartridgeException(Type type, int32_t additionalData) {
  switch (type) {
    case Type::IncompleteFile: m_what = "Incomplete or damaged dump"; break;
    case Type::ValidateFail: m_what = "Faulty or unsupported cartridge dump"; break;
    case Type::UnsupportedMapper: m_what = "Unsupported mapper: " + std::to_string(additionalData); break;
    case Type::PALDump: m_what = "PAL cartridges are unsupported atm"; break;
  }
}

void iNES::insert(std::filesystem::path const& path) {
  int32_t file = ::open(path.string().c_str(), O_RDONLY);
  if (file < 0) throw CartridgeException(errno);

  struct stat sb;
  if (::fstat(file, &sb) == -1) {
    auto const ecode = errno;
    ::close(file);
    throw CartridgeException(ecode);
  }

  if (sb.st_size < sizeof(File)) throw CartridgeException(CartridgeException::Type::IncompleteFile);

  if (m_file != nullptr) {
    ::munmap(m_file, m_size);
    m_file = nullptr, m_size = 0;
  }

  m_file = static_cast<File*>(::mmap(nullptr, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file, 0));
  ::close(file);

  if (m_file == MAP_FAILED) {
    auto const ecode = errno;
    throw CartridgeException(ecode);
  }
  m_size = sb.st_size;

  if (m_validation) {
    if (!m_file->hdr.validate(sb.st_size)) throw CartridgeException(CartridgeException::Type::ValidateFail);
    if (auto const r = m_file->hdr.getRegion(); r != File::Header::Region::NTSC && r != File::Header::Region::Multi)
      throw CartridgeException(CartridgeException::Type::PALDump);
  }

  switch (auto m = m_file->hdr.getMapperId()) {
    case 0x0000: m_mapper = createMMC0(this); break;

    case 0x0001:
    case 0x0069:
    case 0x009b: m_mapper = createMMC1(this); break;

    case 0x0003:
    case 0x00b9: m_mapper = createCNROM(this); break;

    case 0x0004: m_mapper = createMMC3(this); break;

    default: throw CartridgeException(CartridgeException::Type::UnsupportedMapper, m);
  }
}

iNES::~iNES() {
  ::munmap(m_file, m_size);
}
