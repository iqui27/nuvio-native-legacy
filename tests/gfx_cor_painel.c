// Pixel parity between the panel clear fast path and the regular shader.
#include "gfx.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  const struct {
    const char *name;
    GfxRect rect;
    float alpha, scale;
  } cases[] = {
    { "fractional", { 100.5f, 100.5f, 1400, 800 }, 1, 1 },
    { "integer", { 100, 100, 1400, 800 }, 1, 1 },
    { "translucent", { 100, 100, 1400, 800 }, 0.9995f, 1 },
    { "scaled fractional", { 100, 100, 1400, 800 }, 1, 1.125f },
  };
  const size_t bytes = 1920 * 1080 * 4;
  unsigned char *reference = malloc(bytes), *panel = malloc(bytes);
  SDL_Window *window;
  SDL_GLContext context;
  int failed = 0;
  assert(reference && panel);
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Panel pixel regression", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  context = SDL_GL_CreateContext(window);
  assert(context);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++) {
    size_t different = 0;
    int maximum = 0;
    gfx_escala_sair(cases[c].scale);
    for (int pass = 0; pass < 2; pass++) {
      glClearColor(0.6f, 0.5f, 0.4f, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      if (pass)
        gfx_cor_painel(cases[c].rect, 40.0f / 800, 0.082f, 0.086f, 0.102f, cases[c].alpha);
      else
        gfx_cor(cases[c].rect, 40.0f / 800, 0.082f, 0.086f, 0.102f, cases[c].alpha);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pass ? panel : reference);
      assert(glGetError() == GL_NO_ERROR);
    }
    for (size_t i = 0; i < bytes; i++) {
      int delta = abs((int)reference[i] - (int)panel[i]);
      if (delta) different++;
      if (delta > maximum) maximum = delta;
    }
    printf("%s: %s, different=%zu max=%d/255\n",
           different ? "FAIL" : "PASS", cases[c].name, different, maximum);
    if (different) failed = 1;
  }
  gfx_escala_sair(1);
  gfx_encerrar();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  free(reference);
  free(panel);
  return failed;
}
