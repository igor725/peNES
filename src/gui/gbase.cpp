#include "gbase.hh"

#include "../console.hh"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_hex.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

void GUI::init(SDL_Window* wnd, SDL_Renderer* rend, SDL_AudioStream* strm) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;

  ImGui_ImplSDL3_InitForSDLRenderer(wnd, rend);
  ImGui_ImplSDLRenderer3_Init(rend);
  _rend = rend, _astrm = strm;
  updateTheme();
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

  static float updateTimer = 1.f;

  bool open = true;

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_Once);
  if (ImGui::Begin("Debug your peNES", &open, ImGuiWindowFlags_NoCollapse)) {
    if (ImGui::BeginTabBar("DbgTabs")) {
      if (ImGui::BeginTabItem("Stats")) {
        static double sSpeed = 0;

        if ((updateTimer += io.DeltaTime) > 1.f) {
          auto const lock = nes.mkLock<Console::SharedLock>();

          sSpeed      = nes._speed;
          updateTimer = 0.f;
        }
        ImGui::Text("Emulation speed: %.2lf", sSpeed);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("CPU", nullptr, _cpuHalt ? ImGuiTabItemFlags_SetSelected : 0)) {
        _cpuHalt = false;

        auto const lock = nes.mkLock<Console::UniqueLock>();

        auto const halted = nes._cpu.isHalted();
        ImGui::Text("Current CPU state: %s", halted ? "Halted" : "Running");
        if (halted) {
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
        static ImGuiHexEditorState ramEditor = {
            .MaxBytes     = sizeof(CPU6502::CPUState::ram),
            .UserData     = &nes, /* safe to pass once, this reference never change */
            .ReadCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
              if (size < 0 || offset < 0) return 0;
              auto const nes = static_cast<Console*>(state->UserData);

              auto const lock = nes->mkLock<Console::SharedLock>();

              auto const& cpuState = nes->_cpu.exposeState();
              std::memcpy(buf, cpuState.ram.data() + offset, size);
              return cpuState.ram.size();
            },
            .WriteCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
              if (size < 0 || offset < 0) return 0;
              auto const nes = static_cast<Console*>(state->UserData);

              auto const lock = nes->mkLock<Console::UniqueLock>();

              auto& cpuState = nes->_cpu.exposeState();
              std::memcpy(cpuState.ram.data() + offset, buf, size);
              return size;
            },
        };

        ImGui::BeginHexEditor("##HexEditor", &ramEditor);
        ImGui::EndHexEditor();

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
