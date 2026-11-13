/* Test event boundary only; not an SDL SDK and never used by product builds. */
#ifndef AUDIOEXT_TEST_SDL_H
#define AUDIOEXT_TEST_SDL_H
#include "SDL_stdinc.h"
typedef Sint32 SDL_Keycode;
typedef enum { SDL_FALSE, SDL_TRUE } SDL_bool;
enum { SDL_KEYDOWN = 0x300, SDLK_RETURN = 13, SDLK_ESCAPE = 27, SDLK_BACKSPACE = 8,
  SDLK_RIGHT = 1073741903, SDLK_LEFT, SDLK_DOWN, SDLK_UP,
  SDLK_KP_ENTER = 1073741912, SDLK_AC_BACK = 1073742094 };
typedef union {
  Uint32 type;
  struct { Uint32 type; Uint8 repeat; struct { SDL_Keycode sym; int scancode; } keysym; } key;
} SDL_Event;
Uint32 SDL_GetTicks(void);
#endif
