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

  static float               updateTimer = 1.f;
  static ImGuiHexEditorState hexEditor   = {
      .UserData = &nes, /* safe to pass once, this reference never change */
  };

  bool open = true;

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_Once);
  if (ImGui::Begin("Debug your peNES", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
    if (ImGui::BeginTabBar("DbgTabs")) {
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
        if (hexEditor.Bytes != (void*)1) {
          hexEditor.Bytes        = (void*)1;
          hexEditor.MaxBytes     = sizeof(CPU6502::CPUState::ram);
          hexEditor.ReadCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
            if (size < 0 || offset < 0) return 0;
            auto const nes = static_cast<Console*>(state->UserData);

            auto const lock = nes->mkLock<Console::SharedLock>();

            auto const& cpuState = nes->_cpu.exposeState();
            std::memcpy(buf, cpuState.ram.data() + offset, size);
            return cpuState.ram.size();
          };
          hexEditor.WriteCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
            if (size < 0 || offset < 0) return 0;
            auto const nes = static_cast<Console*>(state->UserData);

            auto const lock = nes->mkLock<Console::GuardLock>();

            auto& cpuState = nes->_cpu.exposeState();
            std::memcpy(cpuState.ram.data() + offset, buf, size);
            return size;
          };
        }

        ImGui::BeginHexEditor("##RamEditor", &hexEditor);
        ImGui::EndHexEditor();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("PRG RAM")) {
        if (hexEditor.Bytes != (void*)2) {
          auto const lock = nes.mkLock<Console::SharedLock>();

          hexEditor.Bytes        = (void*)2;
          hexEditor.MaxBytes     = nes._cartridge->hdr.getPrgRamSize();
          hexEditor.ReadCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
            if (size < 0 || size > 255 || offset < 0) return 0;

            auto const nes = static_cast<Console*>(state->UserData);

            auto const lock = nes->mkLock<Console::SharedLock>();

            CPU6502::EvalAddress addr((uint16_t)(0x6000 + offset));
            for (uint16_t i = 0; i < size; ++i) {
              addr.offset        = i;
              ((uint8_t*)buf)[i] = nes->_cpu.readMem<uint8_t>(addr);
            }

            return size;
          };
          hexEditor.WriteCallback = [](ImGuiHexEditorState* state, int32_t offset, void* buf, int32_t size) -> int32_t {
            if (size < 0 || size > 255 || offset < 0) return 0;
            auto nes = static_cast<Console*>(state->UserData);

            auto const lock = nes->mkLock<Console::GuardLock>();

            CPU6502::EvalAddress addr((uint16_t)(0x6000 + offset));
            for (uint16_t i = 0; i < size; ++i) {
              addr.offset = i;
              nes->_cpu.writeMem(addr, ((uint8_t*)buf)[i]);
            }

            return size;
          };
        }

        if (hexEditor.MaxBytes > 0) {
          ImGui::BeginHexEditor("##ProgRamEditor", &hexEditor);
          ImGui::EndHexEditor();
        } else {
          ImGui::Text("This ROM has no PRG-RAM block");
        }

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
