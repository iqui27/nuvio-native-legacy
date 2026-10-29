// Run real Detail section drawing and the real episode menu in a hidden GL window.
#include "../src/detail.c"
#include "dados.h"
#include <assert.h>
#include <unistd.h>
int test_search_layout(float scale);

int main(void) {
  char dir[] = "/tmp/nv-text-layout-XXXXXX", path[256];
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Layout regression", 0, 0, 1920, 1080,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w"); assert(f);
  fputs("homeLayoutLocal 0\ncollapseSidebar 1\nmodernSidebar 0\n", f); fclose(f);
  ajustes_dir(dir);
  assert(ajustes_home_layout() == HOME_LAYOUT_MODERNA);
  CatItem items[5] = {0}; CatEp eps[3] = {0};
  for (int i = 0; i < 5; i++) {
    snprintf(items[i].titulo, sizeof items[i].titulo, "Fixture %d", i);
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt000020%d", i);
    strcpy(items[i].tipo, "series"); strcpy(items[i].poster, "fixture-poster");
    strcpy(items[i].backdrop, "fixture-backdrop");
    items[i].temporadas[0] = 1; items[i].nTemporadas = 1;
  }
  cat_definir_tudo(items, 5, NULL, 0); idx = 0;
  for (int i = 0; i < 3; i++) {
    eps[i].temporada = 1; eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Episode %d", i + 1);
  }
  cat_definir_episodios(0, eps, 3);
  int failures = 0;
  float scales[] = {.85f, 1.f, 1.3f};
  for (int s = 0; s < 3; s++) {
    float scale = scales[s]; gfx_escala_txt_definir(scale);
    failures += test_search_layout(scale);
    for (int ui = 0; ui < 2; ui++) {
      gfx_escala_ui_definir(ui ? 1.2f : 1.f);
      float old = gfx_escala_txt_entrar();
      assert(ajustes_rail_largura_fixa() > 0);
      foco.fileira = SEC_EPISODIOS; foco.coluna = 0;
      temporada = 0; scrollSec[SEC_EPISODIOS] = 0;
      scrollY = NV_DETP_EP_Y - 120.f / scale;
      assert(secaoN(SEC_EPISODIOS) == 3);
      desenhaSecao(SEC_EPISODIOS, 1, 1000);
      GfxRect displayed = menuOrigem;
      assert(displayed.w > 8 && fabsf(displayed.y * scale - 120) < .01f);
      episodios_menu_visto(0, 1, 1, "Episode 1");
      for (int frame = 0; frame < 180; frame++) episodios_menu_atualizar(1.f / 60);
      menuDeVisto(1000);
      GfxRect menu;
      assert(episodios_menu_caixa(&menu));
      float screenY = menu.y * gfx_escala_ui();
      int bad = fabsf(screenY - displayed.y * scale) > .05f;
      printf("%s episode anchor %.0f%% UI %.0f%%: card y=%.2f menu y=%.2f\n",
             bad ? "FAIL" : "PASS", scale * 100, gfx_escala_ui_base() * 100,
             displayed.y * scale, screenY);
      failures += bad;
      episodios_fechar(); gfx_escala_sair(old);
    }
  }
  unlink(path); rmdir(dir);
  return failures ? 1 : 0;
}
