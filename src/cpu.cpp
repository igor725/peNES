#include "cpu.hh"

#include "cpu_table.hh"

#include <iostream>

CPU6502::CPU6502() {}

CPU6502::~CPU6502() {}

bool CPU6502::loadCartridge(iNES const& c) {
  c.copyPRG(&m_ram[0x8000]);
  reset();
  m_regs.PC = (m_ram[0xFFFD] << 8) | m_ram[0xFFFc];
  return true;
}

void CPU6502::reset() {
  m_regs.P.I = true;
}

void CPU6502::step() {
  auto  inst     = (Instruction*)&m_ram[m_regs.PC];
  auto& instInfo = CPU_INSTS[inst->opcode];
  if (instInfo.mode == AddrMode::Illegal) throw IllegalInstruction(inst->opcode);

  auto _asm = [&]() {
    std::cerr << instInfo.name;
    switch (instInfo.mode) {
      case AddrMode::Implied: break;
      case AddrMode::Immediate: std::cerr << " #" << std::hex << (int)inst->operand; break;
    }
    std::cerr << std::endl;
  };
  _asm();

  switch (inst->opcode) {
    // case 0x58: {
    //   m_regs.P.I = false;
    // } break;
    case 0x78: {
      m_regs.P.I = true;
    } break;
    case 0x8E: {
      // TODO: STX
    } break;
    case 0x9A: {
      // TODO: TSX
    } break;
    case 0xA2: {
      // TODO: LDX
    } break;
    case 0xD8: {
      m_regs.P.D = false;
    } break;

    default: throw UnhandledInstruction();
  }

  m_regs.PC += instInfo.size;
}
