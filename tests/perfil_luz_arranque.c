// #657: Light must retain its clean gradient before and after a relaunch.
// Real GL rendering; the two processes share only their disposable data directory.
#include "psestilos.h"
#include "gfx.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1920
#define H 1080

static SDL_Surface *capture(const char *path, const PSCena *scene) {
  unsigned char *pixels = malloc(W * H * 4);
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && surface);
  gfx_novo_quadro();
  psestilos_desenhar(scene, PS_FUNDO_LUZ, 1.0f);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  assert(glGetError() == GL_NO_ERROR);
  for (int y = 0; y < H; y++)
    memcpy((char *)surface->pixels + y * surface->pitch, pixels + (H - 1 - y) * W * 4, W * 4);
  assert(IMG_SavePNG(surface, path) == 0);
  free(pixels);
  return surface;
}

int main(int argc, char **argv) {
  assert(argc == 4);
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Light launch regression", 0, 0, W, H,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar());
  dados_iniciar(NULL);
  assert(getenv("NUVIO_TESTE_DIR") && !strcmp(dados_dir(), getenv("NUVIO_TESTE_DIR")));
  PSCena scene = { .luz = { .12f, .53f, .90f, .90f, .23f, .21f },
                   .xFoco = 960, .perfil = 1, .reduzida = 1 };
  psestilos_iniciar();
  // Reference: the same gradient before the separate film texture exists.
  // Both launches follow the production update-before-draw order below.
  SDL_Surface *clean = capture(argv[1], &scene);
  // A retained Projector texture must not leak grain back into Light.
  psestilos_atualizar(1.0f / 60.0f, &scene, PS_FUNDO_PROJETOR);
  psestilos_iniciar();
  char *previous = dados_ler("light-first.bmp");
  psestilos_atualizar(1.0f / 60.0f, &scene, PS_FUNDO_LUZ);
  SDL_Surface *rendered = capture(argv[2], &scene);
  int relaunchEqual = 1;
  if (previous) {
    SDL_RWops *file = SDL_RWFromFile(previous, "rb");
    assert(file);
    SDL_Surface *loaded = SDL_LoadBMP_RW(file, 1);
    assert(loaded);
    SDL_Surface *first = SDL_ConvertSurfaceFormat(loaded, rendered->format->format, 0);
    assert(first);
    relaunchEqual = !memcmp(first->pixels, rendered->pixels, H * rendered->pitch);
    SDL_FreeSurface(first);
    SDL_FreeSurface(loaded);
  } else {
    assert(SDL_SaveBMP(rendered, argv[3]) == 0);
    assert(dados_gravar("light-first.bmp", argv[3]));
  }
  int cleanEqual = !memcmp(clean->pixels, rendered->pixels, H * rendered->pitch);
  printf("Light %s: clean gradient=%s; second run equals first=%s\n",
         previous ? "second run" : "first run", cleanEqual ? "PASS" : "FAIL",
         relaunchEqual ? "PASS" : "FAIL");
  free(previous);
  SDL_FreeSurface(clean);
  SDL_FreeSurface(rendered);
  return cleanEqual && relaunchEqual ? 0 : 1;
}
