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

  m_file = static_cast<File*>(::mmap(nullptr, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file, 0));
  if (m_file == MAP_FAILED) {
    ::close(file);
    throw;
  }

  if (!m_file->validate(sb.st_size)) {
    ::munmap(m_file, sb.st_size);
    ::close(file);
    throw;
  }

  ::close(file);
}

iNES::~iNES() {}
