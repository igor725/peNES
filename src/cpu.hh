#pragma once

#include "mmu.hh"

#include <array>
#include <cstdint>
#include <string>

class CPU6502: public MMU {
  union Status {
    struct {
      uint8_t C : 1;
      uint8_t Z : 1;
      uint8_t I : 1;
      uint8_t D : 1;
      uint8_t B : 1;
      uint8_t   : 1;
      uint8_t V : 1;
      uint8_t N : 1;
    };

    uint8_t _raw;
  };

  enum class InstClass : uint8_t {
    Control,
    Math,
    Shift,
    Unknown,
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

      inline bool isValid() const { return sig == 0x10; }
    } branch;

    uint8_t raw;

    uint8_t getAddrMode() const { return deflt.addrmode; }

    Instruction(): raw(0) {}

    Instruction(uint8_t r): raw(r) {}

    Instruction(InstClass cl, uint8_t md, uint8_t op): deflt({cl, md, op}) {}
  };

  union Operand {
    uint8_t  u8;
    int8_t   s8;
    uint16_t u16;
    int16_t  s16;
  };

  public:
  enum class ExecStage : uint8_t {
    PreParse, // Parsing stage, only instruction opcode is known at this stage
    PreExec,  // Pre execution stage; mnemonic, addressing mode and operand are ready to read
    PostExec, // Instruction was successfuly executed and cycles counter was altered accordingly
    FailExec, // Instruction failed to execute, CPU panicked further execution is unsafe
  };

  enum class AddrMode : uint8_t {
    Accum,
    Absolute,
    AbsoluteX,
    AbsoluteY,
    Immediate,
    Implied,
    Indirect,
    IndexedXIndir,
    IndirIndexedY,
    Relative,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
  };

  enum class Mnemonic : uint8_t {
    UNK,
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

  struct InstructionStatus {
    uint16_t    startAddr = 0;
    Instruction holder    = {};
    AddrMode    addrMode  = AddrMode::Implied;
    Operand     operand   = {};

    struct [[gnu::packed]] {
      Mnemonic  mnemonic    : 8 = Mnemonic::UNK;
      ExecStage stage       : 2 = ExecStage::PreParse;
      bool      addrModeSet : 1 = false;
      bool      operandSet  : 1 = false;
      uint8_t   cycles      : 4 = 0;
    } flags = {};

    inline Instruction* operator->() { return &holder; }

    InstructionStatus& operator<<(Mnemonic mn) {
      flags.addrModeSet = true, flags.mnemonic = mn;
      return *this;
    }

    InstructionStatus& operator<<(AddrMode md) {
      flags.addrModeSet = true, addrMode = md;
      return *this;
    }

    InstructionStatus& operator<<(uint8_t op) {
      flags.operandSet = true, operand.u8 = op;
      return *this;
    }

    InstructionStatus& operator<<(int8_t op) {
      flags.operandSet = true, operand.s8 = op;
      return *this;
    }

    InstructionStatus& operator<<(uint16_t op) {
      flags.operandSet = true, operand.u16 = op;
      return *this;
    }

    InstructionStatus& operator<<(int16_t op) {
      flags.operandSet = true, operand.s16 = op;
      return *this;
    }

    void operator<<(bool) { /* Dummy pre-exec result handler */ }

    std::string buildMnemonic(bool withAddr = false) const;
  };

  using CPUHook = std::function<void(InstructionStatus&)>;

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

  void pushStatus(bool software) {
    auto p = m_regs.P;

    p.B = software;
    pushStack<uint8_t>(p._raw);
  }

  static void VerboseTesterHook(InstructionStatus& status);
  static void TesterHook(InstructionStatus& status);
  static void HeatMapHook(InstructionStatus& status);
  static void SetHeatMapReportThreshold(uint64_t th);

  void    reset();
  uint8_t step();
  void    triggerNMI();
  void    triggerIRQ();

  void setHook(CPUHook&& hook) { m_hook = std::move(hook); }

  template <typename T>
  T readRam(uint16_t addr) const {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);

    if constexpr (sizeof(T) == 1) {
      return static_cast<T>(readRamByte(addr));
    } else if constexpr (sizeof(T) == 2) {
      if (addr == 0xFFFF) throw;
      return static_cast<T>((readRamByte(addr + 1) << 8) | readRamByte(addr));
    }
  }

  template <typename T>
  T readPC() {
    auto orig = m_regs.PC;
    m_regs.PC += sizeof(T);
    return readRam<T>(orig);
  }

  protected:
  uint8_t handleControl(InstructionStatus& status);
  uint8_t handleMath(InstructionStatus& status);
  uint8_t handleShift(InstructionStatus& status);
  uint8_t writeRamByte(uint16_t addr, uint8_t value);
  uint8_t readRamByte(uint16_t addr) const;
  bool    preExecHook(InstructionStatus& status);
  uint8_t postExecHook(InstructionStatus& status, uint8_t cycles);
  uint8_t interrupt(uint16_t vector, bool software = false);

  private:
  struct {
    uint8_t  A;    // Accumulator
    Status   P;    // Processor status
    uint16_t PC;   // Program counter
    uint8_t  SP;   // Stack pointer
    uint8_t  X, Y; // Indices
  } m_regs;

  struct {
    bool m_nmiTriggered : 1;
    bool m_irqTriggered : 1;
    bool m_intrClrSchd  : 1;
  };

  uint8_t m_padButtons, m_padCounter, m_resetTimer;

  uint8_t m_padding[5];

  std::array<uint8_t, 0x8000> m_ram;
  CPUHook                     m_hook;
};
