#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

class PlatImpl {
  protected:
  void*  m_mappedData  = nullptr;
  size_t m_mappedSize  = 0;
  bool   m_isAnonymous = false;

  PlatImpl()  = default;
  ~PlatImpl() = default;

  void mapPlatformFile(std::filesystem::path const& path);
  void mapPlatformStdin();
  void unmapPlatform();

  public:
  static std::string getErrorString(int32_t errorCode);
};
