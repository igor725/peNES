#include "cpu.hh"
#include "ines.hh"
#include "ppu.hh"

#include <cassert>

int main(int argc, char* argv[]) {
  iNES cartridge(argv[1]);
  assert(cartridge.isNTSC());
  CPU6502 cpu;
  assert(cpu.loadCartridge(cartridge));
  PPU ppu(cpu, cartridge);

  while (true) {
    auto cycles = cpu.step();
    if (cycles >= 1) ppu.run(cycles * 3);
  }

  return 0;
}
