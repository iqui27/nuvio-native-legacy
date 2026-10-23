// The deferred Dynamic background (Android) must stay deferred across the
// split-path ring and corner light: they used to force it out mid-frame.
#include "../src/gfx.c"
#include <SDL2/SDL.h>
#include <assert.h>

static int caso(const char *nome, int anel) {
  const float topo[3] = { 0.1f, 0.1f, 0.2f };
  GfxRect r = { 200, 200, 400, 600 };
  dinPendente = 0; dinAdiado = 0; gfx_n_rect = 0;
  dinAdiadoOk = 1;
  gfx_fundo_din_desenhar(topo, 0.5f);
  assert(dinAdiado && dinPendente);
  if (anel) gfx_anel(r, 0.05f, 4.0f, 1, 1, 1, 1);
  else gfx_luz_canto(r, 0.05f, 0.0f, 0.0f, 150.0f, 1, 1, 1, 0.5f);
  printf("%s: %s\n", dinAdiado && dinPendente ? "PASS" : "FAIL", nome);
  return !(dinAdiado && dinPendente);
}

int main(void) {
  SDL_Window *w;
  int falhou = 0;
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("din adiado", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  falhou |= caso("ring split path keeps background deferred", 1);
  falhou |= caso("corner light split path keeps background deferred", 0);
  gfx_encerrar();
  SDL_Quit();
  return falhou;
}
