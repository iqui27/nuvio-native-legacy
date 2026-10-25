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
    items[i].listaFontes = 1u << AJ_SALVOS_LOCAL;
  }
  cat_definir(items, 2);
  biblioteca_iniciar();
  // Header and source column retain the mock's physical geometry below 100%.
  const float scales[] = {.85f, 1.0f, 1.3f};
  for (unsigned i = 0; i < sizeof scales / sizeof scales[0]; i++) {
    gfx_escala_txt_definir(scales[i]);
    TXT_INI();
    assert(fabsf(bibHS() - fmaxf(1.0f, scales[i])) < .001f);
    assert(fabsf(BIB_TOPO * bibHS() - 124.0f) < .001f);
    assert(fabsf(BIB_FAIXA_Y * bibHS() - 232.0f) < .001f);
    assert(fabsf(hdrX() * bibHS() - paginaX() * scales[i]) < .001f);
    assert(fabsf((bibX() - paginaX()) * scales[i] - 364.0f) < .001f);
    TXT_FIM();
  }
  gfx_escala_txt_definir(1.0f);
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
  listaTab = 1;   // 6D "Minhas listas" has no Source chip; Seguindo keeps it
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
