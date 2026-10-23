// RECUSA NO PEDIDO NAO PODE SEGURAR O MUTEX (C9/TCL, 14/10/2026).
//
// Relato do dono na integracao 2e4d2434: "os poster e artwork na c9 nao estao
// carregando", e na TCL o pouco que carrega e lento; "os icones tb nao estao
// carregando de alguns componentes como do control centre". A recusa do item 4
// (plx-recusa) voltava de tex_obter_limite DEPOIS de BUSCA_MEDIDA, que toma o
// mutex do cache, sem solta-lo. O mutex do SDL e recursivo: o fio principal
// segue entrando, mas os fios de rede e decode esperam para sempre. Na Home um
// cartao da faixa de cull fica TOTALMENTE fora da tela (nivel 2) em todo
// quadro, entao a primeira recusa ja congela o cache inteiro — cartaz,
// fundo, logo e os icones Lucide da Central (gfx_icone passa pelo mesmo cache).
//
// Aqui: fios de verdade, contexto GL, um quadro "de Home" por iteracao com o
// cartao da borda recusado, o cartaz visivel e um icone da Central. Mede o
// tempo ate os dois estarem publicados. NV_SEM_RECUSA=1 tira o cartao fora
// da tela (o quadro de antes do plx-recusa) para comparar o tempo.
//   argv[1]: PNG do ultimo quadro (opcional)
#include "gfx.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARTAZ "deploy/app/art/poster/00.jpg"
#define FORA   "deploy/app/art/poster/01.jpg"
#define ICONE  "deploy/app/art/icones/aj_wifi.png"
#define PRAZO_MS 4000

int main(int argc, char **argv) {
  const char *png = argc > 1 && argv[1][0] ? argv[1] : NULL;
  const char *sem = getenv("NV_SEM_RECUSA");
  int semRecusa = sem && *sem == '1', pronto = 0;
  SDL_Window *w;
  Uint32 t0, ms = 0;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("plx_recusa_trava", 0, 0, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(tex_iniciar(192) == 1);
  tex_escala(1.0f);
  gfx_icones_dir("deploy/app/art");

  t0 = SDL_GetTicks();
  while (!pronto && (ms = SDL_GetTicks() - t0) < PRAZO_MS) {
    SDL_PumpEvents();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Fileira parada, cold start: o ultimo cartao da faixa de cull esta
    // inteiro alem da borda direita (nivel 2) e e recusado em todo quadro.
    tex_grade_movendo(0);
    if (!semRecusa) {
      tex_pedido_fora(2);
      (void)tex_obter_larg(FORA, 300.0f);
      tex_pedido_fora(0);
    }
    GLuint t = tex_obter_larg(CARTAZ, 300.0f);
    if (t) gfx_rect((GfxRect){ 120, 200, 300, 450 }, t, GFX_CARD, 0, 0, 0, 0.05f, 1, 1, 1, 1);
    // Icone da Central de controle (central.c: gfx_icone 32x32).
    gfx_icone((GfxRect){ 480, 200, 32, 32 }, "aj_wifi", 1, 1, 1, 1);
    pronto = t && tex_aspecto(ICONE) > 0.0f;
    tex_bombear(3);
    if (pronto && png) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(pix && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, png) == 0);
      SDL_FreeSurface(s); free(pix);
      printf("captura: %s\n", png);
    }
    SDL_GL_SwapWindow(w);
    SDL_Delay(16);
  }
  printf("plx_recusa_trava (%s): cartaz+icone %s em %u ms, recusas=%d\n",
         semRecusa ? "sem recusa" : "com recusa", pronto ? "prontos" : "AUSENTES",
         (unsigned)ms, tex_n_recusa);
  if (!pronto) { puts("plx_recusa_trava: FAIL"); return 1; }
  puts("plx_recusa_trava: PASS");
  return 0;
}
