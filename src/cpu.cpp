#include "cpu.hh"

#include <cstdint>
#include <exception>
#include <format>
#include <iterator>

class UnhandledInstruction: public std::exception {
  public:
  UnhandledInstruction() {}

  const char* what() const noexcept override { return ""; }
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
    default: std::format_to(std::back_inserter(temp), "${:X}", holder.raw); break;
  }

  temp.push_back(' ');

  switch (addrMode) {
    case AddrMode::Accum: temp.push_back('A');
    case AddrMode::Absolute: std::format_to(std::back_inserter(temp), "${:X}", operand.u16); break;
    case AddrMode::AbsoluteX: std::format_to(std::back_inserter(temp), "${:X},X", operand.u16); break;
    case AddrMode::AbsoluteY: std::format_to(std::back_inserter(temp), "${:X},Y", operand.u16); break;
    case AddrMode::Immediate: std::format_to(std::back_inserter(temp), "#${:X}", operand.u8); break;
    case AddrMode::Implied: temp.append("impl"); break;
    case AddrMode::Indirect: std::format_to(std::back_inserter(temp), "(${:X})", operand.u16); break;
    case AddrMode::IndexedXIndir: std::format_to(std::back_inserter(temp), "(${:X},X)", operand.u8); break;
    case AddrMode::IndirIndexedY: std::format_to(std::back_inserter(temp), "(${:X}),Y", operand.u8); break;
    case AddrMode::Relative: std::format_to(std::back_inserter(temp), "${:X}", operand.s8); break;
    case AddrMode::ZeroPage: std::format_to(std::back_inserter(temp), "${:X}", operand.u8); break;
    case AddrMode::ZeroPageX: std::format_to(std::back_inserter(temp), "${:X},X", operand.u8); break;
    case AddrMode::ZeroPageY: std::format_to(std::back_inserter(temp), "${:X},Y", operand.u8); break;
    default: temp.append("???"); break;
  }

  return temp;
}

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

uint8_t CPU6502::handleControl(InstructionStatus& status) {
  if (status->branch.isValid()) { // Handle branching
    status << AddrMode::Relative << readPC<int8_t>();

    bool flag_status = false;
    switch (status->branch.sel) {
      case 0b00 /* Negative */: {
        status << (status->branch.cond ? Mnemonic::BMI : Mnemonic::BPL);
        flag_status = m_regs.P.N;
      } break;
      case 0b01 /* Overflow */: {
        status << (status->branch.cond ? Mnemonic::BVS : Mnemonic::BVC);
        flag_status = m_regs.P.V;
      } break;
      case 0b10 /* Carry */: {
        status << (status->branch.cond ? Mnemonic::BCS : Mnemonic::BCC);
        flag_status = m_regs.P.C;
      } break;
      case 0b11 /* Zero */: {
        status << (status->branch.cond ? Mnemonic::BEQ : Mnemonic::BNE);
        flag_status = m_regs.P.Z;
      } break;
    }
    status << preExecHook(status);

    if (flag_status != status->branch.cond) return postExecHook(status, 2);

    uint16_t old_pc = m_regs.PC;
    m_regs.PC += status.operand.s8;
    return postExecHook(status, (old_pc & 0xFF00) == (m_regs.PC & 0xFF00) ? 3 : 4);
  }

  // Note: All missing 0x04's here are handled by branching block above
  switch (status->deflt.operation) {
    case 0x00: { // Control Flow
      if (status->getAddrMode() == 0x00) /* BRK */ {
        status << Mnemonic::BRK << preExecHook(status);

        pushStack<uint16_t>(++m_regs.PC);
        m_regs.P.I = 1;

        auto p = m_regs.P;

        p.B = 1;
        pushStack<uint8_t>(p._raw);
        m_regs.PC = readRam<uint16_t>(0xFFFE);
        return postExecHook(status, 7);
      } else if (status->getAddrMode() == 0x01) /* NOP zp */ {
        status << Mnemonic::NOP << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* PHP */ {
        status << Mnemonic::PHP << preExecHook(status);

        auto p = m_regs.P;

        p.B = 1;
        pushStack<uint8_t>(p._raw);
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x03) /* NOP */ {
        status << Mnemonic::NOP << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);
        readRam<uint16_t>(status.operand.u16); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x05) /* NOP zpg,X */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* CLC */ {
        status << Mnemonic::CLC << preExecHook(status);

        m_regs.P.C = 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* NOP abs,X */ {
        status << Mnemonic::NOP << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
        return postExecHook(status, 4);
      }
    } break;
    case 0x01: {
      if (status->getAddrMode() == 0x00) /* JSR abs */ {
        status << Mnemonic::JSR << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        pushStack<uint16_t>(m_regs.PC - 1);
        m_regs.PC = status.operand.u16;
        return postExecHook(status, 6);
      } else if (status->getAddrMode() == 0x01) /* BIT zp */ {
        status << Mnemonic::BIT << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        auto const test = readRamByte(status.operand.u8);
        m_regs.P.Z      = (m_regs.A & test) == 0;
        m_regs.P.N      = (test & 0x80) > 0;
        m_regs.P.V      = (test & 0x40) > 0;
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* PLP */ {
        status << Mnemonic::PLP << preExecHook(status);

        m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x03) /* BIT abs */ {
        status << Mnemonic::BIT << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        auto test  = readRamByte(status.operand.u16);
        m_regs.P.Z = (m_regs.A & test) == 0;
        m_regs.P.N = (test & 0x80) > 0;
        m_regs.P.V = (test & 0x40) > 0;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x05) /* NOP zpg,X */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* SEC */ {
        status << Mnemonic::SEC << preExecHook(status);

        m_regs.P.C = 1;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* NOP abs,X */ {
        status << Mnemonic::NOP << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
        return postExecHook(status, 4);
      }
    } break;
    case 0x02: {
      if (status->getAddrMode() == 0x00) /* RTI */ {
        status << Mnemonic::RTI << preExecHook(status);

        m_regs.P._raw = (popStack<uint8_t>() & 0xEF) | 0x20;
        m_regs.PC     = popStack<uint16_t>();
        return postExecHook(status, 6);
      } else if (status->getAddrMode() == 0x01) /* NOP zpg */ {
        status << Mnemonic::NOP << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u8); // Dummy read
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* PHA */ {
        status << Mnemonic::PHA << preExecHook(status);

        pushStack<uint8_t>(m_regs.A);
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x03) /* JMP abs */ {
        status << Mnemonic::JMP << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        m_regs.PC = status.operand.u16;
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x05) /* NOP zpg,X */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* CLI */ {
        status << Mnemonic::CLI << preExecHook(status);

        m_regs.P.I = 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* NOP abs,X */ {
        status << Mnemonic::NOP << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
        return postExecHook(status, 4);
      }
    } break;
    case 0x03: {
      if (status->getAddrMode() == 0x00) /* RTS */ {
        status << Mnemonic::RTS << preExecHook(status);

        m_regs.PC = popStack<uint16_t>() + 1;
        return postExecHook(status, 6);
      } else if (status->getAddrMode() == 0x01) /* NOP zpg */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u8); // Dummy read
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* PLA */ {
        status << Mnemonic::PLA << preExecHook(status);

        m_regs.A   = popStack<uint8_t>();
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = (m_regs.A & 0x80) > 0;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x03) /* JMP (oper) */ {
        status << Mnemonic::JMP << AddrMode::Indirect << readPC<uint16_t>() << preExecHook(status);

        uint16_t high_addr = (status.operand.u16 & 0xFF00) | static_cast<uint8_t>((status.operand.u16 & 0xFF) + 1);
        m_regs.PC          = (readRamByte(high_addr) << 8) | readRamByte(status.operand.u16);
        return postExecHook(status, 5);
      } else if (status->getAddrMode() == 0x05) /* NOP zpg,X */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* SEI */ {
        status << Mnemonic::SEI << preExecHook(status);

        m_regs.P.I = true;
        return postExecHook(status, 2);

      } else if (status->getAddrMode() == 0x07) /* NOP abs,X */ {
        status << Mnemonic::NOP << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
        return postExecHook(status, 4);
      }
    } break;
    case 0x04: {
      if (status->getAddrMode() == 0x00) /* NOP imm */ {
        status << Mnemonic::NOP << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* STY zp */ {
        status << Mnemonic::STY << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        writeRamByte(status.operand.u8, m_regs.Y);
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* DEY */ {
        status << Mnemonic::DEY << preExecHook(status);

        --m_regs.Y;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* STY abs */ {
        status << Mnemonic::STY << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);
        writeRamByte(status.operand.u16, m_regs.Y);
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x05) /* STY zp,X */ {
        status << Mnemonic::STY << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);

        auto const rval = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

        writeRamByte(rval, m_regs.Y);
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* TYA */ {
        status << Mnemonic::TYA << preExecHook(status);

        m_regs.A   = m_regs.Y;
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = (m_regs.A & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* SHY abs,X (illegal) */ {
      }
    } break;
    case 0x05: {
      if (status->getAddrMode() == 0x00) /* LDY imm */ {
        status << Mnemonic::LDY << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);

        m_regs.Y   = status.operand.u8;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* LDY zp */ {
        status << Mnemonic::LDY << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        m_regs.Y   = readRamByte(status.operand.u8);
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* TAY */ {
        status << Mnemonic::TAY << preExecHook(status);
        m_regs.Y   = m_regs.A;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* LDY abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        m_regs.Y   = readRamByte(status.operand.u16);
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x05) /* LDY zp,X */ {
        status << Mnemonic::LDY << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        m_regs.Y   = readRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.X));
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* CLV */ {
        status << Mnemonic::CLV << preExecHook(status);
        m_regs.P.V = 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* LDY abs,X */ {
        status << Mnemonic::LDY << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);

        uint16_t target_addr = status.operand.u16 + m_regs.X;

        m_regs.Y = readRamByte(target_addr);
        return postExecHook(status, (status.operand.u16 & 0xFF00) == (target_addr & 0xFF00) ? 4 : 5);
      }
    } break;
    case 0x06: {
      if (status->getAddrMode() == 0x00) /* CPY imm */ {
        status << Mnemonic::CPY << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);

        uint8_t val = m_regs.Y - status.operand.u8;

        m_regs.P.C = m_regs.Y >= status.operand.u8;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* CPY zp */ {
        status << Mnemonic::CPY << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        auto const    rval = readRamByte(status.operand.u8);
        uint8_t const val  = m_regs.Y - rval;

        m_regs.P.C = m_regs.Y >= rval;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* INY */ {
        status << Mnemonic::INY << preExecHook(status);

        m_regs.Y += 1;
        m_regs.P.Z = m_regs.Y == 0;
        m_regs.P.N = (m_regs.Y & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* CPY abs */ {
        status << Mnemonic::CPY << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        auto const rval = readRamByte(status.operand.u16);

        uint8_t const val = m_regs.Y - rval;

        m_regs.P.C = m_regs.Y >= rval;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x05) /* NOP zpg,X */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* CLD */ {
        status << Mnemonic::CLD << preExecHook(status);
        m_regs.P.D = false;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* NOP abs,X */ {
        status << Mnemonic::NOP << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
        return postExecHook(status, 4);
      }
    } break;
    case 0x07: {
      if (status->getAddrMode() == 0x00) /* CPX imm */ {
        status << Mnemonic::CPX << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);

        uint8_t val = m_regs.X - status.operand.u8;

        m_regs.P.C = m_regs.X >= status.operand.u8;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* CPX zp */ {
        status << Mnemonic::CPX << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        auto const rval = readRamByte(status.operand.u8);

        uint8_t const val = m_regs.X - rval;

        m_regs.P.C = m_regs.X >= rval;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* INX */ {
        status << Mnemonic::INX << preExecHook(status);

        m_regs.X += 1;
        m_regs.P.Z = m_regs.X == 0;
        m_regs.P.N = (m_regs.X & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* CPX abs */ {
        status << Mnemonic::CPX << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        auto const rval = readRamByte(status.operand.u16);

        uint8_t const val = m_regs.X - rval;

        m_regs.P.C = m_regs.X >= rval;
        m_regs.P.Z = val == 0;
        m_regs.P.N = (val & 0x80) > 0;
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x05) /* NOP zpg,X */ {
        status << Mnemonic::NOP << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        readRam<uint8_t>(static_cast<uint8_t>(status.operand.u8 + m_regs.X)); // Dummy read
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x06) /* SED */ {
        status << Mnemonic::SED << preExecHook(status);

        m_regs.P.D = 1;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* NOP abs,X */ {
        status << Mnemonic::NOP << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        readRam<uint8_t>(status.operand.u16 + m_regs.X); // Dummy read
        return postExecHook(status, 4);
      }
    } break;
  }

  throw UnhandledInstruction();
}

uint8_t CPU6502::handleMath(InstructionStatus& status) {
  if (status->raw == 0x89) /* Special cse of NOP imm */ {
    status << Mnemonic::NOP << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);
    return postExecHook(status, 2);
  }

  auto const getAddrFromOp = [&]() -> std::pair<uint8_t /* num cycles */, uint16_t /* addr */> {
    switch (status->getAddrMode()) {
      case 0b000 /* (Indir,X) */: {
        status << AddrMode::IndexedXIndir << readPC<uint8_t>() << preExecHook(status);
        return {5,
                (readRamByte(static_cast<uint8_t>((status.operand.u8 + m_regs.X) + 1)) << 8) | readRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.X))};
      } break;
      case 0b001 /* Zero Page */: {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);
        return {1, status.operand.u8};
      } break;
      case 0b011 /* Absolute */: {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);
        return {2, status.operand.u16};
      } break;
      case 0b100 /* (Indir),Y*/: {
        status << AddrMode::IndirIndexedY << readPC<uint8_t>() << preExecHook(status);
        uint16_t const base   = (readRamByte((status.operand.u8 + 1) & 0xFF) << 8) | readRamByte(status.operand.u8);
        uint16_t const target = base + m_regs.Y;
        return {(base & 0xFF00) == (target & 0xFF00) ? 4 : 5, target};
      } break;
      case 0b101 /* Zero Page, X */: {
        status << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        return {2, static_cast<uint8_t>(status.operand.u8 + m_regs.X)};
      } break;
      case 0b110 /* Absolute, Y */: {
        status << AddrMode::AbsoluteY << readPC<uint16_t>() << preExecHook(status);
        auto const target = status.operand.u16 + m_regs.Y;
        return {(status.operand.u16 & 0xFF00) == (target & 0xFF00) ? 3 : 4, target};
      } break;
      case 0b111 /* Absolute, X */: {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        auto const target = status.operand.u16 + m_regs.X;
        return {(status.operand.u16 & 0xFF00) == (target & 0xFF00) ? 3 : 4, target};
      } break;
      default: throw -1;
    }
  };
  auto const resolveValue = [&]() -> std::pair<uint8_t, uint8_t> {
    if (status->getAddrMode() == 0x02) {
      status << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);
      return {1, status.operand.u8};
    }
    auto [cycles, addr] = getAddrFromOp();
    return {cycles, readRamByte(addr)};
  };

  // Note: preExecHook() is called inside resolve*() functions here
  switch (status->deflt.operation) {
    case 0x00: { // ORA
      status << Mnemonic::ORA;

      auto const [cycles, value] = resolveValue();

      m_regs.A |= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
    case 0x01: { // AND
      status << Mnemonic::AND;

      auto const [cycles, value] = resolveValue();

      m_regs.A &= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
    case 0x02: { // EOR
      status << Mnemonic::EOR;

      auto const [cycles, value] = resolveValue();

      m_regs.A ^= value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
    case 0x03: { // ADC
      status << Mnemonic::ADC;

      auto const [cycles, value] = resolveValue();

      uint16_t result = m_regs.A + value + m_regs.P.C;
      m_regs.P.C      = result > 0xFF;
      m_regs.P.V      = (~(m_regs.A ^ value) & (m_regs.A ^ result) & 0x80) != 0;
      m_regs.A        = result & 0xFF;
      m_regs.P.Z      = m_regs.A == 0;
      m_regs.P.N      = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
    case 0x04: { // STA
      status << Mnemonic::STA;

      auto const [cycles, addr] = getAddrFromOp();

      writeRamByte(addr, m_regs.A);
      return postExecHook(status, 2 /* base number of cycles for STA */ + cycles);
    } break;
    case 0x05: { // LDA
      status << Mnemonic::LDA;

      auto const [cycles, value] = resolveValue();

      m_regs.A   = value;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
    case 0x06: { // CMP
      status << Mnemonic::CMP;

      auto const [cycles, value] = resolveValue();

      uint8_t val = m_regs.A - value;
      m_regs.P.C  = m_regs.A >= value;
      m_regs.P.Z  = val == 0;
      m_regs.P.N  = (val & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
    case 0x07: { // SBC
      status << Mnemonic::SBC;

      auto const [cycles, value] = resolveValue();

      uint8_t  inverted_value = value ^ 0xFF;
      uint16_t carry_in       = m_regs.P.C ? 1 : 0;
      uint16_t res            = m_regs.A + inverted_value + carry_in;

      m_regs.P.V = ((m_regs.A ^ res) & (inverted_value ^ res) & 0x80) != 0;
      m_regs.P.C = res > 0xFF;
      m_regs.A   = res & 0xFF;
      m_regs.P.Z = m_regs.A == 0;
      m_regs.P.N = (m_regs.A & 0x80) > 0;
      return postExecHook(status, 1 + cycles);
    } break;
  }

  throw UnhandledInstruction();
}

uint8_t CPU6502::handleShift(InstructionStatus& status) {
  switch (status->deflt.operation) {
    case 0x00: {
      status << Mnemonic::ASL;

      uint8_t origValue = 0, resultValue = 0, cycles = 0;
      if (status->getAddrMode() == 0x00) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x01) /* ASL zp */ {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        cycles      = 5;
        origValue   = readRamByte(status.operand.u8);
        resultValue = writeRamByte(status.operand.u8, origValue << 1);
      } else if (status->getAddrMode() == 0x02) /* ASL A */ {
        status << AddrMode::Accum << preExecHook(status);

        cycles      = 2;
        origValue   = m_regs.A;
        resultValue = (m_regs.A <<= 1);
      } else if (status->getAddrMode() == 0x03) /* ASL abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        cycles      = 6;
        origValue   = readRamByte(status.operand.u16);
        resultValue = writeRamByte(status.operand.u16, resultValue << 1);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x05) /* ASL zp,X */ {
        status << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        uint8_t const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

        cycles      = 6;
        origValue   = readRamByte(addr);
        resultValue = writeRamByte(addr, origValue << 1);
      } else if (status->getAddrMode() == 0x06) /* NOP impl (illegal) */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* ASL abs,X */ {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        uint16_t const addr = status.operand.u16 + m_regs.X;

        cycles      = 7;
        origValue   = readRamByte(addr);
        resultValue = writeRamByte(addr, origValue << 1);
      }

      m_regs.P.C = (origValue & 0x80) > 0;
      m_regs.P.Z = (resultValue == 0);
      m_regs.P.N = (resultValue & 0x80) > 0;
      return postExecHook(status, 5);
    } break;
    case 0x01: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      status << Mnemonic::ROL;
      if (status->getAddrMode() == 0x00) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x01) /* ROL zp */ {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        cycles      = 5;
        origValue   = readRamByte(status.operand.u8);
        resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
        writeRamByte(status.operand.u8, resultValue);
      } else if (status->getAddrMode() == 0x02) /* ROL A */ {
        status << AddrMode::Accum << preExecHook(status);

        cycles      = 2;
        origValue   = m_regs.A;
        resultValue = (m_regs.A = (origValue << 1) | (m_regs.P.C ? 1 : 0));
      } else if (status->getAddrMode() == 0x03) /* ROL abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        cycles      = 6;
        origValue   = readRamByte(status.operand.u16);
        resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
        writeRamByte(status.operand.u16, resultValue);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x05) /* ROL zp,X */ {
        status << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        uint8_t const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

        cycles      = 6;
        origValue   = readRamByte(addr);
        resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
        writeRamByte(addr, resultValue);
      } else if (status->getAddrMode() == 0x06) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* ROL abs,X */ {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        uint16_t const addr = status.operand.u16 + m_regs.X;

        cycles      = 6;
        origValue   = readRamByte(addr);
        resultValue = (origValue << 1) | (m_regs.P.C ? 1 : 0);
        writeRamByte(addr, resultValue);
      }

      m_regs.P.C = (origValue & 0x80) != 0;
      m_regs.P.Z = (resultValue == 0);
      m_regs.P.N = (resultValue & 0x80) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x02: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      if (status->getAddrMode() == 0x00) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x01) /* LSR zp */ {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        cycles      = 5;
        origValue   = readRamByte(status.operand.u8);
        resultValue = writeRamByte(status.operand.u8, origValue >> 1);
      } else if (status->getAddrMode() == 0x02) /* LSR A */ {
        status << AddrMode::Accum << preExecHook(status);

        cycles      = 2;
        origValue   = m_regs.A;
        resultValue = (m_regs.A >>= 1);
      } else if (status->getAddrMode() == 0x03) /* LSR abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        cycles      = 6;
        origValue   = readRamByte(status.operand.u16);
        resultValue = writeRamByte(status.operand.u16, origValue >> 1);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
      } else if (status->getAddrMode() == 0x05) /* LSR zp,X */ {
      } else if (status->getAddrMode() == 0x06) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* LSR abs,X */ {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        auto const addr = static_cast<uint16_t>(status.operand.u16 + m_regs.X);

        cycles      = 7;
        origValue   = readRamByte(addr);
        resultValue = writeRamByte(addr, origValue >> 1);
      }

      m_regs.P.N = 0;
      m_regs.P.Z = resultValue == 0;
      m_regs.P.C = (origValue & 0x01) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x03: {
      uint8_t origValue = 0, resultValue = 0, cycles = 0;

      if (status->getAddrMode() == 0x00) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x01) /* ROR zp */ {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        cycles      = 5;
        origValue   = readRamByte(status.operand.u8);
        resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
        writeRamByte(status.operand.u8, resultValue);
      } else if (status->getAddrMode() == 0x02) /* ROR A */ {
        status << Mnemonic::ROR << AddrMode::Accum << preExecHook(status);

        uint8_t incoming_carry = m_regs.P.C ? 0x80 : 0x00;
        uint8_t shifted_value  = (m_regs.A >> 1) | incoming_carry;
        m_regs.P.C             = (m_regs.A & 0x01) != 0;

        m_regs.A   = shifted_value;
        m_regs.P.Z = (shifted_value == 0);
        m_regs.P.N = (shifted_value & 0x80) != 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* ROR abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        cycles      = 6;
        origValue   = readRamByte(status.operand.u16);
        resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
        writeRamByte(status.operand.u16, resultValue);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x05) /* ROR zp,X */ {
        status << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        uint16_t const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

        cycles      = 6;
        origValue   = readRamByte(addr);
        resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
        writeRamByte(addr, resultValue);
      } else if (status->getAddrMode() == 0x06) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* ROR abs,X */ {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        uint16_t const addr = status.operand.u16 + m_regs.X;

        cycles      = 7;
        origValue   = readRamByte(addr);
        resultValue = (origValue >> 1) | (m_regs.P.C ? 0x80 : 0x00);
        writeRamByte(addr, resultValue);
      }

      m_regs.P.C = (origValue & 0x01) != 0;
      m_regs.P.Z = (resultValue == 0);
      m_regs.P.N = (resultValue & 0x80) != 0;
      return postExecHook(status, cycles);
    } break;
    case 0x04: { // Transfers
      if (status->getAddrMode() == 0x00) /* NOP imm */ {
        status << Mnemonic::NOP << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* STX zp */ {
        status << Mnemonic::STX << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        writeRamByte(status.operand.u8, m_regs.X);
        return postExecHook(status, 3);
      } else if (status->getAddrMode() == 0x02) /* TXA */ {
        status << Mnemonic::TXA << preExecHook(status);

        m_regs.A   = m_regs.X;
        m_regs.P.Z = m_regs.A == 0;
        m_regs.P.N = (m_regs.A & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* STX abs */ {
        status << Mnemonic::STX << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        writeRamByte(status.operand.u16, m_regs.X);
        return postExecHook(status, 4);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x05) /* STX zp,Y */ {
      } else if (status->getAddrMode() == 0x06) /* TXS */ {
        status << Mnemonic::TXS << preExecHook(status);

        m_regs.SP = m_regs.X;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* SHX oper,Y (illegal) */ {
      }
    } break;
    case 0x05: {
      uint8_t value = 0, cycles = 1;

      if (status->getAddrMode() == 0x00) /* LDX imm */ {
        status << Mnemonic::LDX << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);

        value = status.operand.u8;
        cycles += 1;
      } else if (status->getAddrMode() == 0x01) /* LDX zp */ {
        status << Mnemonic::LDX << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        value = readRamByte(status.operand.u8);
        cycles += 2;
      } else if (status->getAddrMode() == 0x02) /* TAX */ {
        status << Mnemonic::TAX << preExecHook(status);

        value = m_regs.A;
        cycles += 1;
      } else if (status->getAddrMode() == 0x03) /* LDX abs */ {
        status << Mnemonic::LDX << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        value = readRamByte(status.operand.u16);
        cycles += 3;
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw 1;
      } else if (status->getAddrMode() == 0x05) /* LDX zp,Y */ {
        status << Mnemonic::LDX << AddrMode::ZeroPageY << readPC<uint8_t>() << preExecHook(status);

        value = readRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.Y));
        cycles += 3;
      } else if (status->getAddrMode() == 0x06) /* TSX */ {
        status << Mnemonic::TSX << preExecHook(status);

        value = m_regs.SP;
        cycles += 1;
      } else if (status->getAddrMode() == 0x07) /* LDX abs,Y */ {
        status << Mnemonic::LDX << AddrMode::AbsoluteY << readPC<uint16_t>() << preExecHook(status);

        auto target_addr = status.operand.u16 + m_regs.Y;

        value = readRamByte(target_addr);
        cycles += 3;
        if ((status.operand.u16 & 0xFF00) != (target_addr & 0xFF00)) {
          cycles += 1;
        }
      }

      m_regs.X   = value;
      m_regs.P.Z = m_regs.X == 0;
      m_regs.P.N = (m_regs.X & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x06: {
      uint8_t resultValue = 0, cycles = 0;

      status << Mnemonic::DEC;
      if (status->getAddrMode() == 0x00) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* DEC zp */ {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        cycles      = 5;
        resultValue = writeRamByte(status.operand.u8, readRamByte(status.operand.u8) - 1);
      } else if (status->getAddrMode() == 0x02) /* DEX */ {
        status << Mnemonic::DEX << preExecHook(status);

        m_regs.X -= 1;
        m_regs.P.Z = m_regs.X == 0;
        m_regs.P.N = (m_regs.X & 0x80) > 0;
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* DEC abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        cycles      = 6;
        resultValue = writeRamByte(status.operand.u16, readRamByte(status.operand.u16) - 1);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x05) /* DEC zp,X */ {
        status << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        auto const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

        cycles      = 7;
        resultValue = writeRamByte(addr, readRamByte(addr) - 1);
      } else if (status->getAddrMode() == 0x06) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* DEC abs,X */ {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        auto const addr = status.operand.u16 + m_regs.X;

        cycles      = 7;
        resultValue = writeRamByte(addr, readRamByte(addr) - 1);
      }

      m_regs.P.Z = resultValue == 0;
      m_regs.P.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
    case 0x07: {
      uint8_t resultValue = 0, cycles = 0;

      status << Mnemonic::INC;
      if (status->getAddrMode() == 0x00) /* NOP imm */ {
        status << Mnemonic::NOP << AddrMode::Immediate << readPC<uint8_t>() << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x01) /* INC zp */ {
        status << AddrMode::ZeroPage << readPC<uint8_t>() << preExecHook(status);

        cycles      = 5;
        resultValue = writeRamByte(status.operand.u8, readRamByte(status.operand.u8) + 1);
      } else if (status->getAddrMode() == 0x02) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x03) /* INC abs */ {
        status << AddrMode::Absolute << readPC<uint16_t>() << preExecHook(status);

        cycles      = 6;
        resultValue = writeRamByte(status.operand.u16, readRamByte(status.operand.u16) + 1);
      } else if (status->getAddrMode() == 0x04) /* JAM */ {
        throw;
      } else if (status->getAddrMode() == 0x05) /* INC zp,X */ {
        status << AddrMode::ZeroPageX << readPC<uint8_t>() << preExecHook(status);
        auto const addr = static_cast<uint8_t>(status.operand.u8 + m_regs.X);

        cycles      = 5;
        resultValue = writeRamByte(addr, readRamByte(addr) + 1);
      } else if (status->getAddrMode() == 0x06) /* NOP impl */ {
        status << Mnemonic::NOP << preExecHook(status);
        return postExecHook(status, 2);
      } else if (status->getAddrMode() == 0x07) /* INC abs,X */ {
        status << AddrMode::AbsoluteX << readPC<uint16_t>() << preExecHook(status);
        auto const addr = status.operand.u16 + m_regs.X;

        cycles      = 7;
        resultValue = writeRamByte(addr, readRamByte(addr) + 1);
      }

      m_regs.P.Z = resultValue == 0;
      m_regs.P.N = (resultValue & 0x80) > 0;
      return postExecHook(status, cycles);
    } break;
  }

  throw UnhandledInstruction();
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

  InstructionStatus s {
      .startAddr = m_regs.PC,
      .holder    = readPC<uint8_t>(),
      .operand   = {},
      .flags     = {},
  };

  try {
    if (m_hook) m_hook(s);
    switch (s.holder.deflt.instclass) {
      case InstClass::Control: return handleControl(s); break;
      case InstClass::Math: return handleMath(s); break;
      case InstClass::Shift: return handleShift(s); break;
      default: throw UnhandledInstruction();
    }
  } catch (UnhandledInstruction const& ex) {
    s.flags.stage = ExecStage::FailExec;
    if (m_hook) m_hook(s);
    throw ex; // Rethrow if fail hook is not set or returned
  }
}

void CPU6502::triggerNmi() {
  m_nmitriggered = true;
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

bool CPU6502::preExecHook(InstructionStatus& status) {
  status.flags.stage = ExecStage::PreExec;
  if (m_hook) m_hook(status);
  return true;
}

uint8_t CPU6502::postExecHook(InstructionStatus& status, uint8_t cycles) {
  status.flags.stage = ExecStage::PostExec, status.flags.cycles = cycles;
  if (m_hook) m_hook(status);
  return status.flags.cycles;
}
