#include "cpu.hh"

#include "cpu_table.hh"

#include <iostream>

CPU6502::CPU6502() {}

CPU6502::~CPU6502() {}

bool CPU6502::loadCartridge(iNES const& c) {
  c.copyPRG(&m_ram[0x8000]);
  reset();
  return true;
}

void CPU6502::reset() {
  m_verbose  = true;
  m_regs.P.I = true;
  m_regs.P.D = false;
  m_regs.S   = 0xFD;
  m_regs.PC  = (m_ram[0xFFFD] << 8) | m_ram[0xFFFC];
}

uint8_t CPU6502::step() {
  auto  inst     = (Instruction*)&m_ram[m_regs.PC];
  auto& instInfo = CPU_INSTS[inst->opcode];
  if (instInfo.mode == AddrMode::Illegal) throw IllegalInstruction(inst->opcode);
  m_regs.PC += instInfo.size;

  if (m_verbose) {
    std::cerr << CPU_INST_NAME(instInfo.name);
    switch (instInfo.mode) {
      case AddrMode::Implied: break;
      case AddrMode::Immediate: std::cerr << " #$" << std::hex << (int)inst->operand; break;
      case AddrMode::IndY: std::cerr << " ($" << std::hex << (int)inst->operand << "),Y"; break;
      case AddrMode::Absolute: std::cerr << " $" << std::hex << (int)inst->operandw; break;
      case AddrMode::AbsoluteX: std::cerr << " $" << std::hex << (int)inst->operandw << ",X"; break;
      case AddrMode::ZeroPage: std::cerr << " z$" << std::hex << (int)inst->operand; break;
      case AddrMode::ZeroPageX: std::cerr << " z$" << std::hex << (int)inst->operand << ",X"; break;
      case AddrMode::Relative: std::cerr << " $" << std::hex << (int)inst->operand; break;

      default: std::cerr << " ???";
    }
    std::cerr << std::endl;
  }

  constexpr uint8_t _uacc = 1 << 0;
  constexpr uint8_t _ux   = 1 << 1;
  constexpr uint8_t _uy   = 1 << 2;

  uint8_t updated = 0, cycles = instInfo.cycles;

  switch (inst->opcode) {
    case 0x00: {
      pushStack<uint16_t>(m_regs.PC + 1 /* 1 byte padding? */);
      m_regs.P.I = true;
      pushStack<uint8_t>(m_regs.bP);
      m_regs.PC = (m_ram[0xFFFF] << 8) | m_ram[0xFFFE];
    } break;
    case 0x08: {
      pushStack<uint8_t>(m_regs.bP);
    } break;
    case 0x0A: { // ASL A
      m_regs.P.C = (m_regs.A & 0x80) > 0;
      m_regs.A <<= 1;
      updated |= _uacc;
    } break;
    case 0x10: { // BPL
      if (m_regs.P.N == 0) m_regs.PC += inst->s_operand;
    } break;
    case 0x18: { // CLC
      m_regs.P.C = 0;
    } break;
    case 0x20: { // JSR abs
      pushStack<uint16_t>(m_regs.PC - 1);
      m_regs.PC = inst->operandw;
    } break;
    case 0x28: { // PLP
      m_regs.bP = popStack<uint8_t>();
    } break;
    case 0x29: { // AND imm
      m_regs.A &= inst->operand;
      updated |= _uacc;
    } break;
    case 0x2C: { // BIT oper
      auto test  = m_ram[inst->operandw];
      m_regs.P.Z = (m_regs.A & test) == 0;
      m_regs.P.N = (test & 0x80) > 0;
      m_regs.P.V = (test & 0x40) > 0;
    } break;
    case 0x48: { // PHA
      pushStack<uint8_t>(m_regs.A);
    } break;
    case 0x4A: { // LSR A
      m_regs.P.C = (m_regs.A & 0x01) != 0;
      m_regs.A >>= 1;
      updated |= _uacc;
    } break;
    case 0x4C: { // JMP
      m_regs.PC = inst->operandw;
    } break;
    case 0x5D: {
      uint16_t addr = inst->operandw + m_regs.X;

      m_regs.A ^= m_ram[addr];

      if ((inst->operandw & 0xFF00) != (addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0x60: { // RTS
      m_regs.PC = popStack<uint16_t>() + 1;
    } break;
    case 0x68: { // PLA
      m_regs.A = popStack<uint8_t>();
      updated |= _uacc;
    } break;
    case 0x69: { // ADC imm
      uint16_t result = m_regs.A + inst->operand + m_regs.P.C;
      m_regs.P.C      = result > 0xFF;
      m_regs.P.V      = (~(m_regs.A ^ inst->operand) & (m_regs.A ^ result) & 0x80) != 0;
      m_regs.A        = result & 0xFF;
      updated |= _uacc;
    } break;
    case 0x78: { // SEI
      m_regs.P.I = true;
    } break;
    case 0x84: { // STY oper
      m_ram[inst->operand] = m_regs.Y;
    } break;
    case 0x85: { // STA zp
      m_ram[inst->operand] = m_regs.A;
    } break;
    case 0x86: { // STX zp
      m_ram[inst->operand] = m_regs.X;
    } break;
    case 0x88: { // STX zp
      --m_regs.Y;
      updated |= _uy;
    } break;
    case 0x8A: { // TXA
      m_regs.A = m_regs.X;
      updated |= _uacc;
    } break;
    case 0x8D: { // STA abs
      m_ram[inst->operandw] = m_regs.A;
    } break;
    case 0x8E: { // STX abs
      m_ram[inst->operandw] = m_regs.X;
    } break;
    case 0x95: { // STA oper,X
      m_ram[(m_regs.X + inst->operand) & 0xFF] = m_regs.A;
    } break;
    case 0x9A: { // TXS
      m_regs.S = m_regs.X;
    } break;
    case 0x9D: { // STA abs,X
      m_ram[m_regs.X + inst->operandw] = m_regs.A;
    } break;
    case 0xA0: { // LDY imm
      m_regs.Y = inst->operand;
      updated |= _uy;
    } break;
    case 0xA2: { // LDX imm
      m_regs.X = inst->operand;
      updated |= _ux;
    } break;
    case 0xA5: { // LDA zp
      m_regs.A = m_ram[inst->operand];
      updated |= _uacc;
    } break;
    case 0xA9: { // LDA imm
      m_regs.A = inst->operand;
      updated |= _uacc;
    } break;
    case 0xAA: { // TAX
      m_regs.X = m_regs.A;
      updated |= _ux;
    } break;
    case 0xAD: { // LDA abs
      m_regs.A = m_ram[inst->operandw];
      updated |= _uacc;
    } break;
    case 0xB1: { // LDA (oper),Y
      uint16_t base_addr   = (m_ram[(inst->operand + 1) & 0xFF] << 8) | m_ram[inst->operand];
      uint16_t target_addr = base_addr + m_regs.Y;

      m_regs.A = m_ram[target_addr];
      updated |= _uacc;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) { // Page boundary crossed
        cycles += 1;
      }
    } break;
    case 0xBC: { // LDY oper,X
      uint16_t addr = inst->operandw + m_regs.X;

      m_regs.Y = m_ram[addr];

      if ((inst->operandw & 0xFF00) != (addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xCA: { // DEX
      m_regs.X -= 1;
      updated |= _ux;
    } break;
    case 0xD0: { // BNE rel
      if (m_regs.P.Z == 0) m_regs.PC += inst->s_operand;
    } break;
    case 0xD8: { // CLD
      m_regs.P.D = false;
    } break;
    case 0xE0: { // CPX imm
      uint8_t val = m_regs.X - inst->operand;

      m_regs.P.C = m_regs.X >= inst->operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xE6: {
      ++m_ram[inst->operand];
    } break;
    case 0xE8: { // INX
      m_regs.X += 1;
      updated |= _ux;
    } break;
    case 0xF0: { // BEQ
      if (m_regs.P.Z == 1) m_regs.PC += inst->operand;
    } break;

    default: throw UnhandledInstruction(inst->opcode);
  }

  if (updated == _uacc) {
    m_regs.P.Z = m_regs.A == 0;
    m_regs.P.N = (m_regs.A & 0x80) > 0;
  } else if (updated == _ux) {
    m_regs.P.Z = m_regs.X == 0;
    m_regs.P.N = (m_regs.X & 0x80) > 0;
  } else if (updated == _uy) {
    m_regs.P.Z = m_regs.Y == 0;
    m_regs.P.N = (m_regs.Y & 0x80) > 0;
  }

  return cycles;
}

void CPU6502::writeRam(uint16_t addr, uint8_t value) {
  m_ram[addr] = value;
}

uint8_t CPU6502::readRam(uint16_t addr) const {
  return m_ram[addr];
}
