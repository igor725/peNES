#include "cpu.hh"

#include <iostream>

void CPU6502::TesterHook(InstructionStatus& status) {
  static uint8_t tester = 0;

  switch (status.flags.stage) {
    case CPU6502::ExecStage::PreParse: {
      if (tester++ != 0) abort();
    } break;
    case CPU6502::ExecStage::PreExec: {
      if (tester++ != 1) abort();
    } break;
    case CPU6502::ExecStage::PostExec: {
      if (tester != 2) abort();
      tester = 0;
    } break;
    case CPU6502::ExecStage::FailExec: {
      std::cerr << "!FAIL " << status.buildMnemonic() << std::endl;
      abort();
    } break;
  }
}

void CPU6502::VerboseTesterHook(InstructionStatus& status) {
  static uint8_t tester = 0;

  switch (status.flags.stage) {
    case CPU6502::ExecStage::PreParse: {
      if (tester++ != 0) abort();
    } break;
    case CPU6502::ExecStage::PreExec: {
      if (tester++ != 1) abort();
      std::cerr << status.buildMnemonic();
    } break;
    case CPU6502::ExecStage::PostExec: {
      if (tester != 2) abort();
      std::cerr << " @ " << (int)status.flags.cycles << std::endl;
      tester = 0;
    } break;
    case CPU6502::ExecStage::FailExec: {
      std::cerr << " ! " << std::endl;
      abort();
    } break;
  }
}
