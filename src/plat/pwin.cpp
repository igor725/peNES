#include "plat.hh"

#include <cstdio>
#include <cstring>
#include <vector>
#include <windows.h>

void PlatImpl::mapPlatformFile(std::filesystem::path const& path) {
  HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) throw static_cast<int32_t>(GetLastError());
  }

  LARGE_INTEGER size;
  GetFileSizeEx(hFile, &size);
  m_mappedSize = size.QuadPart;

  HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_WRITECOPY, 0, 0, NULL);
  if (!hMapping) hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  CloseHandle(hFile);

  if (!hMapping) throw static_cast<int32_t>(GetLastError());

  m_mappedData = MapViewOfFile(hMapping, FILE_MAP_COPY, 0, 0, 0);
  if (!m_mappedData) m_mappedData = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
  CloseHandle(hMapping);

  if (!m_mappedData) throw static_cast<int32_t>(GetLastError());
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
  m_mappedData = VirtualAlloc(NULL, m_mappedSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

  if (!m_mappedData) throw static_cast<int32_t>(GetLastError());

  std::memcpy(m_mappedData, holder.data(), holder.size());
  m_isAnonymous = true;
}

void PlatImpl::unmapPlatform() {
  if (m_mappedData != nullptr) {
    if (m_isAnonymous) {
      VirtualFree(m_mappedData, 0, MEM_RELEASE);
    } else {
      UnmapViewOfFile(m_mappedData);
    }
    m_mappedData = nullptr;
    m_mappedSize = 0;
  }
}

std::string PlatImpl::getErrorString(int32_t errorCode) {
  char buf[256];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf),
                 NULL);
  return std::string(buf);
}
