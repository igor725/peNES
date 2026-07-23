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
    Status   SR;   // Processor status
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
  };

  public:
  static constexpr uint32_t BASE_CLOCK_FREQUENCY = 1789773;

  struct CPUState {
    Registers regs;

    struct {
      bool nmiTriggered : 1;
      bool irqTriggered : 1;
      bool intrClrSchd  : 1;
    };

    uint8_t padding[6];

    std::array<uint8_t, 0x800> ram;
  };

  enum class ExecStage : uint8_t {
    PreParse, // Parsing stage, only instruction opcode is known at this stage
    PreExec,  // Pre execution stage; mnemonic, addressing mode and operand are ready to read
    PostExec, // Instruction was successfuly executed and cycles counter was altered accordingly
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

  CPU6502();
  ~CPU6502();

  template <typename T>
  void pushStack(T val) {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);
    if constexpr (sizeof(T) == 1) {
      m_state.ram[0x100 + m_state.regs.SP--] = val;
    } else if constexpr (sizeof(T) == 2) {
      pushStack<uint8_t>((val >> 8) & 0xFF);
      pushStack<uint8_t>(val & 0xFF);
    }
  }

  template <typename T>
  T popStack() {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);
    if constexpr (sizeof(T) == 1) {
      return m_state.ram[0x100 | ++m_state.regs.SP];
    } else if constexpr (sizeof(T) == 2) {
      auto const _low  = popStack<uint8_t>();
      auto const _high = popStack<uint8_t>();
      return (_high << 8) | _low;
    }
  }

  void pushStatus(bool software) {
    auto p = m_state.regs.SR;

    p.B = software;
    pushStack<uint8_t>(p._raw);
  }

  Status popStatus() {
    Status p = {._raw = popStack<uint8_t>()};

    p.B = false;
    return p;
  }

  struct EvalAddress {
    bool    zeroPage    : 1 = false;
    bool    validAddr   : 1 = true;
    bool    buggy       : 1 = false;
    uint8_t cyclesTaken : 4 = 0;

    uint8_t  offset      = 0;
    uint16_t baseAddress = 0;

    inline uint16_t getAddress() const {
      if (buggy) return (baseAddress & 0xFF00) | static_cast<uint8_t>((baseAddress & 0xFF) + offset);
      return zeroPage ? static_cast<uint8_t>(baseAddress + offset) : (baseAddress + offset) & 0xFFFF;
    }

    inline bool isPageCrossed() const { return zeroPage ? false : (baseAddress & 0xFF00) != (getAddress() & 0xFF00); }

    inline EvalAddress&& applyPageTax(InstructionStatus const& status) {
      if (isPageCrossed()) {
        switch (status.flags.mnemonic) {
          case Mnemonic::STA: break;
          case Mnemonic::ASL: break;
          case Mnemonic::SRE: break;
          case Mnemonic::LSR: break;
          case Mnemonic::ROR: break;
          case Mnemonic::ROL: break;
          case Mnemonic::DEC: break;
          case Mnemonic::INC: break;
          case Mnemonic::RRA: break;
          default: cyclesTaken += 1; break;
        }
      }

      return std::move(*this);
    }

    inline EvalAddress buildMoreOffset(uint8_t offset) const {
      EvalAddress tmp {*this};
      tmp.offset += offset;
      return tmp;
    }

    inline EvalAddress(): validAddr(false) {}

    inline EvalAddress(const EvalAddress&)            = default;
    inline EvalAddress& operator=(const EvalAddress&) = default;

    inline EvalAddress(EvalAddress&&) noexcept            = default;
    inline EvalAddress& operator=(EvalAddress&&) noexcept = default;

    inline EvalAddress(uint8_t c, uint16_t a, uint8_t o = 0, bool z = false, bool b = false)
        : cyclesTaken(c), baseAddress(a), offset(o), zeroPage(z), buggy(b) {}

    inline EvalAddress(uint16_t addr): cyclesTaken(0), baseAddress(addr), zeroPage(addr < 0xFF) {}
  };

  template <typename T>
  struct EvalValue: public EvalAddress {
    T value = {};

    inline EvalValue(EvalAddress&& addr, T v): EvalAddress(std::move(addr)), value(std::move(v)) {}

    inline EvalValue(EvalAddress&& addr): EvalAddress(std::move(addr)) {}
  };

  static void VerboseTesterHook(InstructionStatus& status);
  static void TesterHook(InstructionStatus& status);
  static void HeatMapHook(InstructionStatus& status);
  static void UsedInstructionsHook(InstructionStatus& status);
  static void SetHeatMapReportThreshold(uint64_t th);

  void    reset();
  uint8_t step();
  void    triggerNMI();
  void    triggerIRQ();

  Registers& exposeState() { return m_state.regs; }

  void setHook(CPUHook&& hook) { m_hook = std::move(hook); }

  template <typename T>
  T readMem(EvalAddress const& addr) const {
    static_assert(std::is_integral_v<T> && sizeof(T) <= 2);

    if constexpr (sizeof(T) == 1) {
      return static_cast<T>(readMemByte(addr));
    } else if constexpr (sizeof(T) == 2) {
      return static_cast<T>((readMemByte(addr.buildMoreOffset(1)) << 8) | readMemByte(addr));
    }
  }

  template <typename T>
  T readPC() {
    auto orig = m_state.regs.PC;
    m_state.regs.PC += sizeof(T);
    return readMem<T>(orig);
  }

  CPUState dumpState() const { return m_state; }

  void restoreState(CPUState&& state) { m_state = std::move(state); }

  protected:
  inline EvalAddress evaluateOperandToAddr(InstructionStatus const& status) {
    if (status.flags.stage != ExecStage::PreExec) throw;
    switch (status.flags.addrMode) {
      case AddrMode::Accum: throw;
      case AddrMode::Absolute: return EvalAddress(1, status.operand.u16, 0);
      case AddrMode::AbsoluteX: return EvalAddress(1, status.operand.u16, m_state.regs.X).applyPageTax(status);
      case AddrMode::AbsoluteY: return EvalAddress(1, status.operand.u16, m_state.regs.Y).applyPageTax(status);
      case AddrMode::Immediate: return EvalAddress(status.operand.u8);
      case AddrMode::Implied: throw; // Implied operand should not be evaluated
      case AddrMode::Indirect: return EvalAddress(4, status.operand.u16, 0, false, status.flags.mnemonic == Mnemonic::JMP);
      case AddrMode::IndexedXIndir: return EvalAddress(3, readMem<uint16_t>(EvalAddress(0, status.operand.u8, m_state.regs.X, true)), 0);
      case AddrMode::IndirIndexedY: return EvalAddress(2, readMem<uint16_t>(EvalAddress(0, status.operand.u8, 0, true)), m_state.regs.Y).applyPageTax(status);
      case AddrMode::Relative: throw;
      case AddrMode::Invalid: throw;
      case AddrMode::ZeroPage: return EvalAddress(0, status.operand.u8, 0, true);
      case AddrMode::ZeroPageX: return EvalAddress(1, status.operand.u8, m_state.regs.X, true);
      case AddrMode::ZeroPageY: return EvalAddress(1, status.operand.u8, m_state.regs.Y, true);
    }
  }

  template <typename T>
  inline EvalValue<T> evaluateOperandToValue(InstructionStatus const& status) {
    switch (status.flags.addrMode) {
      case AddrMode::Accum: return EvalValue<T> {EvalAddress(), static_cast<T>(m_state.regs.A)};
      case AddrMode::Immediate: return EvalValue<T> {EvalAddress(), static_cast<T>(status.operand.u8)};
      default: break;
    }

    auto eval = EvalValue<T>(evaluateOperandToAddr(status));
    eval.cyclesTaken += sizeof(T);
    eval.value = readMem<T>(eval);
    return eval;
  }

  uint8_t handleControl(InstructionStatus& status);
  uint8_t handleMath(InstructionStatus& status);
  uint8_t handleShift(InstructionStatus& status);
  uint8_t handleIllegal(InstructionStatus& status);
  uint8_t writeMemByte(EvalAddress const& eval, uint8_t value);
  uint8_t readMemByte(EvalAddress const& eval) const;
  void    preExecHook(InstructionStatus& status);
  uint8_t postExecHook(InstructionStatus& status, uint8_t cycles);
  uint8_t interrupt(uint16_t vector, bool software = false);

  private:
  CPUState m_state;
  CPUHook  m_hook;
};
