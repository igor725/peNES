#include "gbase.hh"

#include "../console.hh"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <imgui.h>
#include <imgui_hex.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>
#include <optional>

class SavesSystem {
  struct FullState {
    std::string          dateName;
    CPU6502::CPUState    cpuState;
    PPU::PPUState        ppuState;
    APU::APUState        apuState;
    std::vector<uint8_t> mapperState;
  };

  SavesSystem() {}

  public:
  static constexpr int8_t MAX_SLOTS = 10;

  bool hasSave(int8_t slot) const { return slot < MAX_SLOTS && m_saves[slot].has_value(); }

  std::string_view getName(int8_t slot) {
    if (slot < 0 || slot >= MAX_SLOTS) return {};
    return m_saves[slot]->dateName;
  }

  void save(Console& nes, int8_t slot) {
    if (slot >= MAX_SLOTS) return;

    auto const lock = nes.mkLock<Console::SharedLock>();

    FullState state {
        .dateName = std::format(
            "{:%F %T}", std::chrono::zoned_time {std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())}),
        .cpuState    = nes._cpu.dumpState(),
        .ppuState    = nes._ppu.dumpState(),
        .apuState    = nes._apu.dumpState(),
        .mapperState = nes._cartridge.getMapper()->dumpState(),
    };

    if (slot == -1) {
      std::move_backward(m_saves.begin(), m_saves.end() - 1, m_saves.end());
      m_saves[0] = std::move(state);
    } else if (slot >= 0) {
      m_saves[slot] = std::move(state);
    }
  }

  void restore(Console& nes, int8_t slot) {
    if (slot < 0 || slot >= MAX_SLOTS) return;

    auto const lock = nes.mkLock<Console::UniqueLock>();

    auto const& state = m_saves.at(slot);
    if (!state.has_value()) return;
    nes._cpu.restoreState(state->cpuState);
    nes._ppu.restoreState(state->ppuState);
    nes._apu.restoreState(state->apuState);
    nes._cartridge.getMapper()->restoreState(state->mapperState);
  }

  static SavesSystem& get() {
    static SavesSystem inst;
    return inst;
  }

  private:
  std::array<std::optional<FullState>, MAX_SLOTS> m_saves;
};

void GUI::init(SDL_Window* wnd, SDL_Renderer* rend, SDL_AudioStream* strm) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;

  ImGui_ImplSDL3_InitForSDLRenderer(wnd, rend);
  ImGui_ImplSDLRenderer3_Init(rend);
  _rend = rend, _astrm = strm;
  updateTheme();
}

void GUI::savestateAction(Console& nes, SaveAction sa, int8_t slot) {
  auto& ss = SavesSystem::get();

  switch (sa) {
    case SaveAction::Save: {
      ss.save(nes, slot);
    } break;
    case SaveAction::Load: {
      ss.restore(nes, slot);
    } break;
  }
}

void GUI::updateTheme() {
  if (SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT) /* User is a masochist */ {
    ImGui::StyleColorsLight();
  } else /* All ok with the user's head (maybe, there's possibility of "unknown theme") */ {
    ImGui::StyleColorsDark();
  }
}

void GUI::forwardEvent(SDL_Event const* ev) {
  ImGui_ImplSDL3_ProcessEvent(ev);
}

void GUI::triggerCPUHaltSequence() {
  _cpuHalt = true;
}

bool GUI::produceFrame(Console& nes) {
  auto& io = ImGui::GetIO();

  static float               updateTimer = 1.f;
  static ImGuiHexEditorState hexEditor   = {
      .Bytes        = (void*)-1,
      .BytesPerLine = 16,
      .AddressChars = 5,
      .UserData     = &nes, /* safe to pass once, this reference never change */
      .ReadCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
        if (size < 0 || size > 255 || offset < 0) return 0;

        auto const nes = static_cast<Console*>(state->UserData);

        auto const lock = nes->mkLock<Console::SharedLock>();

        CPU6502::EvalAddress addr((uint16_t)((size_t)state->Bytes + offset));
        for (uint16_t i = 0; i < size; ++i) {
          addr.offset        = i;
          ((uint8_t*)buf)[i] = nes->_cpu.readMem<uint8_t>(addr);
        }

        return size;
      },
      .WriteCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
        if (size < 0 || size > 255 || offset < 0) return 0;
        auto nes = static_cast<Console*>(state->UserData);

        auto const lock = nes->mkLock<Console::GuardLock>();

        CPU6502::EvalAddress addr((uint16_t)((size_t)state->Bytes + offset));
        for (uint16_t i = 0; i < size; ++i) {
          addr.offset = i;
          nes->_cpu.writeMem(addr, ((uint8_t*)buf)[i]);
        }

        return size;
      },
      .GetAddressNameCallback = [](ImGuiHexEditorState* state, int32_t offset, char* buf, int32_t bufSize) -> bool {
        ImFormatString(buf, (size_t)bufSize, "$%04zX", (size_t)state->Bytes /* Holds base offset of the region */ + (size_t)offset);
        return true;
      },

  };

  auto const drawHexEditor = [](const char* addText = nullptr) {
    char buf[128] = {'\0'};

    if (addText) std::strncpy(buf, addText, sizeof(buf) - 1);
    if (hexEditor.SelectStartByte == hexEditor.SelectEndByte) {
      std::strncat(buf, "Hardware address: $%04zX", sizeof(buf) - 1);
    } else {
      std::strncat(buf, "Hardware address: $%04zX-$%04zX", sizeof(buf) - 1);
    }

    auto const reserveSize = ImGui::CalcTextSize(buf).y + ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginChild("##HexEditorChild", ImVec2(0, -reserveSize))) {
      ImGui::BeginHexEditor("##HexEditor", &hexEditor);
      ImGui::EndHexEditor();
    }
    ImGui::EndChild();

    ImGui::Text(buf, (size_t)hexEditor.Bytes + hexEditor.SelectStartByte, (size_t)hexEditor.Bytes + hexEditor.SelectEndByte);
  };

  bool open = true;

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_Once);
  if (ImGui::Begin("Debug your peNES", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
    if (ImGui::BeginTabBar("DbgTabs")) {
      if (ImGui::BeginTabItem("Savestate")) {
        auto& ss = SavesSystem::get();
        for (int8_t i = 0; i < SavesSystem::MAX_SLOTS; ++i) {
          ImGui::PushID(i);
          ImGui::Text("Slot #%d", i);
          ImGui::SameLine();
          if (ImGui::Button("Save")) {
            ss.save(nes, i);
          }
          ImGui::SameLine();
          auto const hasSave = ss.hasSave(i);
          ImGui::BeginDisabled(!hasSave);
          if (ImGui::Button("Load")) {
            ss.restore(nes, i);
          }
          ImGui::EndDisabled();
          if (hasSave) {
            ImGui::SameLine();
            ImGui::Text("%s", ss.getName(i).data());
          }
          ImGui::PopID();
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("CPU", nullptr, _cpuHalt ? ImGuiTabItemFlags_SetSelected : 0)) {
        static double sSpeed = 0;

        auto const lock = nes.mkLock<Console::GuardLock>();

        if ((updateTimer += io.DeltaTime) > 1.f) {
          sSpeed      = nes._speed;
          updateTimer = 0.f;
        }

        auto const halted = nes._cpu.isHalted();
        ImGui::Text("Current CPU state: %s\nEmulation speed: %.2lf", halted ? "halted" : "running", sSpeed);
        if (ImGui::Button("Reset")) {
          nes._cpu.reset();
        }
        if (halted) {
          _cpuHalt = false;
          ImGui::SameLine();
          if (ImGui::Button("Resume (Dangerous)")) {
            nes._cpu.haltResume();
          }

          auto const& state = nes._cpu.exposeState();

          ImGui::Text("Halt position: src/cpu.cpp:%d", state.haltLine);
          ImGui::Text("Registers:\n  A:$%02X, PC:$%04X, SP:$%02X, X:$%02X, Y:$%02X", state.regs.A, state.regs.PC, state.regs.SP, state.regs.X, state.regs.Y);
          ImGui::Text("Status:\n  C:%X, Z:%X, I:%X, D:%X, B:%X, U:%X, V:%X, N:%X", state.regs.SR.C, state.regs.SR.Z, state.regs.SR.I, state.regs.SR.D,
                      state.regs.SR.B, state.regs.SR.U, state.regs.SR.V, state.regs.SR.N);
        }

        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("RAM")) {
        if (hexEditor.Bytes != (void*)0) {
          hexEditor.SelectStartByte = 0;
          hexEditor.SelectEndByte   = 0;
          hexEditor.Bytes           = (void*)0;
          hexEditor.MaxBytes        = sizeof(CPU6502::CPUState::ram);
        }

        drawHexEditor();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("PRG RAM")) {
        if (hexEditor.Bytes != (void*)0x6000) {
          auto const lock = nes.mkLock<Console::SharedLock>();

          hexEditor.SelectStartByte = 0;
          hexEditor.SelectEndByte   = 0;
          hexEditor.Bytes           = (void*)0x6000;
          hexEditor.MaxBytes        = nes._cartridge->hdr.getPrgRamSize();
        }

        if (hexEditor.MaxBytes > 0) {
          drawHexEditor();
        } else {
          ImGui::Text("This ROM has no PRG-RAM block");
        }

        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("PRG ROM")) {
        if (hexEditor.Bytes != (void*)0x8000) {
          auto const lock = nes.mkLock<Console::SharedLock>();

          hexEditor.SelectStartByte = 0;
          hexEditor.SelectEndByte   = 0;
          hexEditor.Bytes           = (void*)0x8000;
          hexEditor.MaxBytes        = Mapper::PROG_BANK_SIZE * Mapper::PROG_BANKS_NUM;
        }

        drawHexEditor("Note: writing to this space is dangerous, behavior depends on the mapper.\n");
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("APU")) {
        static float volume = SDL_GetAudioStreamGain(_astrm);
        if (ImGui::SliderFloat("Master volume", &volume, 0.f, 1.f)) {
          SDL_SetAudioStreamGain(_astrm, volume);
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("PPU")) {
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
  ImGui::End();

  ImGui::EndFrame();
  ImGui::Render();

  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), _rend);
  return open;
}

void GUI::deinit() {
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
}
