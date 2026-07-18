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

  struct Registers {
    uint8_t  A;    // Accumulator
    Status   P;    // Processor status
    uint16_t PC;   // Program counter
    uint8_t  SP;   // Stack pointer
    uint8_t  X, Y; // Indices
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
    } branch;

    uint8_t raw;

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
    SkipExec, // Instruction execution was skipped either by hook or by CPU itself (illegal instruction)
  };

  enum class AddrMode : uint8_t {
    Invalid,
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

    /* Illegal opcodes */
    UNK,
    SLO,
    RLA,
    SRE,
    RRA,
    SAX,
    LAX,
    DCP,
    ISC,
    SHA,
    SHS,
    SHY,
    SHX,
    LAE,
    ANC,
    ASR,
    ARR,
    ANE,
    LXA,
    AXS,
  };

  struct [[gnu::packed]] InstructionStatus {
    CPU6502&    owner;
    uint16_t    startAddr = 0;
    Instruction holder    = {};
    Operand     operand   = {};

    struct [[gnu::packed]] {
      Mnemonic  mnemonic : 8 = Mnemonic::UNK;
      AddrMode  addrMode : 4 = AddrMode::Invalid;
      ExecStage stage    : 3 = ExecStage::PreParse;
      uint8_t   cycles   : 4 = 0;
      bool      isLegal  : 1 = false;
      bool      skipExec : 1 = false;
    } flags = {};

    InstructionStatus& operator<<(Mnemonic mn) {
      flags.mnemonic = mn, flags.isLegal = static_cast<uint32_t>(mn) < static_cast<uint32_t>(Mnemonic::UNK);
      if (mn == Mnemonic::BRK) owner.readPC<uint8_t>(); // Special case, padding byte
      return *this;
    }

    inline void operator<<(AddrMode md) {
      switch (flags.addrMode = md) {
        case AddrMode::Absolute:
        case AddrMode::AbsoluteX:
        case AddrMode::AbsoluteY:
        case AddrMode::Indirect: operand.u16 = owner.readPC<uint16_t>(); break;

        case AddrMode::Immediate:
        case AddrMode::IndexedXIndir:
        case AddrMode::IndirIndexedY:
        case AddrMode::Relative:
        case AddrMode::ZeroPage:
        case AddrMode::ZeroPageX:
        case AddrMode::ZeroPageY: operand.u8 = owner.readPC<uint8_t>(); break;

        case AddrMode::Accum:
        case AddrMode::Implied: break;

        case AddrMode::Invalid: throw;
      }

      owner.preExecHook(*this);
    }

    inline InstClass getClass() const { return holder.deflt.instclass; }

    inline uint8_t getOpCode() const { return holder.deflt.operation; }

    inline uint8_t getAddrMode() const { return holder.deflt.addrmode; }

    inline uint8_t getBranchingSelection() const { return holder.branch.sel; }

    inline uint8_t getBranchingCondition() const { return holder.branch.cond; }

    inline uint8_t getRawInstructionByte() const { return holder.raw; }

    inline bool isBranching() const { return holder.branch.sig == 0x10; }

    std::string buildMnemonic(bool withAddr = false) const;
  };

  static_assert(sizeof(InstructionStatus) == 16);

  using CPUHook = std::function<void(InstructionStatus&)>;

  static constexpr uint32_t BASE_CLOCK_FREQUENCY = 1789773;

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

  static inline bool _isPageCrossed(uint16_t base, uint16_t target) { return (base & 0xFF00) != (target & 0xFF00); }

  static inline bool _isPageCrossTaxed(InstructionStatus const& status) {
    switch (status.flags.mnemonic) {
      case Mnemonic::STA: return false;
      case Mnemonic::ASL: return false;
      default: return true;
    }
  }

  inline std::pair<uint8_t, uint16_t> _evalIndexedXIndir(InstructionStatus const& status) {
    return {3, (readRamByte(static_cast<uint8_t>((status.operand.u8 + m_regs.X) + 1)) << 8) | readRamByte(static_cast<uint8_t>(status.operand.u8 + m_regs.X))};
  }

  inline std::pair<uint8_t, uint16_t> _evalAbsolute(InstructionStatus const& status) { return {1, status.operand.u16}; }

  inline std::pair<uint8_t, uint16_t> _evalAbsoluteX(InstructionStatus const& status) {
    uint16_t const targetAddr = status.operand.u16 + m_regs.X;
    return {_isPageCrossTaxed(status) && _isPageCrossed(status.operand.u16, targetAddr) ? 2 : 1, targetAddr};
  }

  inline std::pair<uint8_t, uint16_t> _evalAbsoluteY(InstructionStatus const& status) {
    uint16_t const targetAddr = status.operand.u16 + m_regs.Y;
    return {_isPageCrossTaxed(status) && _isPageCrossed(status.operand.u16, targetAddr) ? 2 : 1, targetAddr};
  }

  inline std::pair<uint8_t, uint16_t> _evalIndirIndexedY(InstructionStatus const& status) {
    uint16_t const base   = (readRamByte((status.operand.u8 + 1) & 0xFF) << 8) | readRamByte(status.operand.u8);
    uint16_t const target = base + m_regs.Y;
    return {_isPageCrossTaxed(status) && _isPageCrossed(base, target) ? 3 : 2, target};
  }

  inline std::pair<uint8_t, uint16_t> _evalZeroPageX(InstructionStatus const& status) { return {1, static_cast<uint8_t>(status.operand.u8 + m_regs.X)}; }

  inline std::pair<uint8_t, uint16_t> _evalZeroPageY(InstructionStatus const& status) { return {1, static_cast<uint8_t>(status.operand.u8 + m_regs.Y)}; }

  std::pair<uint8_t, uint16_t> evaluateOperandToAddr(InstructionStatus const& status) {
    if (status.flags.stage != ExecStage::PreExec) throw;
    switch (status.flags.addrMode) {
      case AddrMode::Accum: throw;
      case AddrMode::Absolute: return _evalAbsolute(status);
      case AddrMode::AbsoluteX: return _evalAbsoluteX(status);
      case AddrMode::AbsoluteY: return _evalAbsoluteY(status);
      case AddrMode::Immediate: return {0, status.operand.u8};
      case AddrMode::Implied: throw; // Implied operand should not be evaluated
      case AddrMode::Indirect: throw;
      case AddrMode::IndexedXIndir: return _evalIndexedXIndir(status);
      case AddrMode::IndirIndexedY: return _evalIndirIndexedY(status);
      case AddrMode::Relative: throw;
      case AddrMode::Invalid: throw;
      case AddrMode::ZeroPage: return {0, status.operand.u8};
      case AddrMode::ZeroPageX: return _evalZeroPageX(status);
      case AddrMode::ZeroPageY: return _evalZeroPageY(status);
    }
  }

  template <typename T>
  std::tuple<uint8_t, uint16_t, T> evaluateOperandToValue(InstructionStatus const& status) {
    switch (status.flags.addrMode) {
      case AddrMode::Accum: return {0, 0, m_regs.A};
      case AddrMode::Immediate: return {0, 0, static_cast<T>(status.operand.u8)};
      case AddrMode::Indirect: {
        uint16_t const high = (status.operand.u16 & 0xFF00) | static_cast<uint8_t>((status.operand.u16 & 0xFF) + 1);
        return {4, status.operand.u16, (readRamByte(high) << 8) | readRamByte(status.operand.u16)};
      } break;
      default: break;
    }

    auto const [cycles, addr] = evaluateOperandToAddr(status);
    return {cycles + sizeof(T), addr, readRam<T>(addr)};
  }

  static void VerboseTesterHook(InstructionStatus& status);
  static void TesterHook(InstructionStatus& status);
  static void HeatMapHook(InstructionStatus& status);
  static void UsedInstructionsHook(InstructionStatus& status);
  static void SetHeatMapReportThreshold(uint64_t th);

  void    reset();
  uint8_t step();
  void    triggerNMI();
  void    triggerIRQ();

  Registers& exposeState() { return m_regs; }

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
  uint8_t handleUnknown(InstructionStatus& status);
  uint8_t writeRamByte(uint16_t addr, uint8_t value);
  uint8_t readRamByte(uint16_t addr) const;
  void    preExecHook(InstructionStatus& status);
  uint8_t postExecHook(InstructionStatus& status, uint8_t cycles);
  uint8_t interrupt(uint16_t vector, bool software = false);

  private:
  Registers m_regs;

  struct {
    bool m_nmiTriggered : 1;
    bool m_irqTriggered : 1;
    bool m_intrClrSchd  : 1;
  };

  uint8_t m_bus = 0;

  uint8_t m_padding[6];

  std::array<uint8_t, 0x0800> m_ram;
  CPUHook                     m_hook;
};
