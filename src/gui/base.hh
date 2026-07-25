#pragma once
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

struct Console;

class GUI {
  public:
  void init(SDL_Window* wnd, SDL_Renderer* rend);
  void forwardEvent(SDL_Event const*);
  bool produceFrame(Console& nes);
  void deinit();

  private:
  SDL_Renderer* _rend = nullptr;
};
