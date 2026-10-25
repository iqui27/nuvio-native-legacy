#include <SDL2/SDL.h>
static Uint32 now = 1000;
static Uint32 guideTicks(void) { return now; }
#define SDL_GetTicks guideTicks
#include "../src/guia.c"
#undef SDL_GetTicks
#include <assert.h>

static void key(SDL_Keycode k, int repeat, Uint32 delay, Uint32 type) {
  SDL_Event e = {0};
  now += delay;
  e.type = type;
  e.key.keysym.sym = k;
  e.key.repeat = repeat;
  guia_evento(&e);
}
static void check(int channel) {
  assert(!catAberto && !focoTopo && !focoFontes);
  assert(focoLin * 6 + focoCol == channel);
}
int main(void) {
  int counts[] = {6, 6, 6}, visible[] = {0, 1, 2};
  catN = counts; vis = visible; nVis = 3;
  aberta = modoLista = 1;
  filtro = G_FILTRO_TODAS;
  for (int repeat = 0; repeat <= 1; repeat++) {
    focoLin = focoCol = focoTopo = focoFontes = catAberto = dirSeg = 0;
    // webOS sends repeat=0 every ~130 ms; SDL marks repeats explicitly.
    key(SDLK_DOWN, 0, 1000, SDL_KEYDOWN); check(1);
    for (int i = 2; i <= 30; i++) {
      key(SDLK_DOWN, repeat, 130, SDL_KEYDOWN); check(i < 18 ? i : 17);
    }
    key(SDLK_DOWN, 0, 50, SDL_KEYUP);
    key(SDLK_UP, 0, 50, SDL_KEYDOWN); check(16);
    for (int i = 15; i >= -12; i--) {
      key(SDLK_UP, repeat, 130, SDL_KEYDOWN); check(i > 0 ? i : 0);
    }
    // Even a delayed SDL repeat at the edge must not change focus.
    key(SDLK_UP, 1, 700, SDL_KEYDOWN); check(0);
    key(SDLK_UP, 0, 50, SDL_KEYUP);
    key(SDLK_UP, 0, 50, SDL_KEYDOWN); assert(focoTopo && !catAberto);
    // Categories remains available through a fresh header action.
    topoCol = G_TOPO_CATEGORIAS;
    key(SDLK_RETURN, 0, 50, SDL_KEYDOWN); assert(catAberto == 2);
    key(SDLK_LEFT, 0, 50, SDL_KEYDOWN); assert(!catAberto);
    focoTopo = 0;
    key(SDLK_LEFT, 0, 50, SDL_KEYDOWN); assert(focoTopo);
  }
  puts("PASS: held Down/Up scroll across categories, clamp at edges; fresh presses retain navigation");
  return 0;
}
