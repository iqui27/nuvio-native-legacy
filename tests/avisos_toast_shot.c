// Captura isolada do toast da Central de avisos, sem rede ou estado real.
#include "avisos.h"
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

// A inclusao deixa o teste acionar o mesmo evento interno que chega de agenda,
// atualizacao ou recomendacao, sem criar API publica so para fotografia.
#include "../src/avisos.c"

static void captura(const char *nome, SDL_Window *win) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
           1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(win);
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-avisos-toast.bmp";
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700];
  SDL_Window *w;
  SDL_GLContext gl;
  FILE *f;
  int i;
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n");
  fclose(f);
  ajustes_dir(dir);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: toast de avisos", SDL_WINDOWPOS_CENTERED,
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

  toast(4);
  for (i = 0; i < 50; i++) {
    Uint32 agora = SDL_GetTicks();
    avisos_atualizar(1.0f / 60.0f, agora);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    avisos_desenhar(agora);
    if (i == 49) captura(saida, w);
    else SDL_GL_SwapWindow(w);
  }
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: captura do toast gravada.");
  return 0;
}
