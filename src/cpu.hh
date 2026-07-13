#pragma once

#include "ines.hh"

#include <cstdint>
#include <exception>
#include <format>
#include <stack>
#include <string>

class IllegalInstruction: public std::exception {
  std::string m_what;

  public:
  IllegalInstruction(int32_t opcode): m_what(std::format("Illegal instruction {0} (${0:x})!", opcode)) {}

  const char* what() const noexcept override { return m_what.c_str(); }
};

class UnhandledInstruction: public std::exception {
  std::string m_what;

  public:
  UnhandledInstruction(int32_t opcode): m_what(std::format("Unhandled instruction {0} (${0:x})!", opcode)) {}

  const char* what() const noexcept override { return m_what.c_str(); }
};

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

  struct [[gnu::packed]] Instruction {
    uint8_t opcode;

    union {
      int8_t   s_operand;
      int16_t  s_operandw;
      uint8_t  operand;
      uint16_t operandw;
    };
  };

  public:
  CPU6502();
  ~CPU6502();

  bool loadCartridge(iNES const& c);

  template <typename T>
  void pushStack(T val) {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);
    if constexpr (sizeof(T) == 1) {
      m_stack.push(val);
    } else if constexpr (sizeof(T) == 2) {
      m_stack.push((val >> 8) & 0xFF);
      m_stack.push(val & 0xFF);
    }
  }

  template <typename T>
  T popStack() {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);
    if constexpr (sizeof(T) == 1) {
      auto v = m_stack.top();
      m_stack.pop();
      return v;
    } else if constexpr (sizeof(T) == 2) {
      auto _low  = popStack<uint8_t>();
      auto _high = popStack<uint8_t>();
      return (_high << 8) | _low;
    }
  }

  void    reset();
  uint8_t step();
  void    writeRam(uint16_t addr, uint8_t value);
  uint8_t readRam(uint16_t addr) const;

  private:
  struct {
    uint8_t A; // Accumulator

    union {
      Status  P;
      uint8_t bP;
    }; // Processor status

    uint16_t PC;   // Program counter
    uint8_t  S;    // Stack pointer
    uint8_t  X, Y; // Indices
  } m_regs;

  bool m_verbose;

  uint8_t m_ram[65536];

  std::stack<uint8_t> m_stack;
};
