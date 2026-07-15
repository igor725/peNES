#include "cpu.hh"

#include <cstdint>

CPU6502::CPU6502() {}

CPU6502::~CPU6502() {}

void CPU6502::reset() {
  m_nmitriggered = false;
  m_padButtons   = 0;
  m_padCounter   = 0;
  m_regs.P._raw  = 1;
  m_regs.P.I     = true;
  m_regs.P.D     = false;
  m_regs.SP      = 0xFF;
  m_regs.PC      = readRam<uint16_t>(0xFFFC);
}

uint8_t CPU6502::handleControl(Instruction inst) {
  if (inst.branch.sig == 0x10) { // Handle branching
    auto operand = readPC<int8_t>();

    bool flag_status = false;

    switch (inst.branch.sel) {
      case 0b00: flag_status = m_regs.P.N; break; // Negative
      case 0b01: flag_status = m_regs.P.V; break; // Overflow
      case 0b10: flag_status = m_regs.P.C; break; // Carry
      case 0b11: flag_status = m_regs.P.Z; break; // Zero
    }

    if (flag_status != inst.branch.cond) return 2;

    uint16_t old_pc = m_regs.PC;
    m_regs.PC += operand;
    return (old_pc & 0xFF00) == (m_regs.PC & 0xFF00) ? 3 : 4;
  }

  switch (inst.deflt.operation) {
    case 0x00: { // Control Flow
      if (inst.getAddrMode() == 0x00) /* BRK */ {
        pushStack<uint16_t>(++m_regs.PC);
        m_regs.P.I = 1;

        auto p = m_regs.P;

        p.B = 1;
        pushStack<uint8_t>(p._raw);
        m_regs.PC = readRam<uint16_t>(0xFFFE);
        return 7;
      } else if (inst.getAddrMode() == 0x01) /* NOP */ {
        ++m_regs.PC;
        return 2;
      } else if (inst.getAddrMode() == 0x02) /* PHP */ {
        auto p = m_regs.P;

        p.B = 1;
        pushStack<uint8_t>(p._raw);
        return 3;
      } else if (inst.getAddrMode() == 0x03) /* NOP */ {
        readRam<uint16_t>(readPC<uint16_t>());
        return 4;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* CLC */ {
        m_regs.P.C = 0;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* CLC */ {
      }
    } break;
    case 0x01: {
      if (inst.getAddrMode() == 0x00) /* JSR */ {
        auto const operand = readPC<uint16_t>();

        pushStack<uint16_t>(m_regs.PC - 1);
        m_regs.PC = operand;
        return 6;
      } else if (inst.getAddrMode() == 0x01) /* BIT zp */ {
        auto test  = readRamByte(readPC<uint8_t>());
        m_regs.P.Z = (m_regs.A & test) == 0;
        m_regs.P.N = (test & 0x80) > 0;
        m_regs.P.V = (test & 0x40) > 0;
        return 3;
      } else if (inst.getAddrMode() == 0x02) /* PLP */ {
        m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
        return 4;
      } else if (inst.getAddrMode() == 0x03) /* BIT abs */ {
        auto test  = readRamByte(readPC<uint16_t>());
        m_regs.P.Z = (m_regs.A & test) == 0;
        m_regs.P.N = (test & 0x80) > 0;
        m_regs.P.V = (test & 0x40) > 0;
        return 4;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* SEC */ {
        m_regs.P.C = 1;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x02: {
      if (inst.getAddrMode() == 0x00) /* RTI */ {
        m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
        m_regs.PC     = popStack<uint16_t>();
        return 6;
      } else if (inst.getAddrMode() == 0x01) /* ??? */ {
      } else if (inst.getAddrMode() == 0x02) /* PHA */ {
        pushStack<uint8_t>(m_regs.A);
        return 3;
      } else if (inst.getAddrMode() == 0x03) /* JMP abs */ {
        m_regs.PC = readPC<uint16_t>();
        return 3;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* CLI */ {
        m_regs.P.I = 0;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x03: {
      if (inst.getAddrMode() == 0x00) /* RTS */ {
        m_regs.PC = popStack<uint16_t>() + 1;
        return 6;
      } else if (inst.getAddrMode() == 0x01) /* ??? */ {
      } else if (inst.getAddrMode() == 0x02) /* PLA */ {
        m_regs.A   = popStack<uint8_t>();
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = (m_regs.A & 0x80) > 0;
        return 4;
      } else if (inst.getAddrMode() == 0x03) /* JMP (oper) */ {
        uint16_t low_addr  = readPC<uint16_t>();
        uint16_t high_addr = (low_addr & 0xFF00) | static_cast<uint8_t>((low_addr & 0xFF) + 1);
        m_regs.PC          = (readRamByte(high_addr) << 8) | readRamByte(low_addr);
        return 5;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* SEI */ {
        m_regs.P.I = true;
        return 2;

      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x04: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* STY zp */ {
        writeRamByte(readPC<uint8_t>(), m_regs.Y);
        return 3;
      } else if (inst.getAddrMode() == 0x02) /* DEY */ {
        --m_regs.Y;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* STY abs */ {
        writeRamByte(readPC<uint16_t>(), m_regs.Y);
        return 4;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* STY zp,X */ {
        writeRamByte(static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X), m_regs.Y);
        return 4;
      } else if (inst.getAddrMode() == 0x06) /* TYA */ {
        m_regs.A   = m_regs.Y;
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = (m_regs.A & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x05: {
      if (inst.getAddrMode() == 0x00) /* LDY imm */ {
        m_regs.Y   = readPC<uint8_t>();
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x01) /* LDY zp */ {
        m_regs.Y   = readRamByte(readPC<uint8_t>());
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 3;
      } else if (inst.getAddrMode() == 0x02) /* TAY */ {
        m_regs.Y   = m_regs.A;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* LDY abs */ {
        m_regs.Y   = readRamByte(readPC<uint16_t>());
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 4;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* LDY zp,X */ {
        m_regs.Y   = readRamByte(static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X));
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 4;
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) /* LDY abs,X */ {
        uint16_t base_addr   = readPC<uint16_t>();
        uint16_t target_addr = base_addr + m_regs.X;

        m_regs.Y = readRamByte(target_addr);
        return (base_addr & 0xFF00) == (target_addr & 0xFF00) ? 4 : 5;
      }
    } break;
    case 0x06: {
      if (inst.getAddrMode() == 0x00) /* CPY imm */ {
        auto operand = readPC<uint8_t>();

        uint8_t val = m_regs.Y - operand;

        m_regs.P.C = m_regs.Y >= operand;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x01) /* CPY zp */ {
        auto operand = readRamByte(readPC<uint8_t>());

        uint8_t val = m_regs.Y - operand;

        m_regs.P.C = m_regs.Y >= operand;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return 3;
      } else if (inst.getAddrMode() == 0x02) /* INY */ {
        m_regs.Y += 1;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* ??? */ {
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* CLD */ {
        m_regs.P.D = false;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x07: {
      if (inst.getAddrMode() == 0x00) /* CPX */ {
        auto operand = readPC<uint8_t>();

        uint8_t val = m_regs.X - operand;

        m_regs.P.C = m_regs.X >= operand;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x01) /* CPX zp */ {
        auto operand = readRamByte(readPC<uint8_t>());

        uint8_t val = m_regs.X - operand;

        m_regs.P.C = m_regs.X >= operand;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return 3;
      } else if (inst.getAddrMode() == 0x02) /* INX */ {
        m_regs.X += 1;
        m_regs.P.Z = m_regs.X == 0;
        m_regs.P.N = (m_regs.X & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* ??? */ {
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* SED */ {
        m_regs.P.D = 1;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
  }

  throw UnhandledInstruction(inst.raw);
}

uint8_t CPU6502::handleMath(Instruction inst) {
  auto getAddrFromOp = [&]() -> std::pair<uint8_t /* num cycles */, uint16_t /* addr */> {
    switch (inst.getAddrMode()) {
      case 0b000 /* (Indir,X) */: {
        auto const operand = readPC<uint8_t>();
        return {5, (readRamByte(static_cast<uint8_t>((operand + m_regs.X) + 1)) << 8) | readRamByte(static_cast<uint8_t>(operand + m_regs.X))};
      } break;
      case 0b001 /* Zero Page */: return {1, readPC<uint8_t>()};
      case 0b011 /* Absolute */: return {2, readPC<uint16_t>()};
      case 0b100 /* (Indir),Y*/: {
        auto const operand = readPC<uint8_t>();
        auto const base    = ((readRamByte(static_cast<uint8_t>(operand + 1)) << 8) | readRamByte(operand));
        auto const target  = base + m_regs.Y;
        return {(base & 0xFF00) == (target & 0xFF00) ? 4 : 5, target};
      } break;
      case 0b101 /* Zero Page, X */: return {2, static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X)};
      case 0b110 /* Absolute, Y */: {
        auto const base   = readPC<uint16_t>();
        auto const target = base + m_regs.Y;
        return {(base & 0xFF00) == (target & 0xFF00) ? 3 : 4, target};
      } break;
      case 0b111 /* Absolute, X */: {
        auto const base   = readPC<uint16_t>();
        auto const target = base + m_regs.X;
        return {(base & 0xFF00) == (target & 0xFF00) ? 3 : 4, target};
      } break;
      default: throw -1;
    }
  };
  auto resolveValue = [&]() -> std::pair<uint8_t, uint8_t> {
    if (inst.getAddrMode() == 0x02) return {1, readPC<uint8_t>()};
    auto [cycles, addr] = getAddrFromOp();
    return {cycles, readRamByte(addr)};
  };

  switch (inst.deflt.operation) {
    case 0x00: { // ORA
      auto const [cycles, value] = resolveValue();

      m_regs.A |= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return 1 + cycles;
    } break;
    case 0x01: { // AND
      auto const [cycles, value] = resolveValue();

      m_regs.A &= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return 1 + cycles;
    } break;
    case 0x02: { // EOR
      auto const [cycles, value] = resolveValue();

      m_regs.A ^= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return 1 + cycles;
    } break;
    case 0x03: { // ADC
      auto const [cycles, value] = resolveValue();

      uint16_t result = m_regs.A + value + m_regs.P.C;
      m_regs.P.C      = result > 0xFF;
      m_regs.P.V      = (~(m_regs.A ^ value) & (m_regs.A ^ result) & 0x80) != 0;
      m_regs.A        = result & 0xFF;
      m_regs.P.Z      = m_regs.A == 0;
      m_regs.P.N      = (m_regs.A & 0x80) > 0;
      return 1 + cycles;
    } break;
    case 0x04: { // STA
      auto const [cycles, addr] = getAddrFromOp();

      writeRamByte(addr, m_regs.A);
      return 2 /* base number of cycles for STA */ + cycles;
    } break;
    case 0x05: { // LDA
      auto const [cycles, value] = resolveValue();

      m_regs.A   = value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return 1 + cycles;
    } break;
    case 0x06: { // CMP
      auto const [cycles, value] = resolveValue();

      uint8_t val = m_regs.A - value;
      m_regs.P.C  = m_regs.A >= value;
      m_regs.P.Z  = val == 0;
      m_regs.P.N  = (val & 0x80) > 0;
      return 1 + cycles;
    } break;
    case 0x07: { // SBC
      auto const [cycles, value] = resolveValue();

      uint8_t  inverted_value = value ^ 0xFF;
      uint16_t carry_in       = m_regs.P.C ? 1 : 0;
      uint16_t res            = m_regs.A + inverted_value + carry_in;

      m_regs.P.V = ((m_regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_regs.P.C = res > 0xFF;
      m_regs.A   = res & 0xFF;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return 1 + cycles;
    } break;
  }

  throw UnhandledInstruction(inst.raw);
}

uint8_t CPU6502::handleShift(Instruction inst) {
  switch (inst.deflt.operation) {
    case 0x00: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* ASL zp */ {
        auto const operand = readPC<uint8_t>();

        auto orig = readRamByte(operand);
        auto res  = writeRamByte(operand, orig << 1);

        m_regs.P.C = (orig & 0x80) > 0;
        m_regs.P.Z = (res == 0);
        m_regs.P.N = (res & 0x80) > 0;
        return 5;
      } else if (inst.getAddrMode() == 0x02) /* ASL A */ {
        m_regs.P.C = (m_regs.A & 0x80) > 0;
        m_regs.A <<= 1;
        m_regs.P.Z = (m_regs.A == 0);
        m_regs.P.N = (m_regs.A & 0x80) != 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* ASL abs */ {
        auto const operand = readPC<uint16_t>();

        auto orig = readRamByte(operand);
        auto res  = writeRamByte(operand, orig << 1);

        m_regs.P.C = (orig & 0x80) > 0;
        m_regs.P.Z = (res == 0);
        m_regs.P.N = (res & 0x80) > 0;
        return 6;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ASL zp,X */ {
        uint8_t addr = static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X);
        auto    orig = readRamByte(addr);
        auto    res  = writeRamByte(addr, orig << 1);

        m_regs.P.C = (orig & 0x80) > 0;
        m_regs.P.Z = (res == 0);
        m_regs.P.N = (res & 0x80) > 0;
        return 6;
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) {
        auto addr = readPC<uint16_t>() + m_regs.X;
        auto orig = readRamByte(addr);
        auto res  = writeRamByte(addr, orig << 1);

        m_regs.P.C = (orig & 0x80) > 0;
        m_regs.P.Z = (res == 0);
        m_regs.P.N = (res & 0x80) > 0;
        return 7;
      }
    } break;
    case 0x01: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* ROL zp */ {
        auto const operand = readPC<uint8_t>();

        uint8_t value          = readRamByte(operand);
        uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

        m_regs.P.C = (value & 0x80) != 0;

        uint8_t shifted_value = (value << 1) | incoming_carry;

        writeRamByte(operand, shifted_value);
        m_regs.P.Z = (shifted_value == 0);
        m_regs.P.N = (shifted_value & 0x80) != 0;
        return 5;
      } else if (inst.getAddrMode() == 0x02) /* ROL A */ {
        uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

        m_regs.P.C = (m_regs.A & 0x80) != 0;
        m_regs.A   = (m_regs.A << 1) | incoming_carry;
        m_regs.P.Z = (m_regs.A == 0);
        m_regs.P.N = (m_regs.A & 0x80) != 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* ROL abs */ {
        auto const operand = readPC<uint16_t>();

        uint8_t value          = readRamByte(operand);
        uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

        m_regs.P.C = (value & 0x80) != 0;

        uint8_t shifted_value = (value << 1) | incoming_carry;

        writeRamByte(operand, shifted_value);
        m_regs.P.Z = (shifted_value == 0);
        m_regs.P.N = (shifted_value & 0x80) != 0;
        return 6;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ROL zp,X */ {
        uint8_t addr = static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X);

        uint8_t value          = readRamByte(addr);
        uint8_t incoming_carry = m_regs.P.C ? 1 : 0;

        m_regs.P.C = (value & 0x80) != 0;

        uint8_t shifted_value = (value << 1) | incoming_carry;

        writeRamByte(addr, shifted_value);
        m_regs.P.Z = (shifted_value == 0);
        m_regs.P.N = (shifted_value & 0x80) != 0;
        return 6;
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x02: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* LSR zp */ {
        auto const operand = readPC<uint8_t>();

        auto orig = readRamByte(operand);
        auto wr   = writeRamByte(operand, orig >> 1);

        m_regs.P.N = 0;
        m_regs.P.Z = wr == 0;
        m_regs.P.C = (orig & 0x01) != 0;
        return 5;
      } else if (inst.getAddrMode() == 0x02) /* LSR A */ {
        m_regs.P.C = (m_regs.A & 0x01) != 0;
        m_regs.A >>= 1;
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* LSR abs */ {
        auto const operand = readPC<uint16_t>();

        auto orig = readRamByte(operand);
        auto wr   = writeRamByte(operand, orig >> 1);

        m_regs.P.N = 0;
        m_regs.P.Z = wr == 0;
        m_regs.P.C = (orig & 0x01) != 0;
        return 6;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x03: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* ??? */ {
      } else if (inst.getAddrMode() == 0x02) /* ROR A */ {
        uint8_t incoming_carry = m_regs.P.C ? 0x80 : 0x00;
        uint8_t shifted_value  = (m_regs.A >> 1) | incoming_carry;
        m_regs.P.C             = (m_regs.A & 0x01) != 0;

        m_regs.A   = shifted_value;
        m_regs.P.Z = (shifted_value == 0);
        m_regs.P.N = (shifted_value & 0x80) != 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* ??? */ {
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) /* ROR abs,X */ {
        auto const operand = readPC<uint16_t>();

        uint16_t target_addr = operand + m_regs.X;

        uint8_t value = readRamByte(target_addr);

        uint8_t incoming_carry = m_regs.P.C ? 0x80 : 0x00;
        uint8_t shifted_value  = (value >> 1) | incoming_carry;
        m_regs.P.C             = (value & 0x01) != 0;

        writeRamByte(target_addr, shifted_value);
        m_regs.P.Z = (shifted_value == 0);
        m_regs.P.N = (shifted_value & 0x80) != 0;
        return 7;
      }
    } break;
    case 0x04: { // Transfers
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* STX zp */ {
        writeRamByte(readPC<uint8_t>(), m_regs.X);
        return 3;
      } else if (inst.getAddrMode() == 0x02) /* TXA */ {
        m_regs.A   = m_regs.X;
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = (m_regs.A & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* STX abs */ {
        writeRamByte(readPC<uint16_t>(), m_regs.X);
        return 4;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* ??? */ {
      } else if (inst.getAddrMode() == 0x06) /* TXS */ {
        m_regs.SP = m_regs.X;
        return 2;
      } else if (inst.getAddrMode() == 0x07) /* ??? */ {
      }
    } break;
    case 0x05: {
      uint8_t value = 0, cycles = 1;

      if (inst.getAddrMode() == 0x00) /* LDX imm */ {
        value = readPC<uint8_t>();
        cycles += 1;
      } else if (inst.getAddrMode() == 0x01) /* LDX zp */ {
        value = readRamByte(readPC<uint8_t>());
        cycles += 2;
      } else if (inst.getAddrMode() == 0x02) /* TAX */ {
        value = m_regs.A;
        cycles += 1;
      } else if (inst.getAddrMode() == 0x03) /* LDX abs */ {
        value = readRamByte(readPC<uint16_t>());
        cycles += 3;
      } else if (inst.getAddrMode() == 0x04) /* JAM */ {
        throw 1;
      } else if (inst.getAddrMode() == 0x05) /* LDX zp,Y */ {
        value = readRamByte(static_cast<uint8_t>(readPC<uint8_t>() + m_regs.Y));
        cycles += 3;
      } else if (inst.getAddrMode() == 0x06) /* TSX */ {
        value = m_regs.SP;
        cycles += 1;
      } else if (inst.getAddrMode() == 0x07) /* LDX abs,Y */ {
        auto const operand = readPC<uint16_t>();

        auto target_addr = operand + m_regs.Y;

        value = readRamByte(target_addr);
        cycles += 3;
        if ((operand & 0xFF00) != (target_addr & 0xFF00)) {
          cycles += 1;
        }
      } else {
        throw UnhandledInstruction(inst.raw);
      }

      m_regs.X   = value;
      m_regs.P.Z = m_regs.X == 0;
      m_regs.P.N = (m_regs.X & 0x80) > 0;
      return cycles;
    } break;
    case 0x06: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* DEC zp */ {
        auto const operand = readPC<uint8_t>();

        auto const res = writeRamByte(operand, readRamByte(operand) - 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 5;
      } else if (inst.getAddrMode() == 0x02) /* DEX */ {
        m_regs.X -= 1;
        m_regs.P.Z = m_regs.X == 0;
        m_regs.P.N = (m_regs.X & 0x80) > 0;
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* DEC abs */ {
        auto const operand = readPC<uint16_t>();

        auto const res = writeRamByte(operand, readRamByte(operand) - 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 6;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* DEC zp,X */ {
        auto const addr = static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X);
        auto const res  = writeRamByte(addr, readRamByte(addr) - 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 7;
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) /* DEC abs,X */ {
        auto const addr = readPC<uint16_t>() + m_regs.X;
        auto const res  = writeRamByte(addr, readRamByte(addr) - 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 7;
      }
    } break;
    case 0x07: {
      if (inst.getAddrMode() == 0x00) /* ??? */ {
      } else if (inst.getAddrMode() == 0x01) /* INC zp */ {
        auto const operand = readPC<uint8_t>();

        auto const res = writeRamByte(operand, readRamByte(operand) + 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 5;
      } else if (inst.getAddrMode() == 0x02) /* NOP */ {
        return 2;
      } else if (inst.getAddrMode() == 0x03) /* INC abs */ {
        auto const operand = readPC<uint16_t>();

        auto const res = writeRamByte(operand, readRamByte(operand) + 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 6;
      } else if (inst.getAddrMode() == 0x04) /* ??? */ {
      } else if (inst.getAddrMode() == 0x05) /* INC zp,X */ {
        auto const addr = static_cast<uint8_t>(readPC<uint8_t>() + m_regs.X);

        auto const res = writeRamByte(addr, readRamByte(addr) + 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 5;
      } else if (inst.getAddrMode() == 0x06) /* ??? */ {
      } else if (inst.getAddrMode() == 0x07) /* INC abs,X */ {
        auto const addr = readPC<uint16_t>() + m_regs.X;

        auto const res = writeRamByte(addr, readRamByte(addr) + 1);

        m_regs.P.Z = res == 0;
        m_regs.P.N = (res & 0x80) > 0;
        return 7;
      }
    } break;
  }

  throw UnhandledInstruction(inst.raw);
}

uint8_t CPU6502::step() {
  if (m_nmitriggered) {
    m_nmitriggered = false;

    pushStack<uint16_t>(m_regs.PC);

    Status _p = m_regs.P;
    _p.B = 0, _p.U = 1;
    pushStack<uint8_t>(_p._raw);

    m_regs.P.I = 1;
    m_regs.PC  = readRam<uint16_t>(0xFFFA);
    return 7;
  }

  Instruction inst(readPC<uint8_t>());
  switch (inst.deflt.instclass) {
    case InstClass::Control: return handleControl(inst);
    case InstClass::Math: return handleMath(inst);
    case InstClass::Shift: return handleShift(inst);
    default: throw UnhandledInstruction(inst.raw);
  }
}

void CPU6502::triggerNmi() {
  m_nmitriggered = true;
}

uint8_t CPU6502::writeRamByte(uint16_t addr, uint8_t value) {
  if (auto handler = findHandler(addr); isValidHandler(handler)) {
    return handler->second(true, addr, value);
  }
  return m_ram[addr] = value;
}

uint8_t CPU6502::readRamByte(uint16_t addr) const {
  if (auto handler = findHandler(addr); isValidHandler(handler)) {
    return handler->second(false, addr, 0);
  }
  return m_ram[addr];
}
