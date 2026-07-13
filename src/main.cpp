#include "cpu.hh"
#include "ines.hh"

#include <cassert>

int main(void) {
  iNES cartridge("./2048.nes");
  assert(cartridge.isNTSC());
  CPU6502 cpu;
  assert(cpu.loadCartridge(cartridge));

  while (true) {
    cpu.step();
  }

  return 0;
}
