#include "base.hh"

#include "../console.hh"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
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
  if (ev->type == SDL_EVENT_SYSTEM_THEME_CHANGED) updateTheme();
  ImGui_ImplSDL3_ProcessEvent(ev);
}

bool GUI::produceFrame(Console& nes) {
  auto& io = ImGui::GetIO();

  static float  updateTimer = 1.f;
  static double sSpeed      = 0;
  static float  volume      = SDL_GetAudioStreamGain(_astrm);

  bool open = true;

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
  if (ImGui::Begin("Debug your peNES", &open, ImGuiWindowFlags_NoCollapse)) {
    if (ImGui::BeginTabBar("DbgTabs")) {
      if (ImGui::BeginTabItem("Stats")) {
        if ((updateTimer += io.DeltaTime) > 1.f) {
          auto const lock = nes.mkLock<Console::SharedLock>();

          sSpeed      = nes._speed;
          updateTimer = 0.f;
        }
        ImGui::Text("Emulation speed: %.2lf", sSpeed);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("CPU")) {
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("MEM")) {
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("APU")) {
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
