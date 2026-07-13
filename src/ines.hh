#pragma once
#include "constants.hh"

#include <cstddef>
#include <cstdint>
#include <cstring>

class iNES {
  struct File {
    struct Header {
      char    magic[4];
      uint8_t prg_size;
      uint8_t chr_size;

      struct {
        uint8_t mirroring     : 1;
        uint8_t battery       : 1;
        uint8_t trainer       : 1;
        uint8_t fourscreenram : 1;
        uint8_t mapperLow     : 4;
      } flags6;

      struct {
        uint8_t vs_unisystem : 1;
        uint8_t playchoice   : 1;
        uint8_t nes2         : 2;
        uint8_t mapperLow    : 4;
      } flags7;

      uint8_t prg_ram_size;
      uint8_t tv_sys;
      uint8_t padding[6];
    } hdr;

    uint8_t data[];

    bool validate(size_t fsize) const {
      return hdr.magic[0] == 'N' && hdr.magic[1] == 'E' && hdr.magic[2] == 'S' && hdr.magic[3] == '\x1a' && hdr.tv_sys <= 1 &&
             (fsize == sizeof(Header) + (hdr.prg_size * PRG_BLOCK_SIZE) + (hdr.chr_size * CHR_BLOCK_SIZE) + (hdr.flags6.trainer * TRAINER_BLOCK_SIZE));
    }
  };

  public:
  iNES(const char* filename);
  ~iNES();

  bool isNTSC() const { return m_file->hdr.tv_sys == 0; }

  void copyPRG(uint8_t* dst) const {
    ::memcpy(dst, m_file->data, PRG_BLOCK_SIZE * m_file->hdr.prg_size);
    if (m_file->hdr.prg_size < 2) ::memcpy(dst + PRG_BLOCK_SIZE, m_file->data, PRG_BLOCK_SIZE);
  }

  private:
  File const* m_file;
};
