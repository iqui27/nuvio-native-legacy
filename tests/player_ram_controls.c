#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#define NV_SCANCODE_GREEN 487
static int shortcut,visible,writes,save_ok=1;
int ajustes_ram_verde(void) { return shortcut; }
int ajustes_ram_overlay_alternar(void) {
  writes++;if(!save_ok)return 0;visible=!visible;return 1;
}
#include "ram-controls.c"
int main(void) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=NV_SCANCODE_GREEN;
  assert(!bufferRamTecla(&e) && !visible && !writes);
  shortcut=1;assert(bufferRamTecla(&e) && visible && writes==1);
  assert(bufferRamTecla(&e) && !visible && writes==2);
  e.key.repeat=1;assert(!bufferRamTecla(&e) && writes==2);
  e.key.repeat=0;e.type=SDL_KEYUP;assert(!bufferRamTecla(&e));
  e.type=SDL_KEYDOWN;e.key.keysym.scancode=0;e.key.keysym.sym=SDLK_F2;
  assert(bufferRamTecla(&e) && visible);
  shortcut=0;assert(!bufferRamTecla(&e) && visible);
  shortcut=1;save_ok=0;assert(bufferRamTecla(&e) && visible);
  puts("PASS RAM controls: shortcut off by default, optional green/F2, no repeat, independent visibility, save failure");
}
