// MAC: PRE-BUSCA DO PROXIMO DO DESTAQUE E SINOPSE DOS CANDIDATOS (2.0.3).
//
// Catalogo de itens RASOS (so nome e arte do metahub, como a lista do Trakt) e
// o destaque parado: o log mostra a sinopse sendo preenchida pelo /meta real e
// a linha `[hero] pre-busca do proximo` 3 s antes de cada troca, com a busca do
// trailer ligada (trailerHero 1). Precisa de janela GL e de REDE; nao entra na
// suite.
//
//   bash tests/heroprebusca_shot.sh [segundos]
#include "app.h"
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "descoberta.h"
#include "gfx.h"
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *tt, *nome; } TITULOS[] = {
  { "tt0095953", "Rain Man" },
  { "tt0111161", "The Shawshank Redemption" },
  { "tt0468569", "The Dark Knight" },
  { "tt1375666", "Inception" },
  { "tt0133093", "The Matrix" },
  { "tt0110912", "Pulp Fiction" },
};
#define NT (int)(sizeof TITULOS / sizeof *TITULOS)

static void quadros(SDL_Window *w, int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    home_atualizar(1.0f / 60.0f, SDL_GetTicks());
    home_desenhar(SDL_GetTicks());
    if (0 && bmp) {
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
    SDL_GL_SwapWindow(w);
    SDL_Delay(16);
  }
}


int main(int argc, char **argv) {
  const char *dir = argv[1];
  int segundos = argc > 2 ? atoi(argv[2]) : 60;
  static CatItem itens[NT];
  CatFileira fil;
  SDL_Window *w;
  SDL_GLContext gl;
  char cache[600], cam[700];
  FILE *a;
  int i;

  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: pre-busca do destaque", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  artehero_definir_falhou(tex_falhou);
  nuvem_configurar("deploy/app/art");
  desc_tmdb("deploy/app/art");
  snprintf(cache, sizeof cache, "%s/cache", dir);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "trailerHero 1\n");
  fclose(a);
  ajustes_dir(dir);
  assert(home_iniciar("deploy/app/art"));

  memset(itens, 0, sizeof itens);
  for (i = 0; i < NT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "%s", TITULOS[i].tt);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "%s", TITULOS[i].nome);
    snprintf(c->genero, sizeof c->genero, "Filme");   // RASO: sem sinopse, sem meta
    snprintf(c->backdrop, sizeof c->backdrop,
             "https://images.metahub.space/background/medium/%s/img", c->imdb);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
    snprintf(c->poster, sizeof c->poster,
             "https://images.metahub.space/poster/medium/%s/img", c->imdb);
    snprintf(c->logo, sizeof c->logo,
             "https://images.metahub.space/logo/medium/%s/img", c->imdb);
  }
  memset(&fil, 0, sizeof fil);
  snprintf(fil.chave, sizeof fil.chave, "cinemeta_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Populares - Filme");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = NT;
  cat_definir_tudo(itens, NT, &fil, 1);
  quadros(w, segundos * 60, NULL);
  for (i = 0; i < NT; i++)
    printf("[shot] %s sinopse=%s\n", cat_item(i)->imdb,
           cat_item(i)->sinopse[0] ? "SIM" : "NAO");
  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
