// Captura do cartao de novidades da 1.3.12 em estados PT, EN e reduzido.
// A revisao visual e parte do teste: a ilustraçao vetorial, o titulo em duas
// linhas e a legibilidade do accent precisam ser conferidos em 1920x1080.
#include "novidades1312.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ajustesDeTeste(int ingles, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n",
          ingles, tema, reduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    novidades1312_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.035f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    novidades1312_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32,
                                         SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch,
               pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-novidades1312";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: novidades 1.3.12", SDL_WINDOWPOS_CENTERED,
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
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  ajustesDeTeste(0, 0, 2);
  novidades1312_abrir();
  snprintf(nome, sizeof nome, "%s-pt.bmp", saida);
  captura(nome, w);

  ajustesDeTeste(1, 0, 6);
  novidades1312_abrir();
  snprintf(nome, sizeof nome, "%s-en.bmp", saida);
  captura(nome, w);

  ajustesDeTeste(0, 1, 5);
  novidades1312_abrir();
  snprintf(nome, sizeof nome, "%s-reduzido.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da 1.3.12 gravadas.");
  return 0;
}
