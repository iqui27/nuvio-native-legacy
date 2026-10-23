// The glass program is linked from the 3rd frame on, not in gfx_iniciar nor in
// the first frame: until then a glass panel is a flat surface (never empty), and
// after the link the program in use must be right.
#include "gfx.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>

static void pixel(int x, int y, unsigned char out[4]) {
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

static int programas(void) {
  int n = 0;
  for (GLuint id = 1; id < 512; id++) n += glIsProgram(id) == GL_TRUE;
  return n;
}

int main(void) {
  SDL_Window *window;
  SDL_GLContext context;
  unsigned char px[4];
  int failed = 0;
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Glass lazy link", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  context = SDL_GL_CreateContext(window);
  assert(context);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  int antes = programas();   // the glass program is not among them: linked by the first glass draw
  for (int pass = 0; pass < 3; pass++) {   // pass 0: frame 1, flat; 1: links the glass; 2: reuses it
    if (pass) for (int f = 0; f < 3; f++) gfx_novo_quadro();
    glClearColor(0.6f, 0.5f, 0.4f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    pixel(400, 1080 - 300, px);   // the backdrop, before the glass
    int fundo = px[0];
    gfx_vidro((GfxRect){ 100, 100, 600, 400 }, 0.1f, GFX_VIDRO_BARRA, 0.0f, 0.0f, 1.0f);
    pixel(400, 1080 - 300, px);   // inside the glass: scrim must darken the backdrop
    int vidro = px[0] < fundo - 40;
    int linkou = programas() == antes + (pass ? 1 : 0);
    gfx_cor((GfxRect){ 1000, 100, 600, 400 }, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    pixel(1300, 1080 - 300, px);   // the solid red square drawn right after the glass
    int ok = vidro && linkou && px[0] > 250 && px[1] < 5 && px[2] < 5 && glGetError() == GL_NO_ERROR;
    printf("%s: pass %d, glass=%d programs=%d (start %d), after-glass pixel=%d,%d,%d\n", ok ? "PASS" : "FAIL",
           pass, vidro, programas(), antes, px[0], px[1], px[2]);
    if (!ok) failed = 1;
  }
  gfx_encerrar();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return failed;
}
