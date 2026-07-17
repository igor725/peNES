#include "cpu.hh"

#include <cstdint>
#include <iostream>
#include <map>

static std::map<uint16_t, uint64_t> HeatMap;
static uint64_t                     ReportThreshold = 0;

void CPU6502::HeatMapHook(InstructionStatus& status) {
  switch (status.flags.stage) {
    case ExecStage::PostExec: {
      auto& hmEnt = HeatMap[status.startAddr];

      hmEnt += 1;
      if (hmEnt <= ReportThreshold) std::cerr << status.buildMnemonic(true) << " / " << hmEnt << std::endl;
    } break;

    default: break;
  }
}

void CPU6502::SetHeatMapReportThreshold(uint64_t th) {
  ReportThreshold = th;
}

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
    case CPU6502::ExecStage::SkipExec: {
      std::cerr << "!SKIP " << status.buildMnemonic() << std::endl;
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
    case CPU6502::ExecStage::SkipExec:
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
