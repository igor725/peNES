#include "plat.hh"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

void PlatImpl::mapPlatformFile(std::filesystem::path const& path) {
  int32_t const fd = ::open(path.string().c_str(), O_RDONLY);
  if (fd < 0) throw errno;

  struct stat sb;
  if (::fstat(fd, &sb) == -1) {
    auto const ecode = errno;
    ::close(fd);
    throw ecode;
  }

  m_mappedSize = sb.st_size;
  m_mappedData = ::mmap(nullptr, m_mappedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  ::close(fd);

  if (m_mappedData == MAP_FAILED) throw errno;
  m_isAnonymous = false;
}

void PlatImpl::mapPlatformStdin() {
  std::vector<uint8_t> holder;
  holder.reserve(256 * 1024);

  char buf[16 * 1024];
  while (auto const rd = ::fread((void*)&buf, 1, sizeof(buf), stdin)) {
    holder.insert(holder.end(), buf, buf + rd);
  }

  m_mappedSize = holder.size();
  m_mappedData = ::mmap(nullptr, m_mappedSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (m_mappedData == MAP_FAILED) throw errno;
  std::memcpy(m_mappedData, holder.data(), holder.size());
  m_isAnonymous = true;
}

void PlatImpl::unmapPlatform() {
  if (m_mappedData != nullptr) {
    ::munmap(m_mappedData, m_mappedSize);
    m_mappedData = nullptr;
    m_mappedSize = 0;
  }
}

std::string PlatImpl::getErrorString(int32_t errorCode) {
  return std::string(::strerror(errorCode));
}
