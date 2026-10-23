// CAPTURA DA HOME NOS QUATRO NIVEIS DE GPU (gpunivel.h), 29/09/2026.
//
// Pergunta do dono antes de levar os dois canarios a TV de 2019: "a cara nao
// quebrou? o texto a 720p ainda fica legivel?". Esta captura desenha a MESMA
// home (itens do Cinemeta, arte de verdade do metahub) pelo MESMO caminho do
// laco de main.c — gpun_quadro_inicio, clear, home_desenhar, gpun_quadro_fim
// — nos niveis 0, 1 e 2, e grava um BMP de cada. Tambem imprime o que a GPU
// recebe por quadro: preenchimento total e por modo (gfx_fill_modo, em telas
// 1920x1080) e, se o GL do Mac tiver GL_EXT_timer_query, o tempo de GPU medio
// de cada nivel. O M4 nao e a Mali da TV: o tempo absoluto nao vale nada, a
// PROPORCAO entre os niveis (as duas sao GPUs de ladrilhos) e uma pista.
//
// Nao entra na suite (precisa de janela GL, de rede e de olho humano):
//   bash tests/gpunivel_shot.sh /tmp/nuvio-gpunivel
#include "app.h"
#include "nvtmp.h"
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "descoberta.h"
#include "gfx.h"
#include "gpunivel.h"
#include "home.h"
#include "nuvem.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *tt, *nome; } TITULOS[] = {
  { "tt0111161", "The Shawshank Redemption" },
  { "tt0068646", "The Godfather" },
  { "tt0468569", "The Dark Knight" },
  { "tt1375666", "Inception" },
  { "tt0816692", "Interstellar" },
  { "tt0133093", "The Matrix" },
  { "tt0110912", "Pulp Fiction" },
  { "tt0109830", "Forrest Gump" },
  { "tt0120737", "The Lord of the Rings" },
  { "tt0167260", "The Return of the King" },
};
#define NT (int)(sizeof TITULOS / sizeof *TITULOS)

static const char *NOMES[GFX_NMODOS] = {
  "CARD", "SOMBRA", "COR", "HERO", "VEU", "TEXTO", "FUNDO", "VEU_TOPO", "SNAP", "PLAY",
  "BLUR", "DETALHE", "HERO_CHEIO", "ANEL", "OLHO", "FONTES", "MARCA", "VEU_BAIXO", "SOCIAL",
  "AVATAR", "RETRATO", "DISCO", "EDITORIAL", "VEU_CARD", "BRILHO_TOPO", "ARTE", "LUZ", "SINO",
  "ESQUELETO", "LINHA", "CEU", "COR_GRAD", "ANEL_GRAD", "AMBIENTE", "VITRINE", "FUNDO_DIN", "COPIA",
};

static GLuint consulta;
static int temTimer;

static void capturar(const char *bmp);

// Um quadro do laco de main.c, na mesma ordem. `bmp` = capturar a janela
// ANTES do swap (depois dele o buffer de tras e indefinido).
static double quadroCap(SDL_Window *w, int medir, const char *bmp);
static double quadro(SDL_Window *w, int medir) { return quadroCap(w, medir, NULL); }
static double quadroCap(SDL_Window *w, int medir, const char *bmp) {
  GLuint64 ns = 0;
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  gfx_sem_recorte();
  if (medir && temTimer) glBeginQuery(GL_TIME_ELAPSED_EXT, consulta);
  gpun_quadro_inicio();
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  home_atualizar(1.0f / 60.0f, SDL_GetTicks());
  home_desenhar(SDL_GetTicks());
  gpun_quadro_fim();
  if (medir && temTimer) {
    glEndQuery(GL_TIME_ELAPSED_EXT);
    glGetQueryObjectui64vEXT(consulta, GL_QUERY_RESULT, &ns);
  }
  if (bmp) capturar(bmp);
  SDL_GL_SwapWindow(w);
  SDL_Delay(16);
  return ns / 1e6;
}

static void capturar(const char *bmp) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, bmp) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", bmp);
}

int main(int argc, char **argv) {
  const char *dir = argv[1];
  const char *saida = argc > 2 ? argv[2] : nvt("/nuvio-gpunivel");
  static CatItem itens[NT];
  CatFileira fil[2];
  SDL_Window *w;
  SDL_GLContext gl;
  char cache[600], bmp[700];
  int i, n, k;

  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  // SEM ALLOW_HIGHDPI: drawable de 1920x1080 tambem no Mac retina, que e o
  // tamanho da janela da TV (e o do texto rasterizado la, escala 1).
  w = SDL_CreateWindow("Nuvio: niveis de GPU", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  { int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(w, &dw, &dh);
    printf("drawable=%dx%d\n", dw, dh);
    assert(dw == 1920 && dh == 1080); }
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  gpun_iniciar(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  artehero_definir_falhou(tex_falhou);
  nuvem_configurar("deploy/app/art");
  desc_tmdb("deploy/app/art");
  snprintf(cache, sizeof cache, "%s/cache", dir);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajustes_dir(dir);
  assert(home_iniciar("deploy/app/art"));
  temTimer = strstr((const char *)glGetString(GL_EXTENSIONS), "GL_EXT_timer_query") != NULL;
  if (temTimer) glGenQueries(1, &consulta);
  printf("GL_EXT_timer_query: %s\n", temTimer ? "sim" : "nao");

  memset(itens, 0, sizeof itens);
  for (i = 0; i < NT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "%s", TITULOS[i].tt);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "%s", TITULOS[i].nome);
    snprintf(c->genero, sizeof c->genero, "Filme");
    snprintf(c->backdrop, sizeof c->backdrop,
             "https://images.metahub.space/background/medium/%s/img", c->imdb);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
    snprintf(c->poster, sizeof c->poster,
             "https://images.metahub.space/poster/medium/%s/img", c->imdb);
    snprintf(c->logo, sizeof c->logo,
             "https://images.metahub.space/logo/medium/%s/img", c->imdb);
  }
  // Duas fileiras: as duas que cabem na parte de baixo da home 1080p.
  memset(fil, 0, sizeof fil);
  snprintf(fil[0].chave, sizeof fil[0].chave, "cinemeta_movie_top");
  snprintf(fil[0].titulo, sizeof fil[0].titulo, "Populares - Filme");
  snprintf(fil[0].tipo, sizeof fil[0].tipo, "movie");
  fil[0].ini = 0; fil[0].n = NT;
  snprintf(fil[1].chave, sizeof fil[1].chave, "cinemeta_movie_imdbRating");
  snprintf(fil[1].titulo, sizeof fil[1].titulo, "Mais bem avaliados - Filme");
  snprintf(fil[1].tipo, sizeof fil[1].tipo, "movie");
  fil[1].ini = 0; fil[1].n = NT;
  cat_definir_tudo(itens, NT, fil, 2);
  // Foco no segundo card da primeira fileira: o destaque mostra a arte dele,
  // o card em foco ganha anel, especular e halo — o quadro mais cheio da home.
  for (n = 0; n < 60; n++) quadro(w, 0);
  { static const SDL_Keycode TECLAS[] = { SDLK_DOWN, SDLK_RIGHT };
    for (k = 0; k < 2; k++) {
      SDL_Event e;
      memset(&e, 0, sizeof e);
      e.type = SDL_KEYDOWN; e.key.keysym.sym = TECLAS[k];
      home_evento(&e);
      e.type = SDL_KEYUP;
      home_evento(&e);
      for (n = 0; n < 20; n++) quadro(w, 0);
    } }
  // 12 s para as artes chegarem (metahub + decode).
  for (n = 0; n < 720; n++) quadro(w, 0);

  for (k = 0; k < 4; k++) {
    double soma = 0;
    int m;
    if (k == 3) gpun_forcar_720(); // resolution is independent of effects
    gpun_definir_nivel(k == 3 ? 2 : k);
    for (n = 0; n < 30; n++) quadro(w, 0);   // assenta (alvo interno criado)
    for (n = 0; n < 120; n++) soma += quadro(w, 1);
    printf("[shot] nivel=%d fill=%.2f telas, %d rects, %d cheias", k, gfx_fill, gfx_n_rect, gfx_n_cheio);
    if (temTimer) printf(" | GPU %.3f ms/quadro (media de 120)", soma / 120.0);
    printf("\n[shot]   por modo:");
    for (m = 0; m < GFX_NMODOS; m++)
      if (gfx_fill_modo[m] >= 0.005) printf(" %s=%.2f", NOMES[m], gfx_fill_modo[m]);
    printf("\n");
    // A captura le a janela no fim de um quadro inteiro (com a ampliacao).
    snprintf(bmp, sizeof bmp, "%s-nivel%d.bmp", saida, k);
    quadroCap(w, 0, bmp);
  }

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
