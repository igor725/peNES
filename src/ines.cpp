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
    case Type::UnsupportedSubMapper: m_what = "Unsupported submapper: " + std::to_string(additionalData); break;
    case Type::PALDump: m_what = "PAL cartridges are unsupported atm"; break;
  }
}

void iNES::performSetup(int32_t fd) {
  struct stat sb;
  if (::fstat(fd, &sb) == -1) {
    auto const ecode = errno;
    ::close(fd);
    throw CartridgeException(ecode);
  }

  void*  region;
  size_t currSize;

  if (S_ISREG(sb.st_mode)) {
    region = ::mmap(nullptr, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (region == MAP_FAILED) throw CartridgeException(errno);
    currSize = sb.st_size;
    ::close(fd);
  } else {
    std::vector<uint8_t> holder;
    holder.reserve(256 * 1024);

    char buf[16 * 1024];

    while (auto const rd = ::fread((void*)&buf, 1, sizeof(buf), stdin)) {
      holder.insert(holder.end(), buf, buf + rd);
    }

    currSize = holder.size();
    region   = ::mmap(nullptr, currSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    std::memcpy(region, holder.data(), holder.size());
  }

  performSetup(region, currSize);
}

void iNES::performSetup(void* file, size_t size) {
  if (size < sizeof(File::Header)) throw CartridgeException(CartridgeException::Type::IncompleteFile);

  if (m_file != nullptr) {
    ::munmap(m_file, m_size);
    m_file = nullptr, m_size = 0;
  }

  m_file = static_cast<File*>(file);
  m_size = size;

  if (m_validation) {
    if (!m_file->hdr.validate(size)) throw CartridgeException(CartridgeException::Type::ValidateFail);
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
  int32_t const file = ::open(path.string().c_str(), O_RDONLY);
  if (file < 0) throw CartridgeException(errno);
  performSetup(file);
}

void iNES::piped() {
  performSetup(0);
}

iNES::~iNES() {
  ::munmap(m_file, m_size);
}
