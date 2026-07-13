#pragma once

#include <array>
#include <cstdint>
#include <string_view>

enum class AddrMode {
  Illegal,
  Implied,
  IndX,
  IndY,
  ZeroPage,
  ZeroPageX,
  Immediate,
  Absolute,
  AbsoluteX,
  Relative,
  Accum,
};

enum class InstName : uint8_t {
  ILL,
  ADC,
  AND,
  ASL,
  BCC,
  BCS,
  BEQ,
  BIT,
  BMI,
  BNE,
  BPL,
  BRK,
  BVC,
  BVS,
  CLC,
  CLD,
  CLI,
  CLV,
  CMP,
  CPX,
  CPY,
  DEC,
  DEX,
  DEY,
  EOR,
  INC,
  INX,
  INY,
  JMP,
  JSR,
  LDA,
  LDX,
  LDY,
  LSR,
  NOP,
  ORA,
  PHA,
  PHP,
  PLA,
  PLP,
  ROL,
  ROR,
  RTI,
  RTS,
  SBC,
  SEC,
  SED,
  SEI,
  STA,
  STX,
  STY,
  TAX,
  TAY,
  TSX,
  TXA,
  TXS,
  TYA,
};

constexpr std::string_view CPU_INST_NAME(InstName i) {
  switch (i) {
    case InstName::ADC: return "ADC";
    case InstName::AND: return "AND";
    case InstName::ASL: return "ASL";
    case InstName::BCC: return "BCC";
    case InstName::BCS: return "BCS";
    case InstName::BEQ: return "BEQ";
    case InstName::BIT: return "BIT";
    case InstName::BMI: return "BMI";
    case InstName::BNE: return "BNE";
    case InstName::BPL: return "BPL";
    case InstName::BRK: return "BRK";
    case InstName::BVC: return "BVC";
    case InstName::BVS: return "BVS";
    case InstName::CLC: return "CLC";
    case InstName::CLD: return "CLD";
    case InstName::CLI: return "CLI";
    case InstName::CLV: return "CLV";
    case InstName::CMP: return "CMP";
    case InstName::CPX: return "CPX";
    case InstName::CPY: return "CPY";
    case InstName::DEC: return "DEC";
    case InstName::DEX: return "DEX";
    case InstName::DEY: return "DEY";
    case InstName::EOR: return "EOR";
    case InstName::INC: return "INC";
    case InstName::INX: return "INX";
    case InstName::INY: return "INY";
    case InstName::JMP: return "JMP";
    case InstName::JSR: return "JSR";
    case InstName::LDA: return "LDA";
    case InstName::LDX: return "LDX";
    case InstName::LDY: return "LDY";
    case InstName::LSR: return "LSR";
    case InstName::NOP: return "NOP";
    case InstName::ORA: return "ORA";
    case InstName::PHA: return "PHA";
    case InstName::PHP: return "PHP";
    case InstName::PLA: return "PLA";
    case InstName::PLP: return "PLP";
    case InstName::ROL: return "ROL";
    case InstName::ROR: return "ROR";
    case InstName::RTI: return "RTI";
    case InstName::RTS: return "RTS";
    case InstName::SBC: return "SBC";
    case InstName::SEC: return "SEC";
    case InstName::SED: return "SED";
    case InstName::SEI: return "SEI";
    case InstName::STA: return "STA";
    case InstName::STX: return "STX";
    case InstName::STY: return "STY";
    case InstName::TAX: return "TAX";
    case InstName::TAY: return "TAY";
    case InstName::TSX: return "TSX";
    case InstName::TXA: return "TXA";
    case InstName::TXS: return "TXS";
    case InstName::TYA: return "TYA";
    default: return "???";
  }
}

struct InstItem {
  InstName name;
  AddrMode mode;
  uint8_t  size;
  uint8_t  cycles;

  consteval InstItem(): name(InstName::ILL), mode(AddrMode::Illegal), size(1), cycles(0) {}

  consteval InstItem(InstName n, AddrMode a, uint8_t s, uint8_t c): name(n), mode(a), size(s), cycles(c) {}
};

constexpr std::array<InstItem, 256> CPU_INSTS = {
    InstItem(InstName::BRK, AddrMode::Implied, 1, 7),
    InstItem(/* InstName::ORA, AddrMode::IndX, 2, 6 */),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(/* InstName::ORA, AddrMode::ZeroPage, 2, 3 */),
    InstItem(/* InstName::ASL, AddrMode::ZeroPage, 2, 5 */),
    InstItem(),
    InstItem(InstName::PHP, AddrMode::Implied, 1, 3),
    InstItem(/* InstName::ORA, AddrMode::Immediate, 2, 2 */),
    InstItem(InstName::ASL, AddrMode::Accum, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::BPL, AddrMode::Relative, 2, 2 /* TODO: Branching +1 cycle if branch on same page, +2 on different */),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::CLC, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::JSR, AddrMode::Absolute, 3, 6),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::PLP, AddrMode::Implied, 1, 4),
    InstItem(InstName::AND, AddrMode::Immediate, 2, 2),
    InstItem(),
    InstItem(),
    InstItem(InstName::BIT, AddrMode::Absolute, 3, 4),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::PHA, AddrMode::Implied, 1, 3),
    InstItem(),
    InstItem(InstName::LSR, AddrMode::Accum, 1, 2),
    InstItem(),
    InstItem(InstName::JMP, AddrMode::Absolute, 3, 3),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::EOR, AddrMode::AbsoluteX, 3, 4),
    InstItem(),
    InstItem(),
    InstItem(InstName::RTS, AddrMode::Implied, 1, 6),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::PLA, AddrMode::Implied, 1, 4),
    InstItem(InstName::ADC, AddrMode::Immediate, 2, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::SEI, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::TSX, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::STY, AddrMode::ZeroPage, 2, 3),
    InstItem(InstName::STA, AddrMode::ZeroPage, 2, 3),
    InstItem(InstName::STX, AddrMode::ZeroPage, 2, 3),
    InstItem(),
    InstItem(InstName::DEY, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(InstName::TXA, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(InstName::STA, AddrMode::Absolute, 3, 4),
    InstItem(InstName::STX, AddrMode::Absolute, 3, 4),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::STA, AddrMode::ZeroPageX, 2, 4),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::TXS, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(InstName::STA, AddrMode::AbsoluteX, 3, 5),
    InstItem(),
    InstItem(),
    InstItem(InstName::LDY, AddrMode::Immediate, 2, 2),
    InstItem(),
    InstItem(InstName::LDX, AddrMode::Immediate, 2, 2),
    InstItem(),
    InstItem(),
    InstItem(InstName::LDA, AddrMode::ZeroPage, 2, 3),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::LDA, AddrMode::Immediate, 2, 2),
    InstItem(InstName::TAX, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(InstName::LDA, AddrMode::Absolute, 3, 4),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::LDA, AddrMode::IndY, 2, 5),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::LDY, AddrMode::AbsoluteX, 3, 4),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::DEX, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::BNE, AddrMode::Relative, 2, 2 /* TODO: Branching +1 cycle if branch on same page, +2 on different */),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::CLD, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::CPX, AddrMode::Immediate, 2, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::INC, AddrMode::ZeroPage, 2, 5),
    InstItem(),
    InstItem(InstName::INX, AddrMode::Implied, 1, 2),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(InstName::BEQ, AddrMode::Relative, 2, 2 /* +1 if on the same page, +2 if on different */),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
    InstItem(),
};
