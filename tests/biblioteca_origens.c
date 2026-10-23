// Offline source navigation regression; includes private state for bounds assertions.
#ifndef BIB_FONTE
#define BIB_FONTE "../src/biblioteca.c"
#endif
#include BIB_FONTE
#include <assert.h>
#include <limits.h>

static void key(SDL_Keycode code) {
  SDL_Event event = {0};
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = code;
  biblioteca_evento(&event);
}

int main(void) {
  CatItem items[2] = {0};
  for (int i = 0; i < 2; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt900%02d", i);
    strcpy(items[i].tipo, "movie");
    snprintf(items[i].origem, sizeof items[i].origem, "addon.%d", i);
    items[i].naLista = 1;
  }
  cat_definir(items, 2);
  biblioteca_iniciar();
  key(SDLK_DOWN); key(SDLK_DOWN);
  for (int i = 0; i < BIB_N_ORIG + 1; i++) key(SDLK_DOWN);
  key(SDLK_RETURN);
  assert(origem == BIB_N_ORIG + 1);
  for (int i = 0; i < BIB_N_ORIG + 3; i++) key(SDLK_UP);
  for (int i = 0; i < MODO_LISTAS; i++) key(SDLK_RIGHT);
  key(SDLK_RETURN);
  assert(modo == MODO_LISTAS && fonte == FONTE_TRAKT);
  key(SDLK_DOWN); key(SDLK_DOWN); key(SDLK_RETURN);
  assert(fonte == FONTE_TRAKT);
  const char *rot, *val;
  pickerTexto(0, &rot, &val);
  assert(!strcmp(val, "Trakt"));
  // Reject stale pointer/state values before they become a list source.
  const int invalid[] = {-1, BIB_N_FONTES, INT_MAX, INT_MIN};
  for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
    origemSel = invalid[i];
    key(SDLK_RETURN);
    assert(fonte == FONTE_TRAKT);
    fonte = invalid[i];
    pickerTexto(0, &rot, &val);
    assert(!strcmp(val, "Trakt"));
    fonte = FONTE_TRAKT;
    origem = invalid[i];
    modo = MODO_ASSISTIDOS;
    pickerTexto(0, &rot, &val);
    assert(val && *val);
    modo = MODO_LISTAS;
  }
  for (fonte = 0; fonte < BIB_N_FONTES; fonte++) {
    pickerTexto(0, &rot, &val);
    assert(!strcmp(val, ROT_FONTE[fonte]));
  }
  modo = MODO_ASSISTIDOS;
  for (origem = 0; origem < BIB_N_ORIG; origem++) {
    pickerTexto(0, &rot, &val);
    assert(!strcmp(val, ROT_ORIG[origem]));
  }
  origem = BIB_N_ORIG + 1;
  pickerTexto(0, &rot, &val);
  assert(!strcmp(val, "addon.1"));
  puts("biblioteca source navigation: PASS");
  return 0;
}
