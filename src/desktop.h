#ifndef NV_DESKTOP_H
#define NV_DESKTOP_H
#ifdef NV_DESKTOP_RELEASE
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
static inline void desktop_start(void) {
  SDL_SetMainReady();
#ifdef _WIN32
  WSADATA ws;
  if (WSAStartup(MAKEWORD(2, 2), &ws)) exit(1);
#endif
  const char *proofData = getenv("NUVIO_DADOS");
  if (getenv("NV_CAPTURA") && (!proofData || !*proofData)) {
    fputs("[dados] capture requires writable isolated NUVIO_DADOS\n", stderr);
    exit(1);
  }
  if (!getenv("NUVIO_DADOS")) {
    char *p = SDL_GetPrefPath("", "Nuvio");
    if (!p) exit(1);
#ifdef _WIN32
    // SDL DLL and MinGW can use different CRT environment tables.
    if (_putenv_s("NUVIO_DADOS", p)) exit(1);
#else
    if (SDL_setenv("NUVIO_DADOS", p, 1)) exit(1);
#endif
    SDL_free(p);
  }
}
static inline int desktop_event(SDL_Event *e, SDL_Window *window, int typing) {
  if (e->type == SDL_CONTROLLERDEVICEADDED) {
    SDL_GameControllerOpen(e->cdevice.which); return 1;
  }
  if (e->type == SDL_CONTROLLERDEVICEREMOVED) {
    SDL_GameController *c = SDL_GameControllerFromInstanceID(e->cdevice.which);
    if (c) SDL_GameControllerClose(c);
    return 1;
  }
  if (e->type == SDL_CONTROLLERBUTTONDOWN || e->type == SDL_CONTROLLERBUTTONUP) {
    SDL_Keycode k;
    switch (e->cbutton.button) {
      case SDL_CONTROLLER_BUTTON_DPAD_UP: k = SDLK_UP; break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN: k = SDLK_DOWN; break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT: k = SDLK_LEFT; break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: k = SDLK_RIGHT; break;
      case SDL_CONTROLLER_BUTTON_A: k = SDLK_RETURN; break;
      case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: k = SDLK_AC_BACK; break;
      default: return 1;
    }
    int down = e->type == SDL_CONTROLLERBUTTONDOWN;
    memset(e, 0, sizeof *e);
    e->type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e->key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e->key.keysym.sym = k;
  }
  if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
    SDL_Keycode k = e->key.keysym.sym;
    int full = k == SDLK_F11 || (k == SDLK_f && (e->key.keysym.mod & KMOD_GUI) && (e->key.keysym.mod & KMOD_CTRL));
    if (full) {
      if (e->type == SDL_KEYDOWN && !e->key.repeat)
        SDL_SetWindowFullscreen(window, SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
      return 1;
    }
    if (k == SDLK_ESCAPE || (k == SDLK_BACKSPACE && !typing) ||
        (k == SDLK_LEFTBRACKET && (e->key.keysym.mod & KMOD_GUI) && !typing)) e->key.keysym.sym = SDLK_AC_BACK;
  }
  return 0;
}
#endif
#endif
