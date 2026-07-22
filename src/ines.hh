#pragma once

#include "plat/plat.hh"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>

class Mapper;

class CartridgeException: public std::exception {
  std::string m_what;

  public:
  enum class Type {
    IncompleteFile,
    ValidateFail,
    UnsupportedMapper,
    UnsupportedSubMapper,
    PALDump,
  };

  CartridgeException(int32_t errorCode);

  CartridgeException(Type type, int32_t additionalData = 0);

  const char* what() const noexcept override { return m_what.c_str(); }
};

class iNES: public PlatImpl {
  struct File {
    struct [[gnu::packed]] Header {
      enum class Region : uint8_t { NTSC, PAL, Multi, Dendy };
      enum class Device : uint8_t { FamiCom, NinVs, PlayChoise, Extended };

      char    magic[4];
      uint8_t _progSize;
      uint8_t _charSize;

      struct [[gnu::packed]] {
        uint8_t mirroring  : 1;
        uint8_t battery    : 1;
        uint8_t trainer    : 1;
        uint8_t altNametab : 1;
        uint8_t mapperLow  : 4;

        Device  deviceType : 2;
        uint8_t nes2       : 2;
        uint8_t mapperHigh : 4;
      } flags;

      union {
        struct [[gnu::packed]] {
          uint8_t prgRamSize;
          uint8_t flipMode;
          uint8_t padding[6];
        } nes1;

        struct [[gnu::packed]] {
          uint8_t mapperHighest : 4;
          uint8_t subMapper     : 4;

          uint8_t progSizeHigh : 4;
          uint8_t charSizeHigh : 4;

          uint8_t progRamShifts   : 4;
          uint8_t progNvramShifts : 4;

          uint8_t charRamShifts   : 4;
          uint8_t charNvramShifts : 4;

          Region region : 2;
          uint8_t       : 6;

          uint8_t ppuType : 4;
          uint8_t hwType  : 4;

          uint8_t numMiscRoms : 2;
          uint8_t             : 6;

          uint8_t expansionDevice;
        } nes2;
      } flagsEx;

      inline bool isNes2() const { return flags.nes2 == 2; }

      inline uint16_t getProgNum() const { return (isNes2() ? (flagsEx.nes2.progSizeHigh << 8) : 0) | _progSize; }

      inline uint16_t getCharNum() const { return (isNes2() ? (flagsEx.nes2.charSizeHigh << 8) : 0) | _charSize; }

      inline uint32_t getPrgRamSize() const {
        if (isNes2()) {
          uint32_t const volatileRam = flagsEx.nes2.progRamShifts ? (64 << flagsEx.nes2.progRamShifts) : 0;
          uint32_t const nvRam       = flagsEx.nes2.progNvramShifts ? (64 << flagsEx.nes2.progNvramShifts) : 0;
          return volatileRam + nvRam;
        }

        return std::max(flagsEx.nes1.prgRamSize, (uint8_t)1) * 8192;
      }

      inline uint32_t getCharRamSize() const {
        if (isNes2()) {
          uint32_t const volatileRam = flagsEx.nes2.charRamShifts ? (64 << flagsEx.nes2.charRamShifts) : 0;
          uint32_t const nvRam       = flagsEx.nes2.charNvramShifts ? (64 << flagsEx.nes2.charNvramShifts) : 0;
          return volatileRam + nvRam;
        }

        return 8192;
      }

      inline uint16_t getMapperId() const { return (isNes2() ? flagsEx.nes2.mapperHighest << 8 : 0) | (flags.mapperHigh << 4) | flags.mapperLow; }

      inline uint8_t getSubMapperId() const { return isNes2() ? flagsEx.nes2.subMapper : 0; }

      inline Region getRegion() const { return isNes2() ? flagsEx.nes2.region : (flagsEx.nes1.flipMode ? Region::PAL : Region::NTSC); }

      inline bool validate(size_t fsize) const {
        if (!(magic[0] == 'N' && magic[1] == 'E' && magic[2] == 'S' && magic[3] == '\x1a')) return false; // Check header
        if (getProgNum() == 0) return false;
        if (isNes2()) {
          if (flagsEx.nes2.progNvramShifts != 0 && flags.battery == 0) return false;
          // TODO more validity checks?
        }
        return true;
      }

      inline bool isVerticalMirror() const { return flags.mirroring == 1; }
    } hdr;

    uint8_t data[];
  };

  static_assert(sizeof(File::Header) == 16);

  public:
  iNES() = default;
  ~iNES();

  void insert(std::filesystem::path const& path);

  void piped();

  File* get() const { return m_file; }

  auto operator->() const { return m_file; }

  auto getMapper() const { return m_mapper.get(); }

  void disableValidation() { m_validation = false; }

  size_t getFileSize() const { return m_mappedSize; }

  uint32_t resolveCPU(uint16_t addr) const;

  protected:
  void performSetup();

  private:
  std::unique_ptr<Mapper> m_mapper     = {};
  File*                   m_file       = nullptr;
  bool                    m_validation = true;
};
