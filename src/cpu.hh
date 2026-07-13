#pragma once

#include "ines.hh"

#include <cstdint>
#include <exception>
#include <format>
#include <string>

class IllegalInstruction: public std::exception {
  std::string m_what;

  public:
  IllegalInstruction(int32_t opcode): m_what(std::format("Illegal instruction {} ({:x})!", opcode, opcode)) {}

  const char* what() const noexcept override { return m_what.c_str(); }
};

class UnhandledInstruction: public std::exception {};

class CPU6502 {
  struct Status {
    uint8_t N : 1;
    uint8_t V : 1;
    uint8_t B : 1;
    uint8_t D : 1;
    uint8_t I : 1;
    uint8_t Z : 1;
    uint8_t C : 1;
  };

  struct Instruction {
    uint8_t opcode;

    union {
      uint8_t  operand;
      uint16_t operandw;
    };
  };

  public:
  CPU6502();
  ~CPU6502();

  bool loadCartridge(iNES const& c);
  void reset();
  void step();

  private:
  uint8_t m_ram[0xffff];

  struct {
    uint8_t  A;    // Accumulator
    Status   P;    // Processor status
    uint16_t PC;   // Program counter
    uint8_t  S;    // Stack pointer
    uint8_t  X, Y; // Indices
  } m_regs;
};
