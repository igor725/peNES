#include "cpu.hh"

#include "cpu_table.hh"
#include "ppu.hh"

#include <iostream>

CPU6502::CPU6502() {}

CPU6502::~CPU6502() {}

void CPU6502::setPPU(PPU* ppu) {
  m_ppu = ppu;
}

bool CPU6502::loadCartridge(iNES const& c) {
  c.copyPRG(&m_ram[0x8000]);
  reset();
  return true;
}

void CPU6502::reset() {
  m_verbose      = false;
  m_nmitriggered = false;
  m_regs.P._raw  = 1;
  m_regs.P.I     = true;
  m_regs.P.D     = false;
  m_regs.S       = 0xFF;
  m_regs.PC      = (m_ram[0xFFFD] << 8) | m_ram[0xFFFC];
}

uint8_t CPU6502::step() {
  if (m_nmitriggered) {
    m_nmitriggered = false;

    pushStack<uint16_t>(m_regs.PC);

    Status _p = m_regs.P;
    _p.B = 0, _p.U = 1;
    pushStack<uint8_t>(_p._raw);

    m_regs.P.I = 1;
    m_regs.PC  = (m_ram[0xFFFB] << 8) | m_ram[0xFFFA];
    return 7;
  }

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
      case AddrMode::AbsoluteY: std::cerr << " $" << std::hex << (int)inst->operandw << ",Y"; break;
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
    case 0x00: { // BRK
      pushStack<uint16_t>(m_regs.PC + 1 /* 1 byte padding? */);
      m_regs.P.I = 1, m_regs.P.B = 1;
      pushStack<uint8_t>(m_regs.P._raw);
      m_regs.PC = (m_ram[0xFFFF] << 8) | m_ram[0xFFFE];
    } break;
    case 0x01: { // ORA (oper,X)
      uint8_t  lookup_address = inst->operand + m_regs.X;
      uint16_t target_address = (readRam(static_cast<uint8_t>(lookup_address + 1)) << 8) | readRam(lookup_address);

      m_regs.A |= readRam(target_address);
      updated |= _uacc;
    } break;
    case 0x05: { // ORA zp
      m_regs.A |= readRam(inst->operand);
      updated |= _uacc;
    } break;
    case 0x08: { // PHP
      auto p = m_regs.P;

      p.B = 1;
      pushStack<uint8_t>(p._raw);
    } break;
    case 0x09: { // ORA imm
      m_regs.A |= inst->operand;
      updated |= _uacc;
    } break;
    case 0x0A: { // ASL A
      m_regs.P.C = (m_regs.A & 0x80) > 0;
      m_regs.A <<= 1;
      updated |= _uacc;
    } break;
    case 0x0D: { // ORA abs
      m_regs.A |= readRam(inst->operandw);
      updated |= _uacc;
    } break;
    case 0x0E: { // ASL abs
      auto orig = readRam(inst->operandw);
      auto res  = writeRam(inst->operandw, orig << 1);

      m_regs.P.C = (orig & 0x80) > 0;
      m_regs.P.Z = (res == 0);
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0x10: { // BPL rel
      if (m_regs.P.N == 0) {
        uint16_t old_pc = m_regs.PC;
        m_regs.PC += inst->s_operand;
        cycles += 1;
        if ((old_pc & 0xFF00) != (m_regs.PC & 0xFF00)) {
          cycles += 1;
        }
      }
    } break;
    case 0x16: { // ASL zp,X
      uint8_t addr = static_cast<uint8_t>(inst->operand + m_regs.X);
      auto    orig = readRam(addr);
      auto    res  = writeRam(addr, orig << 1);

      m_regs.P.C = (orig & 0x80) > 0;
      m_regs.P.Z = (res == 0);
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0x18: { // CLC
      m_regs.P.C = 0;
    } break;
    case 0x19: { // ORA abs,Y
      uint16_t base_addr   = inst->operandw;
      uint16_t target_addr = base_addr + m_regs.Y;

      m_regs.A |= readRam(target_addr);
      updated |= _uacc;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0x1E: { // ASL abs,X
      auto addr = inst->operandw + m_regs.X;
      auto orig = readRam(addr);
      auto res  = writeRam(addr, orig << 1);

      m_regs.P.C = (orig & 0x80) > 0;
      m_regs.P.Z = (res == 0);
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0x20: { // JSR abs
      pushStack<uint16_t>(m_regs.PC - 1);
      m_regs.PC = inst->operandw;
    } break;
    case 0x24: { // BIT zp
      auto test  = readRam(inst->operand);
      m_regs.P.Z = (m_regs.A & test) == 0;
      m_regs.P.N = (test & 0x80) > 0;
      m_regs.P.V = (test & 0x40) > 0;
    } break;
    case 0x26: { // ROL zp
      uint8_t value          = readRam(inst->operand);
      uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

      m_regs.P.C = (value & 0x80) != 0;

      uint8_t shifted_value = (value << 1) | incoming_carry;

      writeRam(inst->operand, shifted_value);
      m_regs.P.Z = (shifted_value == 0);
      m_regs.P.N = (shifted_value & 0x80) != 0;
    } break;
    case 0x28: { // PLP
      m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
    } break;
    case 0x29: { // AND imm
      m_regs.A &= inst->operand;
      updated |= _uacc;
    } break;
    case 0x2A: { // ROL A
      uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

      m_regs.P.C = (m_regs.A & 0x80) != 0;

      m_regs.A = (m_regs.A << 1) | incoming_carry;

      m_regs.P.Z = (m_regs.A == 0);
      m_regs.P.N = (m_regs.A & 0x80) != 0;
    } break;
    case 0x2C: { // BIT oper
      auto test  = readRam(inst->operandw);
      m_regs.P.Z = (m_regs.A & test) == 0;
      m_regs.P.N = (test & 0x80) > 0;
      m_regs.P.V = (test & 0x40) > 0;
    } break;
    case 0x2D: { // AND oper
      m_regs.A &= readRam(inst->operandw);
      updated |= _uacc;
    } break;
    case 0x2E: { // ROL abs
      uint8_t value          = readRam(inst->operandw);
      uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

      m_regs.P.C = (value & 0x80) != 0;

      uint8_t shifted_value = (value << 1) | incoming_carry;

      writeRam(inst->operandw, shifted_value);
      m_regs.P.Z = (shifted_value == 0);
      m_regs.P.N = (shifted_value & 0x80) != 0;
    } break;
    case 0x30: { // BMI rel
      if (m_regs.P.N == 1) {
        uint16_t old_pc = m_regs.PC;
        m_regs.PC += inst->s_operand;
        cycles += 1;
        if ((old_pc & 0xFF00) != (m_regs.PC & 0xFF00)) {
          cycles += 1;
        }
      }
    } break;
    case 0x36: { // ROL zp,X
      uint8_t addr = static_cast<uint8_t>(inst->operand + m_regs.X);

      uint8_t value          = readRam(addr);
      uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

      m_regs.P.C = (value & 0x80) != 0;

      uint8_t shifted_value = (value << 1) | incoming_carry;

      writeRam(addr, shifted_value);
      m_regs.P.Z = (shifted_value == 0);
      m_regs.P.N = (shifted_value & 0x80) != 0;
    } break;
    case 0x38: { // SEC
      m_regs.P.C = 1;
    } break;
    case 0x3D: { // AND oper,X
      m_regs.A &= readRam(static_cast<uint8_t>(inst->operand + m_regs.X));
      updated |= _uacc;
    } break;
    case 0x40: { // RTI
      m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
      m_regs.PC     = popStack<uint16_t>();
    } break;
    case 0x45: { // EOR zp
      m_regs.A ^= readRam(inst->operand);
      updated |= _uacc;
    } break;
    case 0x46: { // LSR zp
      auto orig = readRam(inst->operand);
      auto wr   = writeRam(inst->operand, orig >> 1);

      m_regs.P.N = 0;
      m_regs.P.Z = wr == 0;
      m_regs.P.C = (orig & 0x01) != 0;
    } break;
    case 0x48: { // PHA
      pushStack<uint8_t>(m_regs.A);
    } break;
    case 0x49: { // EOR imm
      m_regs.A ^= inst->operand;
      updated |= _uacc;
    } break;
    case 0x4A: { // LSR A
      m_regs.P.C = (m_regs.A & 0x01) != 0;
      m_regs.A >>= 1;
      updated |= _uacc;
    } break;
    case 0x4C: { // JMP
      m_regs.PC = inst->operandw;
    } break;
    case 0x4D: { // EOR abs
      m_regs.A ^= readRam(inst->operandw);
      updated |= _uacc;
    } break;
    case 0x4E: { // LSR abs
      auto orig = readRam(inst->operandw);
      auto wr   = writeRam(inst->operandw, orig >> 1);

      m_regs.P.N = 0;
      m_regs.P.Z = wr == 0;
      m_regs.P.C = (orig & 0x01) != 0;
    } break;
    case 0x5D: { // EOR abs,X
      uint16_t addr = inst->operandw + m_regs.X;

      m_regs.A ^= readRam(addr);
      updated |= _uacc;

      if ((inst->operandw & 0xFF00) != (addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0x60: { // RTS
      m_regs.PC = popStack<uint16_t>() + 1;
    } break;
    case 0x65: { // ADC zp
      uint8_t  operand = readRam(inst->operand);
      uint16_t res     = m_regs.A + operand + (m_regs.P.C ? 1 : 0);

      m_regs.P.C = res > 0xFF;
      m_regs.P.V = (~(m_regs.A ^ operand) & (m_regs.A ^ res) & 0x80) != 0x00;
      m_regs.A   = res & 0xFF;
      updated |= _uacc;
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
    case 0x6A: { // ROR A
      uint8_t incoming_carry = m_regs.P.C ? 0x80 : 0x00;
      uint8_t shifted_value  = (m_regs.A >> 1) | incoming_carry;
      m_regs.P.C             = (m_regs.A & 0x01) != 0;

      m_regs.A = shifted_value;
      updated |= _uacc;
    } break;
    case 0x6C: { // JMP (oper)
      uint16_t low_addr  = inst->operandw;
      uint16_t high_addr = (low_addr & 0xFF00) | static_cast<uint8_t>((low_addr & 0xFF) + 1);
      m_regs.PC          = (readRam(high_addr) << 8) | readRam(low_addr);
    } break;
    case 0x6D: { // ADC abs
      uint8_t  operand = readRam(inst->operandw);
      uint16_t res     = m_regs.A + operand + (m_regs.P.C ? 1 : 0);

      m_regs.P.C = res > 0xFF;
      m_regs.P.V = (~(m_regs.A ^ operand) & (m_regs.A ^ res) & 0x80) != 0x00;
      m_regs.A   = res & 0xFF;
      updated |= _uacc;
    } break;
    case 0x78: { // SEI
      m_regs.P.I = true;
    } break;
    case 0x79: { // ADC abs,Y
      uint8_t  operand = readRam(inst->operandw + m_regs.Y);
      uint16_t res     = m_regs.A + operand + (m_regs.P.C ? 1 : 0);

      m_regs.P.C = res > 0xFF;
      m_regs.P.V = (~(m_regs.A ^ operand) & (m_regs.A ^ res) & 0x80) != 0x00;
      m_regs.A   = res & 0xFF;
      updated |= _uacc;
    } break;
    case 0x7E: { // ROR abs,X
      uint16_t base_addr   = inst->operandw;
      uint16_t target_addr = base_addr + m_regs.X;

      uint8_t value = readRam(target_addr);

      uint8_t incoming_carry = m_regs.P.C ? 0x80 : 0x00;
      uint8_t shifted_value  = (value >> 1) | incoming_carry;
      m_regs.P.C             = (value & 0x01) != 0;

      writeRam(target_addr, shifted_value);
      m_regs.P.Z = (shifted_value == 0);
      m_regs.P.N = (shifted_value & 0x80) != 0;
    } break;
    case 0x84: { // STY oper
      writeRam(inst->operand, m_regs.Y);
    } break;
    case 0x85: { // STA zp
      writeRam(inst->operand, m_regs.A);
    } break;
    case 0x86: { // STX zp
      writeRam(inst->operand, m_regs.X);
    } break;
    case 0x88: { // STX zp
      --m_regs.Y;
      updated |= _uy;
    } break;
    case 0x8A: { // TXA
      m_regs.A = m_regs.X;
      updated |= _uacc;
    } break;
    case 0x8C: { // STY abs
      writeRam(inst->operandw, m_regs.Y);
    } break;
    case 0x8D: { // STA abs
      writeRam(inst->operandw, m_regs.A);
    } break;
    case 0x8E: { // STX abs
      writeRam(inst->operandw, m_regs.X);
    } break;
    case 0x90: { // BCC rel
      if (m_regs.P.C == 0) {
        uint16_t old_pc = m_regs.PC;
        m_regs.PC += inst->s_operand;
        cycles += 1;
        if ((old_pc & 0xFF00) != (m_regs.PC & 0xFF00)) {
          cycles += 1;
        }
      }
    } break;
    case 0x91: { // STA (oper),Y
      uint16_t base_address   = (readRam(static_cast<uint8_t>(inst->operand + 1)) << 8) | readRam(inst->operand);
      uint16_t target_address = base_address + m_regs.Y;
      writeRam(target_address, m_regs.A);
    } break;
    case 0x95: { // STA oper,X
      writeRam((m_regs.X + inst->operand) & 0xFF, m_regs.A);
    } break;
    case 0x98: { // TYA
      m_regs.A = m_regs.Y;
      updated |= _uacc;
    } break;
    case 0x99: { // STA abs,Y
      writeRam(inst->operandw + m_regs.Y, m_regs.A);
    } break;
    case 0x9A: { // TXS
      m_regs.S = m_regs.X;
    } break;
    case 0x9D: { // STA abs,X
      writeRam(m_regs.X + inst->operandw, m_regs.A);
    } break;
    case 0xA0: { // LDY imm
      m_regs.Y = inst->operand;
      updated |= _uy;
    } break;
    case 0xA2: { // LDX imm
      m_regs.X = inst->operand;
      updated |= _ux;
    } break;
    case 0xA4: { // LDY zp
      m_regs.Y = readRam(inst->operand);
      updated |= _uy;
    } break;
    case 0xA5: { // LDA zp
      m_regs.A = readRam(inst->operand);
      updated |= _uacc;
    } break;
    case 0xA6: { // LDX zp
      m_regs.X = readRam(inst->operand);
      updated |= _ux;
    } break;
    case 0xA8: { // TAY
      m_regs.Y = m_regs.A;
      updated |= _uy;
    } break;
    case 0xA9: { // LDA imm
      m_regs.A = inst->operand;
      updated |= _uacc;
    } break;
    case 0xAA: { // TAX
      m_regs.X = m_regs.A;
      updated |= _ux;
    } break;
    case 0xAC: { // LDY
      m_regs.Y = readRam(inst->operandw);
      updated |= _uy;
    } break;
    case 0xAD: { // LDA abs
      m_regs.A = readRam(inst->operandw);
      updated |= _uacc;
    } break;
    case 0xAE: { // LDX abs
      m_regs.X = readRam(inst->operandw);
      updated |= _ux;
    } break;
    case 0xB0: { // BCS rel
      if (m_regs.P.C == 1) {
        uint16_t old_pc = m_regs.PC;
        m_regs.PC += inst->s_operand;
        cycles += 1;
        if ((old_pc & 0xFF00) != (m_regs.PC & 0xFF00)) {
          cycles += 1;
        }
      }
    } break;
    case 0xB1: { // LDA (oper),Y
      uint16_t base_addr   = (readRam((inst->operand + 1) & 0xFF) << 8) | readRam(inst->operand);
      uint16_t target_addr = base_addr + m_regs.Y;

      m_regs.A = readRam(target_addr);
      updated |= _uacc;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) { // Page boundary crossed
        cycles += 1;
      }
    } break;
    case 0xB5: { // LDA zp,X
      m_regs.A = readRam(inst->operand + m_regs.X);
      updated |= _uacc;
    } break;
    case 0xB9: { // LDA abs,Y
      uint16_t base_addr   = inst->operandw;
      uint16_t target_addr = base_addr + m_regs.Y;

      m_regs.A = readRam(target_addr);
      updated |= _uacc;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) { // Page boundary crossed
        cycles += 1;
      }
    } break;
    case 0xBA: { // TSX
      m_regs.X = m_regs.S;
      updated |= _ux;
    } break;
    case 0xBC: { // LDY oper,X
      uint16_t addr = inst->operandw + m_regs.X;

      m_regs.Y = readRam(addr);

      if ((inst->operandw & 0xFF00) != (addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xBD: { // LDA abs,X
      uint16_t addr = inst->operandw + m_regs.X;

      m_regs.A = readRam(addr);

      if ((inst->operandw & 0xFF00) != (addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xBE: { // LDX oper,Y
      auto base_addr   = inst->operandw;
      auto target_addr = base_addr + m_regs.Y;
      m_regs.X         = readRam(target_addr);
      updated |= _ux;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xC0: { // CPY imm
      uint8_t val = m_regs.Y - inst->operand;

      m_regs.P.C = m_regs.Y >= inst->operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xC4: { // CPY zp
      uint8_t val = m_regs.Y - readRam(inst->operand);

      m_regs.P.C = m_regs.Y >= readRam(inst->operand);
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xC5: { // CMP zp
      uint8_t operand = readRam(inst->operand);
      uint8_t val     = m_regs.A - operand;

      m_regs.P.C = m_regs.A >= operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xC6: { // DEC
      auto res = writeRam(inst->operand, readRam(inst->operand) - 1);

      m_regs.P.Z = res == 0;
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0xC8: { // INY
      m_regs.Y += 1;
      updated |= _uy;
    } break;
    case 0xC9: { // CMP imm
      uint8_t val = m_regs.A - inst->operand;

      m_regs.P.C = m_regs.A >= inst->operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xCA: { // DEX
      m_regs.X -= 1;
      updated |= _ux;
    } break;
    case 0xCD: { // CMP abs
      uint8_t operand = readRam(inst->operandw);
      uint8_t val     = m_regs.A - operand;

      m_regs.P.C = m_regs.A >= operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xCE: { // DEC abs
      auto res = writeRam(inst->operandw, readRam(inst->operandw) - 1);

      m_regs.P.Z = res == 0;
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0xD0: { // BNE rel
      if (m_regs.P.Z == 0) {
        uint16_t old_pc = m_regs.PC;
        m_regs.PC += inst->s_operand;
        cycles += 1;
        if ((old_pc & 0xFF00) != (m_regs.PC & 0xFF00)) {
          cycles += 1;
        }
      }
    } break;
    case 0xD8: { // CLD
      m_regs.P.D = false;
    } break;
    case 0xD5: { // CMP zp,X
      uint8_t operand = readRam(inst->operand + m_regs.X);
      uint8_t val     = m_regs.A - operand;

      m_regs.P.C = m_regs.A >= operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xD9: { // CMP abs,Y
      uint16_t base_addr   = inst->operandw;
      uint16_t target_addr = base_addr + m_regs.Y;
      uint8_t  operand     = readRam(target_addr);
      uint8_t  val         = m_regs.A - operand;

      m_regs.P.C = m_regs.A >= operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xDD: { // CMP abs,X
      uint16_t base_addr   = inst->operandw;
      uint16_t target_addr = base_addr + m_regs.X;
      uint8_t  operand     = readRam(target_addr);
      uint8_t  val         = m_regs.A - operand;

      m_regs.P.C = m_regs.A >= operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xDE: { // DEC oper,X
      auto addr = inst->operandw + m_regs.X;

      auto res = writeRam(addr, readRam(addr) - 1);

      m_regs.P.Z = res == 0;
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0xE0: { // CPX imm
      uint8_t val = m_regs.X - inst->operand;

      m_regs.P.C = m_regs.X >= inst->operand;
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xE4: { // CPX zp
      uint8_t val = m_regs.X - readRam(inst->operand);

      m_regs.P.C = m_regs.X >= readRam(inst->operand);
      m_regs.P.Z = val == 0;
      m_regs.P.N = (val & 0x80) > 0;
    } break;
    case 0xE6: {
      auto res = writeRam(inst->operand, readRam(inst->operand) + 1);

      m_regs.P.Z = res == 0;
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0xE8: { // INX
      m_regs.X += 1;
      updated |= _ux;
    } break;
    case 0xE9: { // SBC imm
      uint8_t  inverted_value = ~inst->operand;
      uint16_t carry_in       = m_regs.P.C ? 1 : 0;
      uint16_t res            = m_regs.A + inverted_value + carry_in;

      m_regs.P.V = ((m_regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_regs.P.C = res > 0xFF;
      m_regs.A   = res & 0xFF;
      updated |= _uacc;
    } break;
    case 0xEE: { // INC
      auto res = writeRam(inst->operandw, readRam(inst->operandw) + 1);

      m_regs.P.Z = res == 0;
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0xF0: { // BEQ
      if (m_regs.P.Z == 1) {
        uint16_t old_pc = m_regs.PC;
        m_regs.PC += inst->s_operand;
        cycles += 1;
        if ((old_pc & 0xFF00) != (m_regs.PC & 0xFF00)) {
          cycles += 1;
        }
      }
    } break;
    case 0xF6: { // INC zp,X
      auto addr = static_cast<uint8_t>(inst->operand + m_regs.X);

      auto res = writeRam(addr, readRam(addr) + 1);

      m_regs.P.Z = res == 0;
      m_regs.P.N = (res & 0x80) > 0;
    } break;
    case 0xF9: { // SBC abs,Y
      uint16_t base_addr      = inst->operandw;
      uint16_t target_addr    = base_addr + m_regs.Y;
      uint8_t  inverted_value = ~readRam(target_addr);
      uint16_t carry_in       = m_regs.P.C ? 1 : 0;
      uint16_t res            = m_regs.A + inverted_value + carry_in;

      m_regs.P.V = ((m_regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_regs.P.C = res > 0xFF;
      m_regs.A   = res & 0xFF;
      updated |= _uacc;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) {
        cycles += 1;
      }
    } break;
    case 0xFD: { // SBC abs,X
      uint16_t base_addr      = inst->operandw;
      uint16_t target_addr    = base_addr + m_regs.X;
      uint8_t  inverted_value = ~readRam(target_addr);
      uint16_t carry_in       = m_regs.P.C ? 1 : 0;
      uint16_t res            = m_regs.A + inverted_value + carry_in;

      m_regs.P.V = ((m_regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_regs.P.C = res > 0xFF;
      m_regs.A   = res & 0xFF;
      updated |= _uacc;

      if ((base_addr & 0xFF00) != (target_addr & 0xFF00)) {
        cycles += 1;
      }
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

void CPU6502::triggerNmi() {
  m_nmitriggered = true;
}

uint8_t CPU6502::writeRam(uint16_t addr, uint8_t value) {
  if (addr >= 0x2000 && addr <= 0x2007) m_ppu->cpuWrite(addr, value);
  return m_ram[addr] = value;
}

uint8_t CPU6502::readRam(uint16_t addr) const {
  if (addr >= 0x2000 && addr <= 0x2007) return m_ppu->cpuRead(addr);
  return m_ram[addr];
}
