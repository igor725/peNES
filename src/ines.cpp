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

  size_t max_size = sb.st_size + 0x2000;

  void* total_alloc = ::mmap(nullptr, max_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (total_alloc == MAP_FAILED) {
    ::close(file);
    throw 3;
  }

  void* file_map = ::mmap(total_alloc, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, file, 0);
  if (file_map == MAP_FAILED) {
    ::munmap(total_alloc, max_size);
    ::close(file);
    throw 3;
  }

  m_file = static_cast<File*>(total_alloc);

  if (!m_file->validate(sb.st_size)) {
    ::munmap(m_file, max_size);
    ::close(file);
    throw 4;
  }

  ::close(file);
}

iNES::~iNES() {}
