// A TECLA DE VOLTAR E UMA SO (src/teclavoltar.h). Ver tests/teclavoltar.sh.
#include "teclavoltar.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-52s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}
static SDL_Event tecla(SDL_Keycode k, int sc) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.keysym.scancode = (SDL_Scancode)sc;
  return e;
}
int main(void) {
  SDL_Event e;
  e = tecla(SDLK_ESCAPE, 0);            confere("Escape volta", nv_tecla_voltar(&e));
  e = tecla(SDLK_AC_BACK, 0);           confere("AC_BACK (Android/Tizen) volta", nv_tecla_voltar(&e));
  e = tecla(SDLK_BACKSPACE, 0);         confere("Backspace volta", nv_tecla_voltar(&e));
  e = tecla(SDLK_DELETE, 0);            confere("Delete volta", nv_tecla_voltar(&e));
  e = tecla(0, NV_SCANCODE_BACK);       confere("BACK da LG (scancode 482, sem sym) volta", nv_tecla_voltar(&e));
  e = tecla(SDLK_RETURN, 0);            confere("Enter NAO volta", !nv_tecla_voltar(&e));
  e = tecla(SDLK_LEFT, 0);              confere("Esquerda NAO volta", !nv_tecla_voltar(&e));
  e = tecla(0, NV_SCANCODE_BLUE);       confere("AZUL NAO volta", !nv_tecla_voltar(&e));
  e = tecla(SDLK_s, 0);                 confere("letra NAO volta", !nv_tecla_voltar(&e));
  confere("ponteiro nulo NAO volta", !nv_tecla_voltar(NULL));
  printf(falhas ? "teclavoltar: %d falha(s)\n" : "teclavoltar: ok\n", falhas);
  return falhas ? 1 : 0;
}
