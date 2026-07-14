#pragma once

#include "mmu.hh"

#include <array>
#include <cstdint>
#include <exception>
#include <format>
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

class CPU6502: public MMU {
  union Status {
    struct {
      uint8_t C : 1;
      uint8_t Z : 1;
      uint8_t I : 1;
      uint8_t D : 1;
      uint8_t B : 1;
      uint8_t U : 1;
      uint8_t V : 1;
      uint8_t N : 1;
    };

    uint8_t _raw;
  };

  enum class InstClass : uint8_t {
    Control,
    Math,
    Shift,
  };

  union [[gnu::packed]] Instruction {
    struct {
      InstClass instclass : 2;
      uint8_t   addrmode  : 3;
      uint8_t   operation : 3;
    } deflt;

    struct {
      uint8_t sig  : 5;
      uint8_t cond : 1;
      uint8_t sel  : 2;
    } branch;

    uint8_t raw;

    uint8_t getAddrMode() const { return deflt.addrmode; }
  };

  public:
  CPU6502();
  ~CPU6502();

  template <typename T>
  void pushStack(T val) {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);
    if constexpr (sizeof(T) == 1) {
      m_ram[0x100 + m_regs.SP--] = val;
    } else if constexpr (sizeof(T) == 2) {
      pushStack<uint8_t>((val >> 8) & 0xFF);
      pushStack<uint8_t>(val & 0xFF);
    }
  }

  template <typename T>
  T popStack() {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);
    if constexpr (sizeof(T) == 1) {
      return m_ram[0x100 | ++m_regs.SP];
    } else if constexpr (sizeof(T) == 2) {
      auto _low  = popStack<uint8_t>();
      auto _high = popStack<uint8_t>();
      return (_high << 8) | _low;
    }
  }

  void    reset();
  uint8_t step();
  void    triggerNmi();

  template <typename T>
  T readRam(uint16_t addr) const {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);

    if constexpr (sizeof(T) == 1) {
      return static_cast<T>(readRamByte(addr));
    } else if constexpr (sizeof(T) == 2) {
      if (addr == 0xFFFF) throw;
      auto _low  = readRamByte(addr);
      auto _high = readRamByte(addr + 1);
      return static_cast<T>((_high << 8) | _low);
    }
  }

  template <typename T>
  T readPC() {
    auto orig = m_regs.PC;
    m_regs.PC += sizeof(T);
    return readRam<T>(orig);
  }

  protected:
  uint8_t handleControl(Instruction inst);
  uint8_t handleMath(Instruction inst);
  uint8_t handleShift(Instruction inst);
  uint8_t writeRamByte(uint16_t addr, uint8_t value);
  uint8_t readRamByte(uint16_t addr) const;

  private:
  struct {
    uint8_t  A;    // Accumulator
    Status   P;    // Processor status
    uint16_t PC;   // Program counter
    uint8_t  SP;   // Stack pointer
    uint8_t  X, Y; // Indices
  } m_regs;

  bool    m_nmitriggered;
  uint8_t m_padButtons, m_padCounter;

  std::array<uint8_t, 0x8000> m_ram;
};
