#include "base.hh"

#include "../console.hh"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

void GUI::init(SDL_Window* wnd, SDL_Renderer* rend) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;

  ImGui_ImplSDL3_InitForSDLRenderer(wnd, rend);
  ImGui_ImplSDLRenderer3_Init(rend);
  _rend = rend;
}

void GUI::forwardEvent(SDL_Event const* ev) {
  ImGui_ImplSDL3_ProcessEvent(ev);
}

bool GUI::produceFrame(Console& nes) {
  auto& io = ImGui::GetIO();

  static float  updateTimer = 1.f;
  static double sSpeed      = 0;

  bool open = true;

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
  if (ImGui::Begin("Debug your peNES", &open, ImGuiWindowFlags_NoCollapse)) {
    if (ImGui::BeginTabBar("DbgTabs")) {
      if (ImGui::BeginTabItem("Stats")) {
        if ((updateTimer += io.DeltaTime) > 1.f) {
          auto const lock = nes.lockShared();

          sSpeed      = nes._speed;
          updateTimer = 0.f;
        }
        ImGui::Text("Emulation speed: %.2lf", sSpeed);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("CPU")) {
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("APU")) {
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
