#include "ines.hh"

#include "mappers/mapper.hh"

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

  m_size = sb.st_size;
  ::close(file);

  switch (m_file->getMapperId()) {
    case 0x00: m_mapper = createMMC0(this); break;

    case 0x01:
    case 0x69:
    case 0x9b: m_mapper = createMMC1(this); break;

    default: throw;
  }
}

iNES::~iNES() {
  ::munmap(m_file, m_size);
}

uint16_t iNES::resolveCPU(uint16_t addr) const {
  if (addr >= 0x8000 /* Start of PRG */) {
    addr &= 0x7FFF;
    if (m_file->hdr.progSize == 1) addr %= PRG_BLOCK_SIZE; // Mirror PRG if only one bank available
    if (m_file->hdr.flags6.trainer) addr += TRAINER_BLOCK_SIZE;
    return addr;
  }

  throw;
}

uint16_t iNES::resolvePPU(uint16_t addr) const {
  if (addr <= 0x1FFF /* End of CHR */) {
    if (m_file->hdr.charSize == 0) return 0;
    addr += m_file->hdr.progSize * PRG_BLOCK_SIZE;
    if (m_file->hdr.flags6.trainer) addr += TRAINER_BLOCK_SIZE;
    return addr;
  }

  throw;
}
