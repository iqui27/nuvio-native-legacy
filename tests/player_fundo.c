// Fundo do player no Android TV: (1) as barras do filme sao PRETO (0,0,0) e o
// furo segue transparente; (2) com video vivo por baixo o vidro fosco nao pinta
// o assado de 320x180 por cima dele. Captura opcional: argv[1] = pasta.
//   bash tests/player_fundo.sh [pasta-de-capturas]
#include "gfx.h"
#include "corviva.h"
#include "ajustes.h"
#include "player.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;

static void px(int x, int y, unsigned char o[4]) {
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(x, LH - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, o);
}
static void salvar(const char *dir, const char *nome) {
  SDL_Surface *s; unsigned char *p, *t; int y; char cam[700];
  if (!dir) return;
  s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t); snprintf(cam, sizeof cam, "%s/%s", dir, nome);
  assert(SDL_SaveBMP(s, cam) == 0); SDL_FreeSurface(s); printf("captura: %s\n", cam);
}
static void quadro(void) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro();
  glDisable(GL_SCISSOR_TEST);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : NULL;
  unsigned char a[4], b[4];
  CatItem it; GfxRect furo;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("fundo", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  { char c[700]; FILE *f; snprintf(c, sizeof c, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(c, "w"); assert(f); fprintf(f, "idioma 0\n"); fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar(); ajustes_definir_vidro(1);
  memset(&it, 0, sizeof it);

  /* (1) BARRAS PRETAS: 2.39:1 em 16:9, furo no miolo. A superficie e a que o
   * Android compoe sobre o video: fora do furo (0,0,0,1), dentro alfa 0. */
  quadro();
  glClearColor(0.3f, 0.3f, 0.3f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);   // lixo de antes: tem de sumir
  furo = (GfxRect){ 1.0f, 139.0f, 1918.0f, 802.0f };
  player_fundo_fora_do_furo(furo, 0, &it);
  gfx_furo(furo);
  px(960, 30, a); px(960, 1050, b);
  assert(a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 255);   // barra de cima
  assert(b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 255);   // barra de baixo
  px(0, 540, a); px(1919, 540, b);                              // a borda de 1 px do furo
  assert(a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 255);
  assert(b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 255);
  px(960, 540, a);
  assert(a[3] == 0);                                            // o furo continua furo
  { int x, y, n = 0;   // varredura: fora do furo, TODO pixel e preto opaco
    for (y = 0; y < LH; y += 37) for (x = 0; x < LW; x += 41) {
      int fora = y < 139 || y >= 941 || x < 1 || x >= 1919;
      px(x, y, a);
      if (fora) { assert(a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 255); n++; }
    }
    printf("barras: %d pontos fora do furo, todos (0,0,0,255)\n", n); }
  salvar(dir, "barras.bmp");
  { /* tela cheia sem barra: nada a pintar (o furo cobre tudo) */
    quadro(); glClearColor(0.3f, 0.3f, 0.3f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    player_fundo_fora_do_furo((GfxRect){ 0, 0, 1920, 1080 }, 0, &it);
    px(100, 100, a); assert(a[0] >= 75 && a[0] <= 78 && a[3] == 255); }

  /* (2) VIDRO FOSCO SO SEM VIDEO VIVO. Painel de vidro sobre o "video" (um
   * verde chapado). O vidro le a propria tela (gfx_vidro): livre ele captura
   * o que esta atras; com o bloqueio (o player abriu o furo, a tela ali nao e
   * o video) ele nao captura nada e sai chapado. Nos dois o verde transparece:
   * a luz ambiente (vermelho/azul/amarelo) nao entra mais no vidro. */
  { float cor[4][3] = { { .85f, .2f, .2f }, { .2f, .8f, .3f }, { .2f, .3f, .95f }, { .95f, .9f, .2f } };
    GfxRect painel = { 1440, 48, 432, 984 };
    int k;
    setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
    assert(ajustes_vidro_fosco());
    memcpy(nv_ambiente_viva, cor, sizeof cor); nv_ambiente_forca = 1.0f; nv_tempo_viva = 0.0f;
    for (k = 0; k < 2; k++) {
      quadro();
      gfx_ambiente_preparar();                       // o assado do quadro (main.c)
      glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
      glClearColor(0.f, 0.f, 0.f, 0.f); glClear(GL_COLOR_BUFFER_BIT);
      gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0.1f, 0.5f, 0.1f, 1.0f);   // "video"
      if (k == 1) gfx_vidro_fosco_bloquear();        // o player abriu o furo
      gfx_vidro_folha(painel, 36.0f / 984.0f, 1.0f);
      px(1500, 80, a); px(1850, 1000, b);
      printf("fosco %s: topo-esq %d,%d,%d  base-dir %d,%d,%d\n", k ? "bloqueado" : "livre",
             a[0], a[1], a[2], b[0], b[1], b[2]);
      assert(a[1] > a[0] && a[1] > a[2]);
      assert(b[1] > b[0] && b[1] > b[2]);
      assert(k ? gfx_vidro_capturas == 0 : gfx_vidro_capturas == 1);
      salvar(dir, k ? "folha-fosco-bloqueado.bmp" : "folha-fosco-livre.bmp");
    }
  }
  /* o bloqueio vale por quadro: o seguinte volta ao normal */
  quadro(); gfx_ambiente_preparar();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0.1f, 0.5f, 0.1f, 1.0f);
  gfx_vidro_folha((GfxRect){ 1440, 48, 432, 984 }, 36.0f / 984.0f, 1.0f);
  assert(gfx_vidro_capturas == 1);                                // o bloqueio era so daquele quadro
  px(1500, 80, a); assert(a[1] > a[0] && a[1] > a[2]);           // o video atras, de novo
  puts("player_fundo: ok");
  return 0;
}
