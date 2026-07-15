#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

class iNES {
  static constexpr size_t PRG_BLOCK_SIZE     = 16384;
  static constexpr size_t CHR_BLOCK_SIZE     = 8192;
  static constexpr size_t TRAINER_BLOCK_SIZE = 512;

  struct File {
    struct [[gnu::packed]] Header {
      char    magic[4];
      uint8_t prg_size;
      uint8_t chr_size;

      struct [[gnu::packed]] {
        uint8_t mirroring     : 1;
        uint8_t battery       : 1;
        uint8_t trainer       : 1;
        uint8_t fourscreenram : 1;
        uint8_t mapperLow     : 4;
      } flags6;

      struct [[gnu::packed]] {
        uint8_t vs_unisystem : 1;
        uint8_t playchoice   : 1;
        uint8_t nes2         : 2;
        uint8_t mapperHigh   : 4;
      } flags7;

      uint8_t prg_ram_size;
      uint8_t tv_sys;
      uint8_t padding[6];
    } hdr;

    uint8_t data[];

    bool validate(size_t fsize) const {
      return hdr.magic[0] == 'N' && hdr.magic[1] == 'E' && hdr.magic[2] == 'S' && hdr.magic[3] == '\x1a' && hdr.tv_sys <= 1 && hdr.prg_size > 0 &&
             (fsize >= sizeof(Header) + (hdr.prg_size * PRG_BLOCK_SIZE) + (hdr.chr_size * CHR_BLOCK_SIZE) + (hdr.flags6.trainer * TRAINER_BLOCK_SIZE));
    }

    uint8_t getMapperId() const { return (hdr.flags7.mapperHigh << 4) | hdr.flags6.mapperLow; }

    bool isNTSC() const { return hdr.tv_sys == 0; }

    bool isVerticalMirror() const { return hdr.flags6.mirroring == 1; }

    std::span<uint8_t const> getPRG() const { return {data, PRG_BLOCK_SIZE * hdr.prg_size}; }
  };

  public:
  iNES(const char* filename);
  ~iNES();

  File* get() const { return m_file; }

  auto operator->() const { return m_file; }

  uint16_t resolveCPU(uint16_t addr) const;
  uint16_t resolvePPU(uint16_t addr) const;

  private:
  File* m_file;
};
