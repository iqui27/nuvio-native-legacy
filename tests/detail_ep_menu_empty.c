// Episode menu open, catalog refresh drops the episodes: the summary must not
// dereference a missing episode, and must come back once episodes return.
#include "nvtmp.h"
#include "dados.h"
#include "detail.h"
#include "episodios.h"
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NOME_EP "Episodio que volta"
static int viuNome;
static TxtLinha hook_corta(TxtEstilo estilo, const char *s, int r, int g, int b, int a, float w) {
  if (s && strstr(s, NOME_EP)) viuNome = 1;
  return txt_linha_corta(estilo, s, r, g, b, a, w);
}
#define txt_linha_corta hook_corta
#include "../src/detail.c"
#undef txt_linha_corta

static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    detail_desenhar(SDL_GetTicks());
  }
}

int main(void) {
  CatItem c;
  CatEp ep;
  char dir[NVT_MAX];
  SDL_Window *win;

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("ep-menu-empty", 0, 0, 1920, 1080,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  nvt_set(dir, sizeof dir, "/nuvio-ep-menu-empty-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  { char path[700]; FILE *f;
    snprintf(path, sizeof path, "%s/ajustes.txt", dir);
    assert((f = fopen(path, "w")));
    fputs("homeLayoutLocal 1\n", f);
    fclose(f); }
  ajustes_dir(dir);

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Vazio");
  snprintf(c.imdb, sizeof c.imdb, "tt0000998");
  c.temporadas[0] = 1; c.nTemporadas = 1; c.temporada = 1; c.episodio = 1;
  memset(&ep, 0, sizeof ep);
  ep.temporada = 1; ep.episodio = 1;
  snprintf(ep.nome, sizeof ep.nome, NOME_EP);
  cat_definir_tudo(&c, 1, NULL, 0);
  cat_definir_episodios(0, &ep, 1);
  idx = 0; aberto = 1; nivel = 1;
  foco.fileira = SEC_EPISODIOS; foco.coluna = 0;

  episodios_menu_visto(0, 1, 1, ep.nome);
  quadros(40);
  assert(episodios_menu_visivel() && episodios_menu_resumo(NULL));
  assert(viuNome);

  // Catalog refresh with the menu still open: same series, no episodes yet.
  cat_definir_tudo(&c, 1, NULL, 0);
  assert(cat_n_episodios(0) == 0 && epVisiveis() == 0);
  viuNome = 0;
  quadros(5);
  assert(episodios_menu_visivel() && !viuNome);

  // Episodes come back: the summary resumes on the same episode.
  cat_definir_episodios(0, &ep, 1);
  quadros(5);
  assert(viuNome);

  puts("PASS: open episode menu survives empty catalog and resumes");
  return 0;
}
