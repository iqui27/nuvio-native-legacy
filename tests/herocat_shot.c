// CAPTURA da folha "Catalogos do destaque" (#205): sugeridos, por addon, busca
// e estado vazio. Sem rede. Nao entra na suite (precisa de janela GL).
//   bash tests/herocat_shot.sh /tmp/nv-205/hc
#include "ajustes.h"
#include "nvtmp.h"
#include "fileiras.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ajustes_teste_herocat(const char *busca, int foco);

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-205/hc");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: hero catalog", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_iniciar();

  fil_registrar("a.trending.movie", "Trending", "Cinemeta", "movie", 20);
  fil_registrar("a.top.series", "Top", "Cinemeta", "series", 20);
  fil_registrar("b.latest.movie", "Latest releases", "Torrentio", "movie", 20);
  fil_registrar("b.upcoming.series", "Anticipated", "Torrentio", "series", 20);
  fil_registrar("c.drama.movie", "Drama", "Streaming Catalogs", "movie", 20);
  fil_registrar("c.comedia.series", "Comedia", "Streaming Catalogs", "series", 20);

  ajustes_teste_herocat("", 0);
  snprintf(nome, sizeof nome, "%s-1-sugeridos.png", saida);
  captura(nome, w);
  ajustes_teste_herocat("", 3);
  snprintf(nome, sizeof nome, "%s-2-foco.png", saida);
  captura(nome, w);
  ajustes_teste_herocat("top", 1);
  snprintf(nome, sizeof nome, "%s-3-busca.png", saida);
  captura(nome, w);
  ajustes_teste_herocat("zzzz", 0);
  snprintf(nome, sizeof nome, "%s-4-vazio.png", saida);
  captura(nome, w);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
