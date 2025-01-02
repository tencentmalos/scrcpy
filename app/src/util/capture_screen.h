#pragma once

#include <SDL2/SDL.h>


#ifdef __cplusplus
extern "C" {
#endif


int sc_save_screen_shot(const char* filename, SDL_Renderer* renderer);


#ifdef __cplusplus
}
#endif

