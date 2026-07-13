#include "ines.hh"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

iNES::iNES(const char* filename) {
  int32_t file = ::open(filename, O_RDONLY);
  if (file < 0) throw 1;
  struct stat sb;
  if (::fstat(file, &sb) == -1) throw 2;

  m_file = static_cast<File const*>(::mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file, 0));
  if (m_file == nullptr) throw 3;
  if (!m_file->validate(sb.st_size)) throw 4;

  ::close(file);
}

iNES::~iNES() {}

uint8_t iNES::readChr(uint16_t addr) const {
  auto it = a.find(addr);
  if (it == a.end()) return 0;
  return it->second;
}

void iNES::writeChr(uint16_t addr, uint8_t value) {
  a[addr] = value;
}
