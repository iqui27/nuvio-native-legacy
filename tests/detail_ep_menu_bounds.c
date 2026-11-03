// Menu summary (420px) must not use NV_DETP_EP_TEXTO_W (576) on Standard cards.
#include "nvtmp.h"
#include "dados.h"
#include "detail.h"
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static float cardW;
static int resumoDraw;
static float maxTextW, lastBarW;

static float hook_bloco(TxtEstilo estilo, const char *s, int r, int g, int b,
                        float x, float y, float w, float leading, float a, int lines) {
  if (resumoDraw && w > maxTextW + 0.5f) assert(!"synopsis width");
  return txt_bloco_corta(estilo, s, r, g, b, x, y, w, leading, a, lines);
}
static TxtLinha hook_corta(TxtEstilo estilo, const char *s, int r, int g, int b, int a, float w) {
  if (resumoDraw && w > maxTextW + 0.5f) assert(!"title width");
  return txt_linha_corta(estilo, s, r, g, b, a, w);
}
static void hook_cor(GfxRect rect, float radius, float rr, float rg, float rb, float a) {
  if (resumoDraw && rect.h == NV_DETP_EP_BARRA_H && rect.w > 0.0f) {
    if (rect.w > maxTextW + 0.5f) assert(!"progress width");
    lastBarW = rect.w;
  }
  gfx_cor(rect, radius, rr, rg, rb, a);
}

#define txt_bloco_corta hook_bloco
#define txt_linha_corta hook_corta
#define gfx_cor hook_cor
#include "../src/detail.c"
#undef txt_bloco_corta
#undef txt_linha_corta
#undef gfx_cor

int main(void) {
  CatItem c;
  CatEp eps[1];
  char dir[NVT_MAX], path[700];
  SDL_Window *win;

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("ep-menu-bounds", 0, 0, 1920, 1080,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(16);

  nvt_set(dir, sizeof dir, "/nuvio-ep-menu-bounds-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w");
  assert(f);
  fputs("homeLayoutLocal 1\n", f);
  fclose(f);
  ajustes_dir(dir);

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Bounds");
  snprintf(c.imdb, sizeof c.imdb, "tt0000999");
  c.temporadas[0] = 1;
  c.nTemporadas = 1;
  c.temporada = 1;
  c.episodio = 1;
  c.progresso = 40;
  cat_definir_tudo(&c, 1, NULL, 0);
  idx = 0;
  memset(eps, 0, sizeof eps);
  eps[0].temporada = 1;
  eps[0].episodio = 1;
  snprintf(eps[0].nome, sizeof eps[0].nome,
           "Título longo que não pode invadir a coluna de ações do menu");
  snprintf(eps[0].sinopse, sizeof eps[0].sinopse,
           "Sinopse longa repetida para forçar três linhas e medir a largura "
           "máxima desenhada dentro do resumo do menu ao lado das ações.");
  snprintf(eps[0].duracao, sizeof eps[0].duracao, "52 min");
  cat_definir_episodios(0, eps, 1);
  aberto = 1;

  cardW = (float)NV_DETP_EP_W;
  resumoDraw = 0;
  lastBarW = 0;
  desenhaEpisodioEm((GfxRect){100, 200, cardW, (float)NV_DETP_EP_H}, 0, 0, 1, 0, 0);

  cardW = 420.0f;
  maxTextW = cardW - 2.0f * NV_DETP_EP_PAD;
  resumoDraw = 1;
  lastBarW = 0;
  assert(ajustes_home_layout() == HOME_LAYOUT_PADRAO);
  assert(!epModerno() && !epApple());
  desenhaEpisodioEm((GfxRect){100, 200, cardW, 740}, 0, 0, 1, 0, 1);
  assert(lastBarW > 0.0f && lastBarW <= maxTextW + 0.5f);

  puts("PASS: menu summary text stays inside 420px column");
  return 0;
}
