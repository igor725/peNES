#pragma once

#include <cstdint>

struct Console;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_AudioStream;
union SDL_Event;

class GUI {
  public:
  enum class SaveAction { Save, Load };

  void init(SDL_Window* wnd, SDL_Renderer* rend, SDL_AudioStream* strm);
  void savestateAction(Console& nes, SaveAction sa, int8_t slot);
  void forwardEvent(SDL_Event const*);
  bool produceFrame(Console& nes);
  void triggerCPUHaltSequence();
  void updateTheme();
  void deinit();

  private:
  SDL_Renderer*    _rend    = nullptr;
  SDL_AudioStream* _astrm   = nullptr;
  bool             _cpuHalt = false;
};
