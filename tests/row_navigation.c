// Offline regression for row navigation while the previous row is clipped.
#include "../src/home.c"
#include <assert.h>

void row_navigation_library(void);
void row_navigation_detail(void);

int main(void) {
  SDL_Init(SDL_INIT_TIMER);
  CatItem items[4] = {0};
  for (int i = 0; i < 4; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%07d", i + 1);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
  }
  cat_definir(items, 4);
  AddonRemoto addon = {0};
  addon.ativo = 1;
  snprintf(addon.url, sizeof addon.url, "https://fixture.invalid");
  addons_definir_lista(&addon, 1);
  int cols[6] = {4,4,4,4,4,4};
  nFileiras = 6;
  focus_iniciar(&foco, nFileiras, cols);
  for (int r = 0; r < nFileiras; r++) {
    fileiras[r].tipo = FILEIRA_NORMAL;
    fileiras[r].n = 4;
    foco.colunaLembrada[r] = 0;
  }
  focoHero = 0;
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;
  // The hero remains visible while preceding rows leave the viewport.
  // Rebuild targets after each key, as rendering does, at both scroll speeds.
  for (int moving = 0; moving < 2; moving++) {
    foco.fileira = foco.coluna = 0;
    focoHero = 0;
    for (int r = 0; r < nFileiras; r++) foco.colunaLembrada[r] = moving ? r % 4 : 0;
    for (int step = 0; step < 11; step++) {
      navigationCount = 0;
      navigationTarget(100, 50, 400, 100, ponteiroHero, NULL, 0, 0);
      navigationTarget(100, moving ? 550 : 250, 150, 200,
                       ponteiroCard, NULL, foco.fileira, foco.coluna);
      e.key.keysym.sym = step < 5 || step == 10 ? SDLK_DOWN : SDLK_UP;
      home_evento(&e);
      int row = step < 5 ? step + 1 : step < 10 ? 9 - step : 1;
      int col = moving ? row % 4 : 0;
      if (focoHero || foco.fileira != row || foco.coluna != col) {
        fprintf(stderr, "step %d: hero=%d row=%d col=%d; expected row=%d col=%d\n",
                step, focoHero, foco.fileira, foco.coluna, row, col);
        return 1;
      }
    }
  }
  row_navigation_library();
  row_navigation_detail();
  puts("row navigation: Down x5, Up x1, Up x4, Down x1; row memory PASS");
  SDL_Quit();
  return 0;
}
