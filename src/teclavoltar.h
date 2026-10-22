#ifndef NV_TECLAVOLTAR_H
#define NV_TECLAVOLTAR_H
// A TECLA DE VOLTAR, UMA SO PARA TODAS AS TELAS.
//
// Cada tela tinha o seu ehVoltar/teclaVoltar e eles divergiam: a central de
// controle nao aceitava o BACK da LG (NV_SCANCODE_BACK, que chega com sym 0),
// e Delete valia no Novidades e no aviso de DV mas nao no Registro. Aceita:
//   Escape (teclado, emulador), AC_BACK (Android/Tizen), Backspace, Delete e o
//   BACK da LG pelo scancode 482.
// Nenhuma tela convertida perdeu tecla; so ganharam as que faltavam.
// tests/teclavoltar.sh.
#include <SDL2/SDL.h>
#include "layout.h"   // NV_SCANCODE_BACK

static inline int nv_tecla_voltar(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e) return 0;
  k = e->key.keysym.sym;
  return k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}
#endif
