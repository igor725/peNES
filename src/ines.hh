#pragma once

#include "mappers/mapper.hh"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>

class CartridgeException: public std::exception {
  std::string m_what;

  public:
  enum class Type {
    ValidateFail,
    UnsupportedMapper,
    UnmappedMemory,
  };

  CartridgeException(int32_t errorCode);

  CartridgeException(Type type);
};

class iNES {
  struct File {
    struct [[gnu::packed]] Header {
      char    magic[4];
      uint8_t progSize;
      uint8_t charSize;

      struct [[gnu::packed]] {
        uint8_t mirroring     : 1;
        uint8_t battery       : 1;
        uint8_t trainer       : 1;
        uint8_t fourscreenram : 1;
        uint8_t mapperLow     : 4;
      } flags6;

      struct [[gnu::packed]] {
        uint8_t vsUnisys   : 1;
        uint8_t playchoice : 1;
        uint8_t nes2       : 2;
        uint8_t mapperHigh : 4;
      } flags7;

      uint8_t prgRamSize;
      uint8_t flipMode;
      uint8_t padding[6];
    } hdr;

    uint8_t data[];

    bool validate(size_t fsize) const {
      return hdr.magic[0] == 'N' && hdr.magic[1] == 'E' && hdr.magic[2] == 'S' && hdr.magic[3] == '\x1a' && hdr.flipMode <= 1 && hdr.progSize > 0 &&
             (fsize >= sizeof(Header) + (hdr.progSize * PRG_BLOCK_SIZE) + (hdr.charSize * CHR_BLOCK_SIZE) + (hdr.flags6.trainer * TRAINER_BLOCK_SIZE));
    }

    uint8_t getMapperId() const { return (hdr.flags7.mapperHigh << 4) | hdr.flags6.mapperLow; }

    bool isNTSC() const { return hdr.flipMode == 0; }

    bool isVerticalMirror() const { return hdr.flags6.mirroring == 1; }
  };

  public:
  static constexpr size_t PRG_BLOCK_SIZE     = 16384;
  static constexpr size_t CHR_BLOCK_SIZE     = 8192;
  static constexpr size_t TRAINER_BLOCK_SIZE = 512;

  iNES() = default;
  ~iNES();

  void insert(std::filesystem::path const& path);

  File* get() const { return m_file; }

  auto operator->() const { return m_file; }

  auto getMapper() const { return m_mapper.get(); }

  uint16_t resolveCPU(uint16_t addr) const;
  uint16_t resolvePPU(uint16_t addr) const;

  private:
  std::unique_ptr<Mapper> m_mapper = {};
  File*                   m_file   = nullptr;
  size_t                  m_size   = 0;
};
