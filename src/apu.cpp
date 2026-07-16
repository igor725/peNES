#include "apu.hh"

APU::APU(CPU6502& c): m_cpu(c) {}

APU::~APU() {}

std::optional<uint8_t> APU::handleRead(uint16_t addr) {
  if (addr >= 0x4000 && addr <= 0x4003) /* Pulse 1 */ {
    return 0;
  } else if (addr == 0x4015) /* APU Status */ {
    return 0;
  }

  return {};
}

bool APU::handleWrite(uint16_t addr, uint8_t value) {
  if (addr >= 0x4000 && addr <= 0x4003) /* Pulse 1 */ {
    return true;
  } else if (addr >= 0x4004 && addr <= 0x4007) /* Pulse 2 */ {
    return true;
  } else if (addr >= 0x4008 && addr <= 0x400B) /* Triangle */ {
    return true;
  } else if (addr >= 0x400C && addr <= 0x400F) /* Noise */ {
    return true;
  } else if (addr >= 0x4010 && addr <= 0x4013) /* DMC */ {
    return true;
  } else if (addr == 0x4015) /* APU Status */ {
    return true;
  } else if (addr == 0x4017) /* Frame Counter */ {
    m_cpu.triggerIRQ();
    return true;
  }

  return false;
}
