#pragma once
#include <SDL3/SDL.h>
namespace core_metal {
bool initImGuiPlatform(SDL_Window *window);
void processImGuiEvent(const SDL_Event &event);
}
