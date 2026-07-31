#include "cpu.hh"

#include <cstdint>
#include <format>
#include <iterator>
#include <magic_enum/magic_enum.hpp>

#if PENES_MICROPROFILE
#include <microprofile.h>
#endif

class SkipInstruction {
  public:
  SkipInstruction() = default;
};

class HaltExecution {
  uint16_t const m_line;

  public:
  HaltExecution(uint16_t l /* ALWAYS make sure the line number takes less space than N bits specified in CPUState::haltLine */): m_line(l) {}

  uint16_t getLine() const { return m_line; }
};

std::string CPU6502::InstructionStatus::buildMnemonic(bool withAddr) const {
  std::string temp;
  temp.reserve(withAddr ? 16 : 10);

  if (withAddr) std::format_to(std::back_inserter(temp), "${:X} ", startAddr);

  temp.append(magic_enum::enum_name(flags.mnemonic));
  temp.push_back(' ');

  switch (flags.addrMode) {
    case AddrMode::Accum: temp.push_back('A'); break;
    case AddrMode::Absolute: std::format_to(std::back_inserter(temp), "a${:04X}", operand.u16); break;
    case AddrMode::AbsoluteX: std::format_to(std::back_inserter(temp), "a${:04X},X", operand.u16); break;
    case AddrMode::AbsoluteY: std::format_to(std::back_inserter(temp), "a${:04X},Y", operand.u16); break;
    case AddrMode::Immediate: std::format_to(std::back_inserter(temp), "#${:02X}", operand.u8); break;
    case AddrMode::Implied: temp.append("impl"); break;
    case AddrMode::Indirect: std::format_to(std::back_inserter(temp), "(${:04X})", operand.u16); break;
    case AddrMode::IndexedXIndir: std::format_to(std::back_inserter(temp), "(${:02X},X)", operand.u8); break;
    case AddrMode::IndirIndexedY: std::format_to(std::back_inserter(temp), "(${:02X}),Y", operand.u8); break;
    case AddrMode::Relative: std::format_to(std::back_inserter(temp), "r${:02X}", operand.s8); break;
    case AddrMode::ZeroPage: std::format_to(std::back_inserter(temp), "z${:02X}", operand.u8); break;
    case AddrMode::ZeroPageX: std::format_to(std::back_inserter(temp), "z${:02X},X", operand.u8); break;
    case AddrMode::ZeroPageY: std::format_to(std::back_inserter(temp), "z${:02X},Y", operand.u8); break;
    default: temp.append("???"); break;
  }

  return temp;
}

void CPU6502::reset() {
  m_state.intrFlags = 0;
  m_state.regs.SP   = 0xFF;
  m_state.regs.PC   = 0x00;
  m_state.regs.SR.C = 0;
  m_state.regs.SR.Z = 0;
  m_state.regs.SR.D = 0;
  m_state.regs.SR.V = 0;
  m_state.regs.A    = 0;
  m_state.regs.X    = 0;
  m_state.regs.Y    = 0;
  interrupt(0xFFFC);
}

uint8_t CPU6502::interrupt(uint16_t vector, bool software) {
  pushStack<uint16_t>(m_state.regs.PC);
  if (vector != 0xFFFC /* Looks like reset vector should not push the status, idefk */) pushStatus(software);
  m_state.regs.SR.I = 1;
  m_state.regs.PC   = readMem<uint16_t>(vector);
  return 7;
}

uint8_t CPU6502::handleControl(InstructionStatus& status) {
  if (status.isBranching()) { // Handle branching
    bool flag_status = false;
    switch (status.getBranchingSelection()) {
      case 0b00 /* Negative */: {
        status << (status.getBranchingCondition() ? Mnemonic::BMI : Mnemonic::BPL);
        flag_status = m_state.regs.SR.N;
      } break;
      case 0b01 /* Overflow */: {
        status << (status.getBranchingCondition() ? Mnemonic::BVS : Mnemonic::BVC);
        flag_status = m_state.regs.SR.V;
      } break;
      case 0b10 /* Carry */: {
        status << (status.getBranchingCondition() ? Mnemonic::BCS : Mnemonic::BCC);
        flag_status = m_state.regs.SR.C;
      } break;
      case 0b11 /* Zero */: {
        status << (status.getBranchingCondition() ? Mnemonic::BEQ : Mnemonic::BNE);
        flag_status = m_state.regs.SR.Z;
      } break;
    }
    status << AddrMode::Relative;
    if (flag_status != status.getBranchingCondition()) return postExecHook(status, 2);

    uint16_t old_pc = m_state.regs.PC;
    m_state.regs.PC += status.operand.s8;
    return postExecHook(status, (old_pc & 0xFF00) == (m_state.regs.PC & 0xFF00) ? 3 : 4);
  }

  // Note: All missing 0x04's here are handled by branching block above
  switch (status.getOpCode()) {
    case 0x00: { // Control Flow
      switch (status.getAddrMode()) {
        case 0x00: /* BRK impl */ {
          status << Mnemonic::BRK << AddrMode::Implied;
          return postExecHook(status, interrupt(0xFFFE, true));
        } break;
        case 0x01: /* NOP zp (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPage; break;
        case 0x02: /* PHP impl */ {
          status << Mnemonic::PHP << AddrMode::Implied;
          pushStatus(true);
          return postExecHook(status, 3);
        } break;
        case 0x03: /* NOP abs (illegal) */ status << Mnemonic::NOP << AddrMode::Absolute; break;
        case 0x05: /* NOP zp,X (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPageX; break;
        case 0x06: /* CLC impl */ {
          status << Mnemonic::CLC << AddrMode::Implied;

          m_state.regs.SR.C = 0;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ status << Mnemonic::NOP << AddrMode::AbsoluteX; break;
      }
    } break;
    case 0x01: {
      switch (status.getAddrMode()) {
        case 0x00: /* JSR abs */ {
          status << Mnemonic::JSR << AddrMode::Absolute;

          pushStack<uint16_t>(m_state.regs.PC - 1);
          m_state.regs.PC = status.operand.u16;
          return postExecHook(status, 6);
        } break;
        case 0x01: /* BIT zp */ {
          status << Mnemonic::BIT << AddrMode::ZeroPage;

          auto const test   = readMemByte(evaluateOperandToAddr(status));
          m_state.regs.SR.Z = (m_state.regs.A & test) == 0;
          m_state.regs.SR.N = (test & 0x80) > 0;
          m_state.regs.SR.V = (test & 0x40) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* PLP impl */ {
          status << Mnemonic::PLP << AddrMode::Implied;

          m_state.regs.SR = popStatus();
          return postExecHook(status, 4);
        } break;
        case 0x03: /* BIT abs */ {
          status << Mnemonic::BIT << AddrMode::Absolute;

          auto const test   = readMemByte(evaluateOperandToAddr(status));
          m_state.regs.SR.Z = (m_state.regs.A & test) == 0;
          m_state.regs.SR.N = (test & 0x80) > 0;
          m_state.regs.SR.V = (test & 0x40) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zp,X (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPageX; break;
        case 0x06: /* SEC impl */ {
          status << Mnemonic::SEC << AddrMode::Implied;

          m_state.regs.SR.C = 1;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ status << Mnemonic::NOP << AddrMode::AbsoluteX; break;
      }
    } break;
    case 0x02: {
      switch (status.getAddrMode()) {
        case 0x00: /* RTI impl */ {
          status << Mnemonic::RTI << AddrMode::Implied;

          m_state.regs.SR._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
          m_state.regs.PC      = popStack<uint16_t>();
          return postExecHook(status, 6);
        } break;
        case 0x01: /* NOP zp (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPage; break;
        case 0x02: /* PHA impl */ {
          status << Mnemonic::PHA << AddrMode::Implied;

          pushStack<uint8_t>(m_state.regs.A);
          return postExecHook(status, 3);
        } break;
        case 0x03: /* JMP abs */ {
          status << Mnemonic::JMP << AddrMode::Absolute;

          m_state.regs.PC = status.operand.u16;
          return postExecHook(status, 3);
        } break;
        case 0x05: /* NOP zp,X (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPageX; break;
        case 0x06: /* CLI impl */ {
          status << Mnemonic::CLI << AddrMode::Implied;
          m_state.intrFlags |= INTRMASK_NOIRQ;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ status << Mnemonic::NOP << AddrMode::AbsoluteX; break;
      }
    } break;
    case 0x03: {
      switch (status.getAddrMode()) {
        case 0x00: /* RTS impl */ {
          status << Mnemonic::RTS << AddrMode::Implied;

          m_state.regs.PC = popStack<uint16_t>() + 1;
          return postExecHook(status, 6);
        } break;
        case 0x01: /* NOP zp (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPage;
          readMemByte(evaluateOperandToAddr(status)); // Dummy read
          return postExecHook(status, 3);
        } break;
        case 0x02: /* PLA impl */ {
          status << Mnemonic::PLA << AddrMode::Implied;

          m_state.regs.A    = popStack<uint8_t>();
          m_state.regs.SR.Z = m_state.regs.A == 0;
          m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x03: /* JMP (oper) */ {
          status << Mnemonic::JMP << AddrMode::Indirect;
          m_state.regs.PC = evaluateOperandToValue<uint16_t>(status).value;
          return postExecHook(status, 5);
        } break;
        case 0x05: /* NOP zp,X (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPageX; break;
        case 0x06: /* SEI impl */ {
          status << Mnemonic::SEI << AddrMode::Implied;

          m_state.regs.SR.I = true;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ status << Mnemonic::NOP << AddrMode::AbsoluteX; break;
      }
    } break;
    case 0x04: {
      switch (status.getAddrMode()) {
        case 0x00: /* NOP imm (illegal) */ {
          status << Mnemonic::NOP << AddrMode::Immediate;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* STY zp */ {
          status << Mnemonic::STY << AddrMode::ZeroPage;

          writeMemByte(evaluateOperandToAddr(status), m_state.regs.Y);
          return postExecHook(status, 3);
        } break;
        case 0x02: /* DEY impl */ {
          status << Mnemonic::DEY << AddrMode::Implied;

          --m_state.regs.Y;
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* STY abs */ {
          status << Mnemonic::STY << AddrMode::Absolute;
          writeMemByte(evaluateOperandToAddr(status), m_state.regs.Y);
          return postExecHook(status, 4);
        } break;
        case 0x05: /* STY zp,X */ {
          status << Mnemonic::STY << AddrMode::ZeroPageX;
          writeMemByte(evaluateOperandToAddr(status), m_state.regs.Y);
          return postExecHook(status, 4);
        } break;
        case 0x06: /* TYA impl */ {
          status << Mnemonic::TYA << AddrMode::Implied;

          m_state.regs.A    = m_state.regs.Y;
          m_state.regs.SR.Z = m_state.regs.A == 0;
          m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* SHY abs,X (illegal) */ {
          status << Mnemonic::SHY << AddrMode::AbsoluteX;
          writeMemByte(evaluateOperandToAddr(status), m_state.regs.Y & (((status.operand.u16 + m_state.regs.X) >> 8) + 1));
          return postExecHook(status, 5);
        } break;
      }
    } break;
    case 0x05: {
      switch (status.getAddrMode()) {
        case 0x00: /* LDY imm */ {
          status << Mnemonic::LDY << AddrMode::Immediate;

          m_state.regs.Y    = status.operand.u8;
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* LDY zp */ {
          status << Mnemonic::LDY << AddrMode::ZeroPage;

          m_state.regs.Y    = readMemByte(evaluateOperandToAddr(status));
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* TAY impl */ {
          status << Mnemonic::TAY << AddrMode::Implied;
          m_state.regs.Y    = m_state.regs.A;
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* LDY abs */ {
          status << Mnemonic::LDY << AddrMode::Absolute;

          m_state.regs.Y    = readMemByte(evaluateOperandToAddr(status));
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* LDY zp,X */ {
          status << Mnemonic::LDY << AddrMode::ZeroPageX;
          m_state.regs.Y    = readMemByte(evaluateOperandToAddr(status));
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x06: /* CLV impl */ {
          status << Mnemonic::CLV << AddrMode::Implied;
          m_state.regs.SR.V = 0;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* LDY abs,X */ {
          status << Mnemonic::LDY << AddrMode::AbsoluteX;
          auto const eval   = evaluateOperandToAddr(status);
          m_state.regs.Y    = readMemByte(eval);
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 3 + eval.cyclesTaken);
        } break;
      }
    } break;
    case 0x06: {
      switch (status.getAddrMode()) {
        case 0x00: /* CPY imm */ {
          status << Mnemonic::CPY << AddrMode::Immediate;

          uint8_t val = m_state.regs.Y - status.operand.u8;

          m_state.regs.SR.C = m_state.regs.Y >= status.operand.u8;
          m_state.regs.SR.Z = val == 0;
          m_state.regs.SR.N = (val & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* CPY zp */ {
          status << Mnemonic::CPY << AddrMode::ZeroPage;

          auto const    rval = readMemByte(evaluateOperandToAddr(status));
          uint8_t const val  = m_state.regs.Y - rval;

          m_state.regs.SR.C = m_state.regs.Y >= rval;
          m_state.regs.SR.Z = val == 0;
          m_state.regs.SR.N = (val & 0x80) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* INY impl */ {
          status << Mnemonic::INY << AddrMode::Implied;

          m_state.regs.Y += 1;
          m_state.regs.SR.Z = m_state.regs.Y == 0;
          m_state.regs.SR.N = (m_state.regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* CPY abs */ {
          status << Mnemonic::CPY << AddrMode::Absolute;

          auto const rval = readMemByte(evaluateOperandToAddr(status));

          uint8_t const val = m_state.regs.Y - rval;

          m_state.regs.SR.C = m_state.regs.Y >= rval;
          m_state.regs.SR.Z = val == 0;
          m_state.regs.SR.N = (val & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zp,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          readMemByte(evaluateOperandToAddr(status)); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* CLD impl */ {
          status << Mnemonic::CLD << AddrMode::Implied;
          m_state.regs.SR.D = false;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ status << Mnemonic::NOP << AddrMode::AbsoluteX; break;
      }
    } break;
    case 0x07: {
      switch (status.getAddrMode()) {
        case 0x00: /* CPX imm */ {
          status << Mnemonic::CPX << AddrMode::Immediate;

          uint8_t val = m_state.regs.X - status.operand.u8;

          m_state.regs.SR.C = m_state.regs.X >= status.operand.u8;
          m_state.regs.SR.Z = val == 0;
          m_state.regs.SR.N = (val & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* CPX zp */ {
          status << Mnemonic::CPX << AddrMode::ZeroPage;
          auto const eval = evaluateOperandToValue<uint8_t>(status);

          uint8_t const val = m_state.regs.X - eval.value;

          m_state.regs.SR.C = m_state.regs.X >= eval.value;
          m_state.regs.SR.Z = val == 0;
          m_state.regs.SR.N = (val & 0x80) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* INX impl */ {
          status << Mnemonic::INX << AddrMode::Implied;

          m_state.regs.X += 1;
          m_state.regs.SR.Z = m_state.regs.X == 0;
          m_state.regs.SR.N = (m_state.regs.X & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* CPX abs */ {
          status << Mnemonic::CPX << AddrMode::Absolute;
          auto const eval = evaluateOperandToValue<uint8_t>(status);

          uint8_t const val = m_state.regs.X - eval.value;

          m_state.regs.SR.C = m_state.regs.X >= eval.value;
          m_state.regs.SR.Z = val == 0;
          m_state.regs.SR.N = (val & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zp,X (illegal) */ status << Mnemonic::NOP << AddrMode::ZeroPageX; break;
        case 0x06: /* SED impl */ {
          status << Mnemonic::SED << AddrMode::Implied;

          m_state.regs.SR.D = 1;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ status << Mnemonic::NOP << AddrMode::AbsoluteX; break;
      }
    } break;
  }

  /* All control class NOPs fallthrough here */
  auto const eval = evaluateOperandToValue<uint8_t>(status);
  return postExecHook(status, 2 + eval.cyclesTaken);
}

uint8_t CPU6502::handleMath(InstructionStatus& status) {
  if (status.getRawInstructionByte() == 0x89) /* Special cse of NOP imm (illegal) */ {
    status << Mnemonic::NOP << AddrMode::Immediate;
    return postExecHook(status, 2);
  }

  auto const parse = [&]() {
    switch (status.getAddrMode()) {
      case 0b000 /* (Indir,X) */: status << AddrMode::IndexedXIndir; break;
      case 0b001 /* Zero Page */: status << AddrMode::ZeroPage; break;
      case 0b010 /* Immediate */: status << AddrMode::Immediate; break;
      case 0b011 /* Absolute */: status << AddrMode::Absolute; break;
      case 0b100 /* (Indir),Y*/: status << AddrMode::IndirIndexedY; break;
      case 0b101 /* Zero Page, X */: status << AddrMode::ZeroPageX; break;
      case 0b110 /* Absolute, Y */: status << AddrMode::AbsoluteY; break;
      case 0b111 /* Absolute, X */: status << AddrMode::AbsoluteX; break;
    }
  };

  // Note: preExecHook() is called inside resolve*() functions here
  switch (status.getOpCode()) {
    case 0x00: { // ORA
      status << Mnemonic::ORA;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      m_state.regs.A |= eval.value;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
    case 0x01: { // AND
      status << Mnemonic::AND;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      m_state.regs.A &= eval.value;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
    case 0x02: { // EOR
      status << Mnemonic::EOR;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      m_state.regs.A ^= eval.value;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
    case 0x03: { // ADC
      status << Mnemonic::ADC;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      uint16_t result   = m_state.regs.A + eval.value + m_state.regs.SR.C;
      m_state.regs.SR.C = result > 0xFF;
      m_state.regs.SR.V = (~(m_state.regs.A ^ eval.value) & (m_state.regs.A ^ result) & 0x80) != 0;
      m_state.regs.A    = result & 0xFF;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
    case 0x04: { // STA
      status << Mnemonic::STA;
      parse();

      auto eval = evaluateOperandToAddr(status);
      if (status.flags.addrMode == AddrMode::AbsoluteX) eval.cyclesTaken += 1;
      if (status.flags.addrMode == AddrMode::AbsoluteY) eval.cyclesTaken += 1;
      if (status.flags.addrMode == AddrMode::IndirIndexedY) eval.cyclesTaken += 1;

      writeMemByte(eval, m_state.regs.A);
      return postExecHook(status, 3 + eval.cyclesTaken);
    } break;
    case 0x05: { // LDA
      status << Mnemonic::LDA;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      m_state.regs.A    = eval.value;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
    case 0x06: { // CMP
      status << Mnemonic::CMP;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      uint8_t val       = m_state.regs.A - eval.value;
      m_state.regs.SR.C = m_state.regs.A >= eval.value;
      m_state.regs.SR.Z = val == 0;
      m_state.regs.SR.N = (val & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
    case 0x07: { // SBC
      status << Mnemonic::SBC;
      parse();

      auto const eval = evaluateOperandToValue<uint8_t>(status);

      uint8_t  inverted_value = eval.value ^ 0xFF;
      uint16_t carry_in       = m_state.regs.SR.C ? 1 : 0;
      uint16_t res            = m_state.regs.A + inverted_value + carry_in;

      m_state.regs.SR.V = ((m_state.regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_state.regs.SR.C = res > 0xFF;
      m_state.regs.A    = res & 0xFF;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      return postExecHook(status, 2 + eval.cyclesTaken);
    } break;
  }

  std::unreachable();
}

uint8_t CPU6502::handleShift(InstructionStatus& status) {
  switch (status.getOpCode()) {
    case 0x00: {
      status << Mnemonic::ASL;

      uint8_t origValue = 0, resultValue = 0, cycles = 0;
      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw HaltExecution(__LINE__);
        case 0x01: /* ASL zp */ status << AddrMode::ZeroPage; break;
        case 0x02: /* ASL A */ {
          status << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_state.regs.A;
          resultValue = (m_state.regs.A <<= 1);
          goto leave_early_ASL;
        } break;
        case 0x03: /* ASL abs */ status << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* ASL zp,X */ status << AddrMode::ZeroPageX; break;
        case 0x06: /* NOP impl (illegal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;
        case 0x07: /* ASL abs,X */ cycles += 1, status << AddrMode::AbsoluteX; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += 4 + eval.cyclesTaken;
        origValue   = eval.value;
        resultValue = writeMemByte(eval, origValue << 1);
      }

    leave_early_ASL:
      m_state.regs.SR.C = (origValue & 0x80) > 0;
      m_state.regs.SR.Z = (resultValue == 0);
      m_state.regs.SR.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x01: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      status << Mnemonic::ROL;
      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw HaltExecution(__LINE__);
        case 0x01: /* ROL zp */ status << AddrMode::ZeroPage; break;
        case 0x02: /* ROL A */ {
          status << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_state.regs.A;
          resultValue = (m_state.regs.A = (origValue << 1) | (m_state.regs.SR.C ? 1 : 0));
          goto leave_early_ROL;
        } break;
        case 0x03: /* ROL abs */ status << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* ROL zp,X */ status << AddrMode::ZeroPageX; break;
        case 0x06: /* NOP impl (illegal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;

        case 0x07: /* ROL abs,X */ cycles += 1, status << AddrMode::AbsoluteX; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += 4 + eval.cyclesTaken;
        origValue   = eval.value;
        resultValue = writeMemByte(eval, (origValue << 1) | (m_state.regs.SR.C ? 1 : 0));
      }

    leave_early_ROL:
      m_state.regs.SR.C = (origValue & 0x80) != 0;
      m_state.regs.SR.Z = (resultValue == 0);
      m_state.regs.SR.N = (resultValue & 0x80) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x02: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;
      status << Mnemonic::LSR;

      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw HaltExecution(__LINE__);
        case 0x01: /* LSR zp */ status << AddrMode::ZeroPage; break;
        case 0x02: /* LSR A */ {
          status << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_state.regs.A;
          resultValue = (m_state.regs.A >>= 1);
          goto leave_early_LSR;
        } break;
        case 0x03: /* LSR abs */ status << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* LSR zp,X */ status << AddrMode::ZeroPageX; break;
        case 0x06: /* NOP impl (illegal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;

        case 0x07: /* LSR abs,X */ cycles += 1, status << AddrMode::AbsoluteX; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += 4 + eval.cyclesTaken;
        origValue   = eval.value;
        resultValue = writeMemByte(eval, origValue >> 1);
      }

    leave_early_LSR:
      m_state.regs.SR.N = 0;
      m_state.regs.SR.Z = resultValue == 0;
      m_state.regs.SR.C = (origValue & 0x01) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x03: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw HaltExecution(__LINE__);
        case 0x01: /* ROR zp */ status << Mnemonic::ROR << AddrMode::ZeroPage; break;
        case 0x02: /* ROR A */ {
          status << Mnemonic::ROR << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_state.regs.A;
          resultValue = (m_state.regs.A = (origValue >> 1) | (m_state.regs.SR.C ? 0x80 : 0x00));
          goto leave_early_ROR;
        } break;
        case 0x03: /* ROR abs */ status << Mnemonic::ROR << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* ROR zp,X */ status << Mnemonic::ROR << AddrMode::ZeroPageX; break;
        case 0x06: /* NOP impl (illegal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;

        case 0x07: /* ROR abs,X */ cycles += 1, status << Mnemonic::ROR << AddrMode::AbsoluteX; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += 4 + eval.cyclesTaken;
        origValue   = eval.value;
        resultValue = writeMemByte(eval, (origValue >> 1) | (m_state.regs.SR.C ? 0x80 : 0x00));
      }

    leave_early_ROR:
      m_state.regs.SR.C = (origValue & 0x01) != 0;
      m_state.regs.SR.Z = (resultValue == 0);
      m_state.regs.SR.N = (resultValue & 0x80) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x04: {
      switch (status.getAddrMode()) {
        case 0x00: /* NOP imm (illegal) */ status << Mnemonic::NOP << AddrMode::Immediate; goto shift_NOPs;

        case 0x01: /* STX zp */ {
          status << Mnemonic::STX << AddrMode::ZeroPage;

          writeMemByte(evaluateOperandToAddr(status), m_state.regs.X);
          return postExecHook(status, 3);
        } break;
        case 0x02: /* TXA */ {
          status << Mnemonic::TXA << AddrMode::Implied;

          m_state.regs.A    = m_state.regs.X;
          m_state.regs.SR.Z = m_state.regs.A == 0;
          m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* STX abs */ {
          status << Mnemonic::STX << AddrMode::Absolute;

          writeMemByte(evaluateOperandToAddr(status), m_state.regs.X);
          return postExecHook(status, 4);
        } break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* STX zp,Y */ {
          status << Mnemonic::STX << AddrMode::ZeroPageY;
          auto const eval = evaluateOperandToAddr(status);

          writeMemByte(eval, m_state.regs.X);
          return postExecHook(status, 4);
        } break;
        case 0x06: /* TXS */ {
          status << Mnemonic::TXS << AddrMode::Implied;

          m_state.regs.SP = m_state.regs.X;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* SHX abs,Y (illegal) */ {
          status << Mnemonic::SHX << AddrMode::AbsoluteY;
          auto const eval = evaluateOperandToAddr(status);

          writeMemByte(eval, m_state.regs.X & ((eval.getAddress() >> 8) + 1));
          return postExecHook(status, 5);
        } break;
      }
    } break;
    case 0x05: {
      uint8_t value = 0, cycles = 2;

      switch (status.getAddrMode()) {
        case 0x00: /* LDX imm */ status << Mnemonic::LDX << AddrMode::Immediate; break;
        case 0x01: /* LDX zp */ status << Mnemonic::LDX << AddrMode::ZeroPage; break;
        case 0x02: /* TAX */ {
          status << Mnemonic::TAX << AddrMode::Implied;

          value = m_state.regs.A;
          goto leave_early_XSET;
        } break;
        case 0x03: /* LDX abs */ status << Mnemonic::LDX << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* LDX zp,Y */ status << Mnemonic::LDX << AddrMode::ZeroPageY; break;
        case 0x06: /* TSX */ {
          status << Mnemonic::TSX << AddrMode::Implied;

          value = m_state.regs.SP;
          goto leave_early_XSET;
        } break;
        case 0x07: /* LDX abs,Y */ status << Mnemonic::LDX << AddrMode::AbsoluteY; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += eval.cyclesTaken;
        value = eval.value;
      }

    leave_early_XSET:
      m_state.regs.X    = value;
      m_state.regs.SR.Z = m_state.regs.X == 0;
      m_state.regs.SR.N = (m_state.regs.X & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x06: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      status << Mnemonic::DEC;
      switch (status.getAddrMode()) {
        case 0x00: /*  NOP imm (illegal) */ status << Mnemonic::NOP << AddrMode::Immediate; goto shift_NOPs;

        case 0x01: /* DEC zp */ status << AddrMode::ZeroPage; break;
        case 0x02: /* DEX */ {
          status << Mnemonic::DEX << AddrMode::Implied;

          cycles      = 2;
          origValue   = m_state.regs.X;
          resultValue = (m_state.regs.X -= 1);
          goto leave_early_DEC;
        } break;
        case 0x03: /* DEC abs */ status << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* DEC zp,X */ status << AddrMode::ZeroPageX; break;
        case 0x06: /* NOP impl (illegal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;
        case 0x07: /* DEC abs,X */ cycles += 1, status << AddrMode::AbsoluteX; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += 4 + eval.cyclesTaken;
        origValue   = eval.value;
        resultValue = writeMemByte(eval, origValue - 1);
      }

    leave_early_DEC:
      m_state.regs.SR.Z = resultValue == 0;
      m_state.regs.SR.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x07: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      status << Mnemonic::INC;
      switch (status.getAddrMode()) {
        case 0x00: /* NOP imm (illegal) */ status << Mnemonic::NOP << AddrMode::Immediate; goto shift_NOPs;
        case 0x01: /* INC zp */ status << AddrMode::ZeroPage; break;
        case 0x02: /* NOP impl (legal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;
        case 0x03: /* INC abs */ status << AddrMode::Absolute; break;
        case 0x04: /* JAM */ throw HaltExecution(__LINE__);
        case 0x05: /* INC zp,X */ status << AddrMode::ZeroPageX; break;
        case 0x06: /* NOP impl (illegal) */ status << Mnemonic::NOP << AddrMode::Implied; goto shift_NOPs;
        case 0x07: /* INC abs,X */ cycles += 1, status << AddrMode::AbsoluteX; break;
      }

      {
        auto const eval = evaluateOperandToValue<uint8_t>(status);

        cycles += 4 + eval.cyclesTaken;
        origValue   = eval.value;
        resultValue = writeMemByte(eval, origValue + 1);
      }

      m_state.regs.SR.Z = resultValue == 0;
      m_state.regs.SR.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
  }

shift_NOPs:
  /* All control class NOPs fallthrough here */
  uint8_t cyclesTaken = 2;
  if (status.flags.addrMode != AddrMode::Implied) {
    auto const eval = evaluateOperandToValue<uint8_t>(status);
    cyclesTaken += eval.cyclesTaken;
  }
  return postExecHook(status, cyclesTaken);
}

uint8_t CPU6502::handleIllegal(InstructionStatus& status) { /* All instructions below are illegal */
  const auto addrMode = [](uint8_t opc, uint8_t am) -> AddrMode {
    static AddrMode addrModes[] = {AddrMode::IndexedXIndir, AddrMode::ZeroPage,  AddrMode::Immediate, AddrMode::Absolute,
                                   AddrMode::IndirIndexedY, AddrMode::ZeroPageX, AddrMode::AbsoluteY, AddrMode::AbsoluteX};

    if ((opc >= 0x04 && opc <= 0x05) && am == 0b101) return AddrMode::ZeroPageY;
    if ((opc >= 0x05 && opc <= 0x05) && am == 0b111) return AddrMode::AbsoluteY;
    if (opc == 0x04 && am == 0b111) return AddrMode::AbsoluteY;
    return addrModes[am];
  };

  switch (status.getOpCode()) {
    case 0x00: {
      status << (status.getAddrMode() == 0b10 ? Mnemonic::ANC : Mnemonic::SLO) << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x01: {
      status << (status.getAddrMode() == 0b10 ? Mnemonic::ANC : Mnemonic::RLA) << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x02: {
      status << (status.getAddrMode() == 0b10 ? Mnemonic::ASR : Mnemonic::SRE) << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x03: {
      status << (status.getAddrMode() == 0b10 ? Mnemonic::ARR : Mnemonic::RRA) << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x04: {
      static Mnemonic mnemonics[] = {Mnemonic::SAX, Mnemonic::SAX, Mnemonic::ANE, Mnemonic::SAX, Mnemonic::SHA, Mnemonic::SAX, Mnemonic::SHS, Mnemonic::SHA};
      status << mnemonics[status.getAddrMode()] << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x05: {
      static Mnemonic mnemonics[] = {Mnemonic::LAX, Mnemonic::LAX, Mnemonic::LXA, Mnemonic::LAX, Mnemonic::LAX, Mnemonic::LAX, Mnemonic::LAE, Mnemonic::LAX};
      status << mnemonics[status.getAddrMode()] << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x06: {
      status << (status.getAddrMode() == 0b10 ? Mnemonic::AXS : Mnemonic::DCP) << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
    case 0x07: {
      status << (status.getAddrMode() == 0b10 ? Mnemonic::SBC : Mnemonic::ISC) << addrMode(status.getOpCode(), status.getAddrMode());
    } break;
  }

  // TODO for me from the future: explore possibilities of calling actual instruction handlers above from here to combine them (less code reuse)

  uint8_t cycles = 2 /* TODO calculate those */, origValue = 0, resultValue = 0;
  switch (status.flags.mnemonic) {
    case Mnemonic::SLO: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      /* ASL */
      origValue   = eval.value;
      resultValue = writeMemByte(eval, origValue << 1);

      /* ORA */
      m_state.regs.A |= resultValue;

      m_state.regs.SR.C = (origValue & 0x80) > 0;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
    } break;
    case Mnemonic::RLA: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      /* ROL */
      origValue   = eval.value;
      resultValue = writeMemByte(eval, (origValue << 1) | (m_state.regs.SR.C ? 1 : 0));

      /* AND */
      m_state.regs.A &= resultValue;

      m_state.regs.SR.C = (origValue & 0x80) != 0;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
    } break;
    case Mnemonic::SRE: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      /* LSR */
      origValue   = eval.value;
      resultValue = writeMemByte(eval, origValue >> 1);

      /* EOR */
      m_state.regs.A ^= resultValue;

      m_state.regs.SR.C = (origValue & 0x01) != 0;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
    } break;
    case Mnemonic::RRA: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      /* ROR */
      origValue         = eval.value;
      resultValue       = writeMemByte(eval, (origValue >> 1) | (m_state.regs.SR.C ? 0x80 : 0x00));
      m_state.regs.SR.C = (origValue & 0x01) != 0;

      /* ADC */
      uint16_t result = m_state.regs.A + resultValue + m_state.regs.SR.C;

      m_state.regs.SR.V = (~(m_state.regs.A ^ resultValue) & (m_state.regs.A ^ result) & 0x80) != 0;
      m_state.regs.SR.C = result > 0xFF;
      m_state.regs.A    = result & 0xFF;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
    } break;
    case Mnemonic::SAX: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);
      writeMemByte(eval, m_state.regs.A & m_state.regs.X);
    } break;
    case Mnemonic::LAX: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      m_state.regs.X = m_state.regs.A = eval.value;
      m_state.regs.SR.Z               = m_state.regs.X == 0;
      m_state.regs.SR.N               = (m_state.regs.X & 0x80) > 0;
    } break;
    case Mnemonic::DCP: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      /* DEC */
      auto const decRes = writeMemByte(eval, eval.value - 1);

      /* CMP */
      uint8_t val       = m_state.regs.A - decRes;
      m_state.regs.SR.C = m_state.regs.A >= decRes;
      m_state.regs.SR.Z = val == 0;
      m_state.regs.SR.N = (val & 0x80) > 0;
    } break;
    case Mnemonic::ISC: {
      auto const eval = evaluateOperandToValue<uint8_t>(status);

      /* INC */
      auto const incRes = writeMemByte(eval, eval.value + 1);

      /* SBC */
      uint8_t const  inverted_value = incRes ^ 0xFF;
      uint16_t const res            = m_state.regs.A + inverted_value + (m_state.regs.SR.C ? 1 : 0);

      m_state.regs.SR.V = ((m_state.regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_state.regs.SR.C = res > 0xFF;
      m_state.regs.A    = res & 0xFF;
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
    } break;
    case Mnemonic::ANC: {
      m_state.regs.A &= status.operand.u8;
      m_state.regs.SR.C = (m_state.regs.A & 0x80) > 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
      m_state.regs.SR.Z = m_state.regs.A == 0;
    } break;
    case Mnemonic::ASR: {
      m_state.regs.A &= status.operand.u8;
      m_state.regs.SR.C = (m_state.regs.A & 0x01) != 0;
      m_state.regs.A >>= 1;
      m_state.regs.SR.Z = (m_state.regs.A == 0);
      m_state.regs.SR.N = (m_state.regs.A & 0x80) != 0;
    } break;
    case Mnemonic::ARR: {
      m_state.regs.A &= status.operand.u8;

      m_state.regs.A    = (m_state.regs.A >> 1) | (m_state.regs.SR.C ? 0x80 : 0x00);
      m_state.regs.SR.C = (m_state.regs.A & 0x40) != 0;
      m_state.regs.SR.V = ((m_state.regs.A >> 6) ^ (m_state.regs.A >> 5)) & 0x01;
      m_state.regs.SR.Z = (m_state.regs.A == 0);
      m_state.regs.SR.N = (m_state.regs.A & 0x80) != 0;
    } break;
    case Mnemonic::ANE: {
      m_state.regs.A    = (m_state.regs.A | 0xee) & (m_state.regs.X & status.operand.u8);
      m_state.regs.SR.Z = m_state.regs.A == 0;
      m_state.regs.SR.N = (m_state.regs.A & 0x80) > 0;
    } break;
    case Mnemonic::LXA: {
      m_state.regs.A    = (m_state.regs.A | 0xee) & status.operand.u8;
      m_state.regs.X    = m_state.regs.A;
      m_state.regs.SR.Z = (m_state.regs.X == 0);
      m_state.regs.SR.N = (m_state.regs.X & 0x80) != 0;
    } break;
    case Mnemonic::AXS: {
      uint8_t const res = m_state.regs.A & m_state.regs.X;

      m_state.regs.SR.C = (res >= status.operand.u8);
      m_state.regs.X    = res - status.operand.u8;
      m_state.regs.SR.Z = (m_state.regs.X == 0);
      m_state.regs.SR.N = (m_state.regs.X & 0x80) != 0;
    } break;
    case Mnemonic::SBC: {
      uint16_t const value = (uint16_t)m_state.regs.A - (uint16_t)status.operand.u8 - (uint16_t)(m_state.regs.SR.C ? 0 : 1);

      m_state.regs.SR.C = (value < 0x100);
      m_state.regs.SR.V = ((m_state.regs.A ^ (uint8_t)value) & (~status.operand.u8 ^ (uint8_t)value) & 0x80) != 0;
      m_state.regs.A    = (uint8_t)value;
      m_state.regs.SR.Z = (m_state.regs.A == 0);
      m_state.regs.SR.N = (m_state.regs.A & 0x80) != 0;
    } break;

    default: break;
  }

  return postExecHook(status, cycles);
}

uint8_t CPU6502::step() {
#if PENES_MICROPROFILE
  MICROPROFILE_SCOPEI("NES", "CPU Step", MP_DARKGREY);
#endif
  if (m_state.intrFlags & INTRMASK_CPUHALT) return 0;
  if (m_brkpt.type == CPUBreak::Type::Exec && m_brkpt.address == m_state.regs.PC) m_brkpt.func();
  if (m_state.intrFlags & INTRMASK_NMI) {
    m_state.intrFlags &= ~INTRMASK_NMI;
    return interrupt(0xFFFA, false);
  }
  if (m_state.intrFlags & INTRMASK_NOIRQ) {
    m_state.intrFlags &= ~INTRMASK_NOIRQ;
    m_state.regs.SR.I = 0;
  } else if (m_state.regs.SR.I == 0 && m_state.intrFlags & INTRMASK_IRQ) {
    m_state.intrFlags &= ~INTRMASK_IRQ;
    return interrupt(0xFFFE, false);
  }

  InstructionStatus s {
      .owner     = *this,
      .startAddr = m_state.regs.PC,
      .holder    = readPC<uint8_t>(),
      .operand   = {},
      .flags     = {},
  };

  try {
    if (m_hook) m_hook(s);
    switch (s.getClass()) {
      case InstClass::Control: return handleControl(s); break;
      case InstClass::Math: return handleMath(s); break;
      case InstClass::Shift: return handleShift(s); break;
      case InstClass::Unknown: return handleIllegal(s);
    }
  } catch (SkipInstruction const& ex) {
    s.flags.stage = ExecStage::SkipExec;
    if (m_hook) m_hook(s);
    return 0;
  } catch (HaltExecution const& ex) {
    m_state.intrFlags |= INTRMASK_CPUHALT;
    m_state.haltLine = ex.getLine();
    return 0;
  }
}

void CPU6502::setInterrupt(uint8_t mask, uint8_t level) {
  m_state.intrFlags = (m_state.intrFlags & ~mask) | level;
}

uint8_t CPU6502::writeMemByte(EvalAddress const& eval, uint8_t value) {
  if (!eval.validAddr) throw HaltExecution(__LINE__);
  auto const addr = eval.getAddress();
  if (m_brkpt.type == CPUBreak::Type::Write && m_brkpt.address == addr) m_brkpt.func();
  if (addr <= 0x1FFF) return m_state.ram[addr & 0x7ff] = value;

  if (auto const han = findHandler(addr)) {
    if (auto const ret = (*han)(true, addr, value); ret.has_value()) return ret.value();
  }

  return value;
}

uint8_t CPU6502::readMemByte(EvalAddress const& eval) const {
  if (!eval.validAddr) throw HaltExecution(__LINE__);
  auto const addr = eval.getAddress();
  if (m_brkpt.type == CPUBreak::Type::Read && m_brkpt.address == addr) m_brkpt.func();
  if (addr <= 0x1FFF) return m_state.ram[addr & 0x7ff];

  if (auto const han = findHandler(addr)) {
    if (auto const ret = (*han)(false, addr, 0); ret.has_value()) return ret.value();
  }

  return (eval.baseAddress >> 8); // 100thCoin's open bus accuracy test 2+3. Not sure if I want to get any further into this rabbit hole
}

void CPU6502::preExecHook(InstructionStatus& status) {
  if (status.flags.addrMode == AddrMode::Invalid) throw HaltExecution(__LINE__);
  status.flags.stage = ExecStage::PreExec;
  if (m_hook) m_hook(status);
  if (status.flags.skipExec) throw SkipInstruction();
}

uint8_t CPU6502::postExecHook(InstructionStatus& status, uint8_t cycles) {
  status.flags.stage = ExecStage::PostExec, status.flags.cycles += cycles;
  if (m_hook) m_hook(status);
  return status.flags.cycles;
}
