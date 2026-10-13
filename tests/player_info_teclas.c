// #386: directional input must not replay the top-left format entry.
// Real OSD and event handler; intercept only the format drawing boundary.
#include "badges.h"
static float alphaFormato;
static int formatos;
static float formatoTeste(FormatoMarca f, float x, float y, float h,
                          float r, float g, float b, float a) {
  (void)x; (void)y; (void)r; (void)g; (void)b;
  alphaFormato = a; formatos++;
  return marca_formato_largura(f, h);
}
#include "video.h"
static int larguraTeste(void) { return 1920; }
#define video_largura larguraTeste
#define marca_formato formatoTeste
#include "../src/player.c"
#undef marca_formato
#undef video_largura
#include <assert.h>
static void desenhar(Uint32 agora) {
  formatos = 0;
  desenharOsdCorpo(agora, 1, 1, NULL);
}
int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_Window *w = SDL_CreateWindow("info", 0, 0, 64, 64,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  gfx_opacidade_grupo = 0; // no pixels needed; inspect the OSD draw arguments
  aberto = visivel = 1; entrada = anim = cheio = fileira = 1;
  comVideo = 0; canalSessao = 0; duracaoSeg = 3600; posSeg = 100;
  barraFoco = 0; ultimoInput = 1000;
  desenhar(1000);
  for (Uint32 t = 1016; t <= 1500; t += 16) desenhar(t);
  assert(formatos && alphaFormato > .8f);
  for (int i = 0; i < 12; i++) {
    SDL_Event e = {0}; e.type = SDL_KEYDOWN;
    e.key.keysym.sym = i % 2 ? SDLK_LEFT : SDLK_RIGHT;
    player_evento(&e);
    // Same timestamp as input was the bug's animation reset trigger.
    ultimoInput = 1512 + i * 16;
    desenhar(ultimoInput);
    assert(formatos && alphaFormato > .8f);
  }
  // A new OSD appearance still gets its entry after being hidden.
  desenhar(2500); assert(!formatos);
  SDL_Quit();
  puts("PASS: directional keys preserve format opacity; new OSD animates");
  return 0;
}
