#include "cpu.hh"

#include <cstdint>
#include <exception>
#include <format>
#include <iterator>

class UnhandledInstruction: public std::exception {
  public:
  UnhandledInstruction() {}

  const char* what() const noexcept override { return "Attempt to execute an unknown instruction"; }
};

class SkipInstruction: public std::exception {
  public:
  SkipInstruction() {}
};

CPU6502::CPU6502(): MMU() {}

CPU6502::~CPU6502() {}

std::string CPU6502::InstructionStatus::buildMnemonic(bool withAddr) const {
  std::string temp;
  temp.reserve(withAddr ? 16 : 10);

  if (withAddr) std::format_to(std::back_inserter(temp), "${:X} ", startAddr);

  switch (flags.mnemonic) {
    case Mnemonic::ADC: temp.append("ADC"); break;
    case Mnemonic::AND: temp.append("AND"); break;
    case Mnemonic::ASL: temp.append("ASL"); break;
    case Mnemonic::BCC: temp.append("BCC"); break;
    case Mnemonic::BCS: temp.append("BCS"); break;
    case Mnemonic::BEQ: temp.append("BEQ"); break;
    case Mnemonic::BIT: temp.append("BIT"); break;
    case Mnemonic::BMI: temp.append("BMI"); break;
    case Mnemonic::BNE: temp.append("BNE"); break;
    case Mnemonic::BPL: temp.append("BPL"); break;
    case Mnemonic::BRK: temp.append("BRK"); break;
    case Mnemonic::BVC: temp.append("BVC"); break;
    case Mnemonic::BVS: temp.append("BVS"); break;
    case Mnemonic::CLC: temp.append("CLC"); break;
    case Mnemonic::CLD: temp.append("CLD"); break;
    case Mnemonic::CLI: temp.append("CLI"); break;
    case Mnemonic::CLV: temp.append("CLV"); break;
    case Mnemonic::CMP: temp.append("CMP"); break;
    case Mnemonic::CPX: temp.append("CPX"); break;
    case Mnemonic::CPY: temp.append("CPY"); break;
    case Mnemonic::DEC: temp.append("DEC"); break;
    case Mnemonic::DEX: temp.append("DEX"); break;
    case Mnemonic::DEY: temp.append("DEY"); break;
    case Mnemonic::EOR: temp.append("EOR"); break;
    case Mnemonic::INC: temp.append("INC"); break;
    case Mnemonic::INX: temp.append("INX"); break;
    case Mnemonic::INY: temp.append("INY"); break;
    case Mnemonic::JMP: temp.append("JMP"); break;
    case Mnemonic::JSR: temp.append("JSR"); break;
    case Mnemonic::LDA: temp.append("LDA"); break;
    case Mnemonic::LDX: temp.append("LDX"); break;
    case Mnemonic::LDY: temp.append("LDY"); break;
    case Mnemonic::LSR: temp.append("LSR"); break;
    case Mnemonic::NOP: temp.append("NOP"); break;
    case Mnemonic::ORA: temp.append("ORA"); break;
    case Mnemonic::PHA: temp.append("PHA"); break;
    case Mnemonic::PHP: temp.append("PHP"); break;
    case Mnemonic::PLA: temp.append("PLA"); break;
    case Mnemonic::PLP: temp.append("PLP"); break;
    case Mnemonic::ROL: temp.append("ROL"); break;
    case Mnemonic::ROR: temp.append("ROR"); break;
    case Mnemonic::RTI: temp.append("RTI"); break;
    case Mnemonic::RTS: temp.append("RTS"); break;
    case Mnemonic::SBC: temp.append("SBC"); break;
    case Mnemonic::SEC: temp.append("SEC"); break;
    case Mnemonic::SED: temp.append("SED"); break;
    case Mnemonic::SEI: temp.append("SEI"); break;
    case Mnemonic::STA: temp.append("STA"); break;
    case Mnemonic::STX: temp.append("STX"); break;
    case Mnemonic::STY: temp.append("STY"); break;
    case Mnemonic::TAX: temp.append("TAX"); break;
    case Mnemonic::TAY: temp.append("TAY"); break;
    case Mnemonic::TSX: temp.append("TSX"); break;
    case Mnemonic::TXA: temp.append("TXA"); break;
    case Mnemonic::TXS: temp.append("TXS"); break;
    case Mnemonic::TYA: temp.append("TYA"); break;

    case Mnemonic::SLO: temp.append("SLO"); break;
    case Mnemonic::RLA: temp.append("RLA"); break;
    case Mnemonic::SRE: temp.append("SRE"); break;
    case Mnemonic::RRA: temp.append("RRA"); break;
    case Mnemonic::SAX: temp.append("SAX"); break;
    case Mnemonic::LAX: temp.append("LAX"); break;
    case Mnemonic::DCP: temp.append("DCP"); break;
    case Mnemonic::ISC: temp.append("ISC"); break;
    case Mnemonic::SHA: temp.append("SHA"); break;
    case Mnemonic::SHS: temp.append("SHS"); break;
    case Mnemonic::SHY: temp.append("SHY"); break;
    case Mnemonic::SHX: temp.append("SHX"); break;
    case Mnemonic::LAE: temp.append("LAE"); break;
    case Mnemonic::ANC: temp.append("ANC"); break;
    case Mnemonic::ASR: temp.append("ASR"); break;
    case Mnemonic::ARR: temp.append("ARR"); break;
    case Mnemonic::ANE: temp.append("ANE"); break;
    case Mnemonic::LXA: temp.append("LXA"); break;
    case Mnemonic::AXS: temp.append("AXS"); break;

    default: std::format_to(std::back_inserter(temp), "${:02X}", holder.raw); break;
  }

  temp.push_back(' ');

  switch (flags.addrMode) {
    case AddrMode::Accum: temp.push_back('A');
    case AddrMode::Absolute: std::format_to(std::back_inserter(temp), "a${:X}", operand.u16); break;
    case AddrMode::AbsoluteX: std::format_to(std::back_inserter(temp), "a${:X},X", operand.u16); break;
    case AddrMode::AbsoluteY: std::format_to(std::back_inserter(temp), "a${:X},Y", operand.u16); break;
    case AddrMode::Immediate: std::format_to(std::back_inserter(temp), "#${:X}", operand.u8); break;
    case AddrMode::Implied: temp.append("impl"); break;
    case AddrMode::Indirect: std::format_to(std::back_inserter(temp), "(${:X})", operand.u16); break;
    case AddrMode::IndexedXIndir: std::format_to(std::back_inserter(temp), "(${:X},X)", operand.u8); break;
    case AddrMode::IndirIndexedY: std::format_to(std::back_inserter(temp), "(${:X}),Y", operand.u8); break;
    case AddrMode::Relative: std::format_to(std::back_inserter(temp), "r${:X}", operand.s8); break;
    case AddrMode::ZeroPage: std::format_to(std::back_inserter(temp), "z${:X}", operand.u8); break;
    case AddrMode::ZeroPageX: std::format_to(std::back_inserter(temp), "z${:X},X", operand.u8); break;
    case AddrMode::ZeroPageY: std::format_to(std::back_inserter(temp), "z${:X},Y", operand.u8); break;
    default: temp.append("???"); break;
  }

  return temp;
}

void CPU6502::reset() {
  m_nmiTriggered = false;
  m_irqTriggered = false;
  m_intrClrSchd  = false;
  m_regs.P.C     = 1;
  m_regs.P.I     = 1;
  m_regs.P.D     = 0;
  m_regs.A       = 0;
  m_regs.X       = 0;
  m_regs.Y       = 0;
  m_regs.SP      = 0xFF;
  m_regs.PC      = readRam<uint16_t>(0xFFFC);
}

uint8_t CPU6502::interrupt(uint16_t vector, bool software) {
  pushStack<uint16_t>(m_regs.PC);
  m_regs.P.I = 1;

  pushStatus(software);
  m_regs.PC = readRam<uint16_t>(vector);
  return 7;
}

uint8_t CPU6502::handleControl(InstructionStatus& status) {
  if (status.isBranching()) { // Handle branching
    bool flag_status = false;
    switch (status.getBranchingSelection()) {
      case 0b00 /* Negative */: {
        status << (status.getBranchingCondition() ? Mnemonic::BMI : Mnemonic::BPL);
        flag_status = m_regs.P.N;
      } break;
      case 0b01 /* Overflow */: {
        status << (status.getBranchingCondition() ? Mnemonic::BVS : Mnemonic::BVC);
        flag_status = m_regs.P.V;
      } break;
      case 0b10 /* Carry */: {
        status << (status.getBranchingCondition() ? Mnemonic::BCS : Mnemonic::BCC);
        flag_status = m_regs.P.C;
      } break;
      case 0b11 /* Zero */: {
        status << (status.getBranchingCondition() ? Mnemonic::BEQ : Mnemonic::BNE);
        flag_status = m_regs.P.Z;
      } break;
    }
    status << AddrMode::Relative;

    if (flag_status != status.getBranchingCondition()) return postExecHook(status, 2);

    uint16_t old_pc = m_regs.PC;
    m_regs.PC += status.operand.s8;
    return postExecHook(status, (old_pc & 0xFF00) == (m_regs.PC & 0xFF00) ? 3 : 4);
  }

  // Note: All missing 0x04's here are handled by branching block above
  switch (status.getOpCode()) {
    case 0x00: { // Control Flow
      switch (status.getAddrMode()) {
        case 0x00: /* BRK */ {
          status << Mnemonic::BRK;
          return postExecHook(status, interrupt(0xFFFE, true));
        } break;
        case 0x01: /* NOP zp (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPage;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* PHP */ {
          status << Mnemonic::PHP;
          pushStatus(true);
          return postExecHook(status, 3);
        } break;
        case 0x03: /* NOP abs (illegal) */ {
          status << Mnemonic::NOP << AddrMode::Absolute;
          readRam<uint16_t>(status.operand.u16); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zpg,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* CLC */ {
          status << Mnemonic::CLC;

          m_regs.P.C = 0;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::AbsoluteX;
          readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
          return postExecHook(status, 4);
        } break;
      }
    } break;
    case 0x01: {
      switch (status.getAddrMode()) {
        case 0x00: /* JSR abs */ {
          status << Mnemonic::JSR << AddrMode::Absolute;

          pushStack<uint16_t>(m_regs.PC - 1);
          m_regs.PC = status.operand.u16;
          return postExecHook(status, 6);
        } break;
        case 0x01: /* BIT zp */ {
          status << Mnemonic::BIT << AddrMode::ZeroPage;

          auto const test = readRamByte(status.operand.u8);
          m_regs.P.Z      = (m_regs.A & test) == 0;
          m_regs.P.N      = (test & 0x80) > 0;
          m_regs.P.V      = (test & 0x40) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* PLP */ {
          status << Mnemonic::PLP;

          m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
          return postExecHook(status, 4);
        } break;
        case 0x03: /* BIT abs */ {
          status << Mnemonic::BIT << AddrMode::Absolute;

          auto test  = readRamByte(status.operand.u16);
          m_regs.P.Z = (m_regs.A & test) == 0;
          m_regs.P.N = (test & 0x80) > 0;
          m_regs.P.V = (test & 0x40) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zpg,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* SEC */ {
          status << Mnemonic::SEC;

          m_regs.P.C = 1;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::AbsoluteX;
          readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
          return postExecHook(status, 4);
        } break;
      }
    } break;
    case 0x02: {
      switch (status.getAddrMode()) {
        case 0x00: /* RTI */ {
          status << Mnemonic::RTI;

          m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
          m_regs.PC     = popStack<uint16_t>();
          return postExecHook(status, 6);
        } break;
        case 0x01: /* NOP zpg (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPage;
          readRam<uint8_t>(status.operand.u8); // Dummy read
          return postExecHook(status, 3);
        } break;
        case 0x02: /* PHA */ {
          status << Mnemonic::PHA;

          pushStack<uint8_t>(m_regs.A);
          return postExecHook(status, 3);
        } break;
        case 0x03: /* JMP abs */ {
          status << Mnemonic::JMP << AddrMode::Absolute;

          m_regs.PC = status.operand.u16;
          return postExecHook(status, 3);
        } break;
        case 0x05: /* NOP zpg,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* CLI */ {
          status << Mnemonic::CLI;
          m_intrClrSchd = true;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::AbsoluteX;
          readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
          return postExecHook(status, 4);
        } break;
      }
    } break;
    case 0x03: {
      switch (status.getAddrMode()) {
        case 0x00: /* RTS */ {
          status << Mnemonic::RTS;

          m_regs.PC = popStack<uint16_t>() + 1;
          return postExecHook(status, 6);
        } break;
        case 0x01: /* NOP zp (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPage;
          readRam<uint8_t>(status.operand.u8); // Dummy read
          return postExecHook(status, 3);
        } break;
        case 0x02: /* PLA */ {
          status << Mnemonic::PLA;

          m_regs.A   = popStack<uint8_t>();
          m_regs.P.Z = m_regs.A == 0;
          m_regs.P.N = (m_regs.A & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x03: /* JMP (oper) */ {
          status << Mnemonic::JMP << AddrMode::Indirect;
          m_regs.PC = std::get<2>(evaluateOperandToValue<uint16_t>(status));
          return postExecHook(status, 5);
        } break;
        case 0x05: /* NOP zpg,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* SEI */ {
          status << Mnemonic::SEI;

          m_regs.P.I = true;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::AbsoluteX;
          readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
          return postExecHook(status, 4);
        } break;
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

          writeRamByte(status.operand.u8, m_regs.Y);
          return postExecHook(status, 3);
        } break;
        case 0x02: /* DEY */ {
          status << Mnemonic::DEY;

          --m_regs.Y;
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* STY abs */ {
          status << Mnemonic::STY << AddrMode::Absolute;
          writeRamByte(status.operand.u16, m_regs.Y);
          return postExecHook(status, 4);
        } break;
        case 0x05: /* STY zp,X */ {
          status << Mnemonic::STY << AddrMode::ZeroPageX;
          auto const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

          writeRamByte(addr, m_regs.Y);
          return postExecHook(status, 4);
        } break;
        case 0x06: /* TYA */ {
          status << Mnemonic::TYA;

          m_regs.A   = m_regs.Y;
          m_regs.P.Z = m_regs.A == 0;
          m_regs.P.N = (m_regs.A & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* SHY abs,X (illegal) */ {
          status << Mnemonic::SHY << AddrMode::AbsoluteX;
          auto const addr = static_cast<uint16_t>(status.operand.u16 + m_regs.X);

          writeRamByte(addr, m_regs.Y & ((addr >> 8) + 1));
          return postExecHook(status, 5);
        } break;
      }
    } break;
    case 0x05: {
      switch (status.getAddrMode()) {
        case 0x00: /* LDY imm */ {
          status << Mnemonic::LDY << AddrMode::Immediate;

          m_regs.Y   = status.operand.u8;
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* LDY zp */ {
          status << Mnemonic::LDY << AddrMode::ZeroPage;

          m_regs.Y   = readRamByte(status.operand.u8);
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* TAY */ {
          status << Mnemonic::TAY;
          m_regs.Y   = m_regs.A;
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* LDY abs */ {
          status << Mnemonic::LDY << AddrMode::Absolute;

          m_regs.Y   = readRamByte(status.operand.u16);
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* LDY zp,X */ {
          status << Mnemonic::LDY << AddrMode::ZeroPageX;
          m_regs.Y   = readRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.X));
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x06: /* CLV */ {
          status << Mnemonic::CLV;
          m_regs.P.V = 0;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* LDY abs,X */ {
          status << Mnemonic::LDY << AddrMode::AbsoluteX;
          uint16_t const addr = status.operand.u16 + m_regs.X;

          m_regs.Y   = readRamByte(addr);
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, (status.operand.u16 & 0xFF00) == (addr & 0xFF00) ? 4 : 5);
        } break;
      }
    } break;
    case 0x06: {
      switch (status.getAddrMode()) {
        case 0x00: /* CPY imm */ {
          status << Mnemonic::CPY << AddrMode::Immediate;

          uint8_t val = m_regs.Y - status.operand.u8;

          m_regs.P.C = m_regs.Y >= status.operand.u8;
          m_regs.P.Z = val == 0;
          m_regs.P.N = (val & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* CPY zp */ {
          status << Mnemonic::CPY << AddrMode::ZeroPage;

          auto const    rval = readRamByte(status.operand.u8);
          uint8_t const val  = m_regs.Y - rval;

          m_regs.P.C = m_regs.Y >= rval;
          m_regs.P.Z = val == 0;
          m_regs.P.N = (val & 0x80) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* INY */ {
          status << Mnemonic::INY;

          m_regs.Y += 1;
          m_regs.P.Z = m_regs.Y == 0;
          m_regs.P.N = (m_regs.Y & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* CPY abs */ {
          status << Mnemonic::CPY << AddrMode::Absolute;

          auto const rval = readRamByte(status.operand.u16);

          uint8_t const val = m_regs.Y - rval;

          m_regs.P.C = m_regs.Y >= rval;
          m_regs.P.Z = val == 0;
          m_regs.P.N = (val & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zpg,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* CLD */ {
          status << Mnemonic::CLD;
          m_regs.P.D = false;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::AbsoluteX;
          readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
          return postExecHook(status, 4);
        } break;
      }
    } break;
    case 0x07: {
      switch (status.getAddrMode()) {
        case 0x00: /* CPX imm */ {
          status << Mnemonic::CPX << AddrMode::Immediate;

          uint8_t val = m_regs.X - status.operand.u8;

          m_regs.P.C = m_regs.X >= status.operand.u8;
          m_regs.P.Z = val == 0;
          m_regs.P.N = (val & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* CPX zp */ {
          status << Mnemonic::CPX << AddrMode::ZeroPage;
          auto const [cycles, addr, rval] = evaluateOperandToValue<uint8_t>(status);

          uint8_t const val = m_regs.X - rval;

          m_regs.P.C = m_regs.X >= rval;
          m_regs.P.Z = val == 0;
          m_regs.P.N = (val & 0x80) > 0;
          return postExecHook(status, 3);
        } break;
        case 0x02: /* INX */ {
          status << Mnemonic::INX;

          m_regs.X += 1;
          m_regs.P.Z = m_regs.X == 0;
          m_regs.P.N = (m_regs.X & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* CPX abs */ {
          status << Mnemonic::CPX << AddrMode::Absolute;
          auto const [cycles, addr, rval] = evaluateOperandToValue<uint8_t>(status);

          uint8_t const val = m_regs.X - rval;

          m_regs.P.C = m_regs.X >= rval;
          m_regs.P.Z = val == 0;
          m_regs.P.N = (val & 0x80) > 0;
          return postExecHook(status, 4);
        } break;
        case 0x05: /* NOP zpg,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::ZeroPageX;
          evaluateOperandToValue<uint8_t>(status); // Dummy read
          return postExecHook(status, 4);
        } break;
        case 0x06: /* SED */ {
          status << Mnemonic::SED;

          m_regs.P.D = 1;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* NOP abs,X (illegal) */ {
          status << Mnemonic::NOP << AddrMode::AbsoluteX;
          readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
          return postExecHook(status, 4);
        } break;
      }
    } break;
  }

  throw UnhandledInstruction();
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

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      m_regs.A |= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
    case 0x01: { // AND
      status << Mnemonic::AND;
      parse();

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      m_regs.A &= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
    case 0x02: { // EOR
      status << Mnemonic::EOR;
      parse();

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      m_regs.A ^= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
    case 0x03: { // ADC
      status << Mnemonic::ADC;
      parse();

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      uint16_t result = m_regs.A + value + m_regs.P.C;
      m_regs.P.C      = result > 0xFF;
      m_regs.P.V      = (~(m_regs.A ^ value) & (m_regs.A ^ result) & 0x80) != 0;
      m_regs.A        = result & 0xFF;
      m_regs.P.Z      = m_regs.A == 0;
      m_regs.P.N      = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
    case 0x04: { // STA
      status << Mnemonic::STA;
      parse();

      auto [cycles, addr] = evaluateOperandToAddr(status);
      if (status.flags.addrMode == AddrMode::AbsoluteX) cycles += 1;
      if (status.flags.addrMode == AddrMode::AbsoluteY) cycles += 1;
      if (status.flags.addrMode == AddrMode::IndirIndexedY) cycles += 1;

      writeRamByte(addr, m_regs.A);
      return postExecHook(status, 3 + cycles);
    } break;
    case 0x05: { // LDA
      status << Mnemonic::LDA;
      parse();

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      m_regs.A   = value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
    case 0x06: { // CMP
      status << Mnemonic::CMP;
      parse();

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      uint8_t val = m_regs.A - value;
      m_regs.P.C  = m_regs.A >= value;
      m_regs.P.Z  = val == 0;
      m_regs.P.N  = (val & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
    case 0x07: { // SBC
      status << Mnemonic::SBC;
      parse();

      auto const [cycles, addr, value] = evaluateOperandToValue<uint8_t>(status);

      uint8_t  inverted_value = value ^ 0xFF;
      uint16_t carry_in       = m_regs.P.C ? 1 : 0;
      uint16_t res            = m_regs.A + inverted_value + carry_in;

      m_regs.P.V = ((m_regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_regs.P.C = res > 0xFF;
      m_regs.A   = res & 0xFF;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 2 + cycles);
    } break;
  }

  throw UnhandledInstruction();
}

uint8_t CPU6502::handleShift(InstructionStatus& status) {
  switch (status.getOpCode()) {
    case 0x00: {
      status << Mnemonic::ASL;

      uint8_t origValue = 0, resultValue = 0, cycles = 0;
      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw;
        case 0x01: /* ASL zp */ {
          status << AddrMode::ZeroPage;
          auto const [ncycles, addr, origValue] = evaluateOperandToValue<uint8_t>(status);

          cycles      = 4 + ncycles;
          resultValue = writeRamByte(addr, origValue << 1);
        } break;
        case 0x02: /* ASL A */ {
          status << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_regs.A;
          resultValue = (m_regs.A <<= 1);
        } break;
        case 0x03: /* ASL abs */ {
          status << AddrMode::Absolute;
          auto const [ncycles, addr, origValue] = evaluateOperandToValue<uint8_t>(status);

          cycles      = 4 + ncycles;
          resultValue = writeRamByte(status.operand.u16, origValue << 1);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* ASL zp,X */ {
          status << AddrMode::ZeroPageX;
          auto const [ncycles, addr, origValue] = evaluateOperandToValue<uint8_t>(status);

          cycles      = 4 + ncycles;
          resultValue = writeRamByte(addr, origValue << 1);
        } break;
        case 0x06: /* NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* ASL abs,X */ {
          status << AddrMode::AbsoluteX;
          auto const [ncycles, addr, origValue] = evaluateOperandToValue<uint8_t>(status);

          cycles      = 5 + ncycles;
          resultValue = writeRamByte(addr, origValue << 1);
        } break;
      }

      m_regs.P.C = (origValue & 0x80) > 0;
      m_regs.P.Z = (resultValue == 0);
      m_regs.P.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x01: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      status << Mnemonic::ROL;
      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw;
        case 0x01: /* ROL zp */ {
          status << AddrMode::ZeroPage;

          cycles      = 5;
          origValue   = readRamByte(status.operand.u8);
          resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
          writeRamByte(status.operand.u8, resultValue);
        } break;
        case 0x02: /* ROL A */ {
          status << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_regs.A;
          resultValue = (m_regs.A = (origValue << 1) | (m_regs.P.C ? 1 : 0));
        } break;
        case 0x03: /* ROL abs */ {
          status << AddrMode::Absolute;

          cycles      = 6;
          origValue   = readRamByte(status.operand.u16);
          resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
          writeRamByte(status.operand.u16, resultValue);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* ROL zp,X */ {
          status << AddrMode::ZeroPageX;
          uint8_t const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

          cycles      = 6;
          origValue   = readRamByte(addr);
          resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
          writeRamByte(addr, resultValue);
        } break;
        case 0x06: /* NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* ROL abs,X */ {
          status << AddrMode::AbsoluteX;
          uint16_t const addr = status.operand.u16 + m_regs.X;

          cycles      = 7;
          origValue   = readRamByte(addr);
          resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
          writeRamByte(addr, resultValue);
        } break;
      }

      m_regs.P.C = (origValue & 0x80) != 0;
      m_regs.P.Z = (resultValue == 0);
      m_regs.P.N = (resultValue & 0x80) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x02: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;
      status << Mnemonic::LSR;

      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw;
        case 0x01: /* LSR zp */ {
          status << AddrMode::ZeroPage;

          cycles      = 5;
          origValue   = readRamByte(status.operand.u8);
          resultValue = writeRamByte(status.operand.u8, origValue >> 1);
        } break;
        case 0x02: /* LSR A */ {
          status << AddrMode::Accum;

          cycles      = 2;
          origValue   = m_regs.A;
          resultValue = (m_regs.A >>= 1);
        } break;
        case 0x03: /* LSR abs */ {
          status << AddrMode::Absolute;

          cycles      = 6;
          origValue   = readRamByte(status.operand.u16);
          resultValue = writeRamByte(status.operand.u16, origValue >> 1);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* LSR zp,X */ {
          status << AddrMode::ZeroPageX;
          auto const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

          cycles      = 6;
          origValue   = readRamByte(addr);
          resultValue = writeRamByte(addr, origValue >> 1);
        } break;
        case 0x06: /* NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* LSR abs,X */ {
          status << AddrMode::AbsoluteX;
          auto const addr = static_cast<uint16_t>(status.operand.u16 + m_regs.X);

          cycles      = 7;
          origValue   = readRamByte(addr);
          resultValue = writeRamByte(addr, origValue >> 1);
        } break;
      }

      m_regs.P.N = 0;
      m_regs.P.Z = resultValue == 0;
      m_regs.P.C = (origValue & 0x01) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x03: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      switch (status.getAddrMode()) {
        case 0x00: /* JAM */ throw;
        case 0x01: /* ROR zp */ {
          status << Mnemonic::ROR << AddrMode::ZeroPage;

          cycles      = 5;
          origValue   = readRamByte(status.operand.u8);
          resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
          writeRamByte(status.operand.u8, resultValue);
        } break;
        case 0x02: /* ROR A */ {
          status << Mnemonic::ROR << AddrMode::Accum;

          uint8_t incoming_carry = m_regs.P.C ? 0x80 : 0x00;
          uint8_t shifted_value  = (m_regs.A >> 1) | incoming_carry;
          m_regs.P.C             = (m_regs.A & 0x01) != 0;

          m_regs.A   = shifted_value;
          m_regs.P.Z = (shifted_value == 0);
          m_regs.P.N = (shifted_value & 0x80) != 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* ROR abs */ {
          status << Mnemonic::ROR << AddrMode::Absolute;

          cycles      = 6;
          origValue   = readRamByte(status.operand.u16);
          resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
          writeRamByte(status.operand.u16, resultValue);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* ROR zp,X */ {
          status << Mnemonic::ROR << AddrMode::ZeroPageX;
          uint16_t const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

          cycles      = 6;
          origValue   = readRamByte(addr);
          resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
          writeRamByte(addr, resultValue);
        } break;
        case 0x06: /* NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* ROR abs,X */ {
          status << Mnemonic::ROR << AddrMode::AbsoluteX;
          uint16_t const addr = status.operand.u16 + m_regs.X;

          cycles      = 7;
          origValue   = readRamByte(addr);
          resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
          writeRamByte(addr, resultValue);
        } break;
      }

      m_regs.P.C = (origValue & 0x01) != 0;
      m_regs.P.Z = (resultValue == 0);
      m_regs.P.N = (resultValue & 0x80) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x04: { // Transfers
      switch (status.getAddrMode()) {
        case 0x00: /* NOP imm (illegal) */ {
          status << Mnemonic::NOP << AddrMode::Immediate;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* STX zp */ {
          status << Mnemonic::STX << AddrMode::ZeroPage;

          writeRamByte(status.operand.u8, m_regs.X);
          return postExecHook(status, 3);
        } break;
        case 0x02: /* TXA */ {
          status << Mnemonic::TXA;

          m_regs.A   = m_regs.X;
          m_regs.P.Z = m_regs.A == 0;
          m_regs.P.N = (m_regs.A & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* STX abs */ {
          status << Mnemonic::STX << AddrMode::Absolute;

          writeRamByte(status.operand.u16, m_regs.X);
          return postExecHook(status, 4);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* STX zp,Y */ {
          status << Mnemonic::STX << AddrMode::ZeroPageY;

          writeRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.Y), m_regs.X);
          return postExecHook(status, 4);
        } break;
        case 0x06: /* TXS */ {
          status << Mnemonic::TXS;

          m_regs.SP = m_regs.X;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* SHX abs,Y (illegal) */ {
          status << Mnemonic::SHX << AddrMode::AbsoluteY;
          auto const addr = static_cast<uint16_t>(status.operand.u16 + m_regs.Y);

          writeRamByte(addr, m_regs.X & ((addr >> 8) + 1));
          return postExecHook(status, 5);
        } break;
      }
    } break;
    case 0x05: {
      uint8_t value = 0, cycles = 1;

      switch (status.getAddrMode()) {
        case 0x00: /* LDX imm */ {
          status << Mnemonic::LDX << AddrMode::Immediate;

          value = status.operand.u8;
          cycles += 1;
        } break;
        case 0x01: /* LDX zp */ {
          status << Mnemonic::LDX << AddrMode::ZeroPage;

          value = readRamByte(status.operand.u8);
          cycles += 2;
        } break;
        case 0x02: /* TAX */ {
          status << Mnemonic::TAX;

          value = m_regs.A;
          cycles += 1;
        } break;
        case 0x03: /* LDX abs */ {
          status << Mnemonic::LDX << AddrMode::Absolute;

          value = readRamByte(status.operand.u16);
          cycles += 3;
        } break;
        case 0x04: throw;
        case 0x05: /* LDX zp,Y */ {
          status << Mnemonic::LDX << AddrMode::ZeroPageY;

          value = readRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.Y));
          cycles += 3;
        } break;
        case 0x06: /* TSX */ {
          status << Mnemonic::TSX;

          value = m_regs.SP;
          cycles += 1;
        } break;
        case 0x07: /* LDX abs,Y */ {
          status << Mnemonic::LDX << AddrMode::AbsoluteY;

          auto target_addr = status.operand.u16 + m_regs.Y;

          value = readRamByte(target_addr);
          cycles += 3;
          if ((status.operand.u16 & 0xFF00) != (target_addr & 0xFF00)) {
            cycles += 1;
          }
        } break;
      }

      m_regs.X   = value;
      m_regs.P.Z = m_regs.X == 0;
      m_regs.P.N = (m_regs.X & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x06: {
      uint8_t resultValue = 0, cycles = 0;

      status << Mnemonic::DEC;
      switch (status.getAddrMode()) {
        case 0x00: /*  NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* DEC zp */ {
          status << AddrMode::ZeroPage;

          cycles      = 5;
          resultValue = writeRamByte(status.operand.u8, readRamByte(status.operand.u8) - 1);
        } break;
        case 0x02: /* DEX */ {
          status << Mnemonic::DEX;

          m_regs.X -= 1;
          m_regs.P.Z = m_regs.X == 0;
          m_regs.P.N = (m_regs.X & 0x80) > 0;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* DEC abs */ {
          status << AddrMode::Absolute;

          cycles      = 6;
          resultValue = writeRamByte(status.operand.u16, readRamByte(status.operand.u16) - 1);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* DEC zp,X */ {
          status << AddrMode::ZeroPageX;
          auto const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

          cycles      = 6;
          resultValue = writeRamByte(addr, readRamByte(addr) - 1);
        } break;
        case 0x06: /* NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* DEC abs,X */ {
          status << AddrMode::AbsoluteX;
          auto const addr = status.operand.u16 + m_regs.X;

          cycles      = 7;
          resultValue = writeRamByte(addr, readRamByte(addr) - 1);
        } break;
      }

      m_regs.P.Z = resultValue == 0;
      m_regs.P.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x07: {
      uint8_t resultValue = 0, cycles = 0;

      status << Mnemonic::INC;
      switch (status.getAddrMode()) {
        case 0x00: /* NOP imm (illegal) */ {
          status << Mnemonic::NOP << AddrMode::Immediate;
          return postExecHook(status, 2);
        } break;
        case 0x01: /* INC zp */ {
          status << AddrMode::ZeroPage;

          cycles      = 5;
          resultValue = writeRamByte(status.operand.u8, readRamByte(status.operand.u8) + 1);
        } break;
        case 0x02: /* NOP impl (legal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x03: /* INC abs */ {
          status << AddrMode::Absolute;

          cycles      = 6;
          resultValue = writeRamByte(status.operand.u16, readRamByte(status.operand.u16) + 1);
        } break;
        case 0x04: /* JAM */ throw;
        case 0x05: /* INC zp,X */ {
          status << AddrMode::ZeroPageX;
          auto const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

          cycles      = 6;
          resultValue = writeRamByte(addr, readRamByte(addr) + 1);
        } break;
        case 0x06: /* NOP impl (illegal) */ {
          status << Mnemonic::NOP;
          return postExecHook(status, 2);
        } break;
        case 0x07: /* INC abs,X */ {
          status << AddrMode::AbsoluteX;
          auto const addr = status.operand.u16 + m_regs.X;

          cycles      = 7;
          resultValue = writeRamByte(addr, readRamByte(addr) + 1);
        } break;
      }

      m_regs.P.Z = resultValue == 0;
      m_regs.P.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
  }

  throw UnhandledInstruction();
}

uint8_t CPU6502::handleUnknown(InstructionStatus& status) { /* All instructions below are illegal */
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

  return postExecHook(status, 0);
}

uint8_t CPU6502::step() {
  if (m_nmiTriggered) {
    m_nmiTriggered = false;
    return interrupt(0xFFFA, false);
  } else if (m_regs.P.I == 1 && m_intrClrSchd) {
    m_intrClrSchd = false;
    m_regs.P.I    = 0;
  } else if (m_regs.P.I == 0 && m_irqTriggered && !m_nmiTriggered) {
    m_irqTriggered = false;
    return interrupt(0xFFFE, false);
  }

  InstructionStatus s {
      .owner     = *this,
      .startAddr = m_regs.PC,
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
      case InstClass::Unknown: return handleUnknown(s);
    }
  } catch (SkipInstruction const& ex) {
    s.flags.stage = ExecStage::SkipExec;
    if (m_hook) m_hook(s);
    return 0;
  } catch (UnhandledInstruction const& ex) {
    s.flags.stage = ExecStage::FailExec;
    if (m_hook) m_hook(s);
    throw ex; // Rethrow if fail hook is not set or returned
  }
}

void CPU6502::triggerNMI() {
  m_nmiTriggered = true;
}

void CPU6502::triggerIRQ() {
  m_irqTriggered = true;
}

uint8_t CPU6502::writeRamByte(uint16_t addr, uint8_t value) {
  if (addr <= 0x1FFF) addr &= 0x7ff;
  if (auto handler = findHandler(addr); isValidHandler(handler)) {
    return handler->second(true, addr, value);
  }
  return m_ram.at(addr) = value;
}

uint8_t CPU6502::readRamByte(uint16_t addr) const {
  if (addr <= 0x1FFF) addr &= 0x7ff;
  if (auto handler = findHandler(addr); isValidHandler(handler)) {
    return handler->second(false, addr, 0);
  }
  return m_ram.at(addr);
}

void CPU6502::preExecHook(InstructionStatus& status) {
  status.flags.stage = ExecStage::PreExec;
  if (m_hook) m_hook(status);
  if (status.flags.skipExec) throw SkipInstruction();
}

uint8_t CPU6502::postExecHook(InstructionStatus& status, uint8_t cycles) {
  status.flags.stage = ExecStage::PostExec, status.flags.cycles += cycles;
  if (m_hook) m_hook(status);
  return status.flags.cycles;
}
