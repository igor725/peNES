#include "ines.hh"

#include "mappers/mapper.hh"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

CartridgeException::CartridgeException(int32_t errorCode) {
  m_what = ::strerror(errorCode);
}

CartridgeException::CartridgeException(Type type) {
  switch (type) {
    case Type::ValidateFail: m_what = "Failed to validate cartridge dump"; break;
    case Type::UnsupportedMapper: m_what = "Unsupported mapper"; break;
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

  if (m_file != nullptr) {
    ::munmap(m_file, m_size);
    m_file = nullptr, m_size = 0;
  }

  m_file = static_cast<File*>(::mmap(nullptr, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file, 0));
  if (m_file == MAP_FAILED) {
    auto const ecode = errno;
    ::close(file);
    throw CartridgeException(ecode);
  }

  if (!m_file->validate(sb.st_size)) {
    ::munmap(m_file, sb.st_size);
    ::close(file);
    throw CartridgeException(CartridgeException::Type::ValidateFail);
  }

  m_size = sb.st_size;
  ::close(file);

  switch (m_file->getMapperId()) {
    case 0x00: m_mapper = createMMC0(this); break;

    case 0x01:
    case 0x69:
    case 0x9b: m_mapper = createMMC1(this); break;

    default: throw CartridgeException(CartridgeException::Type::UnsupportedMapper);
  }
}

iNES::~iNES() {
  ::munmap(m_file, m_size);
}
