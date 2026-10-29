// Default subtitle outline (borda 1) must be a solid opaque ring, not a
// translucent drop shadow. White "I" on a bright frame, black border:
//   bash tests/legenda_contorno_shot.sh <dir> [antigo]
// Asserts the four sides of the stem sit on pure black. "antigo" redraws the
// old 3-stamp alpha .82 version (must fail the same check).
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LW 1920
#define LH 1080
static GLuint fbo, fboTex;
static unsigned char quadro[LW * LH * 4];

static void ler(void) {
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, quadro);
}
static int px(int x, int y) { return quadro[((size_t)(LH - 1 - y) * LW + x) * 4]; }

static void salvar(const char *dir, const char *id, int x0, int y0, int w, int h) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, w, h, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700]; int x, y;
  for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
    unsigned char *d = (unsigned char *)s->pixels + y * s->pitch + x * 3;
    const unsigned char *o = quadro + ((size_t)(LH - 1 - (y0 + y)) * LW + x0 + x) * 4;
    d[0] = o[0]; d[1] = o[1]; d[2] = o[2];
  }
  snprintf(nome, sizeof nome, "%s/%s.bmp", dir, id);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  fprintf(stderr, "captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/tmp";
  int antigo = argc > 2 && !strcmp(argv[2], "antigo");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("legenda contorno", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));

  txt_novo_quadro();
  TxtEstilo est = TXT_LEG_90;
  TxtLinha cor = txt_linha_corta_enfase(est, "I", 255, 255, 255, 255, 1e9f, TXT_FAMILIA_MONTSERRAT, TXT_ENF_NEGRITO);
  TxtLinha bor = txt_linha_corta_enfase(est, "I", 0, 0, 0, 255, 1e9f, TXT_FAMILIA_MONTSERRAT, TXT_ENF_NEGRITO);
  assert(cor.tex && bor.tex);
  float X = 900, Y = 500;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  // Stem bounding box: the white line alone on black (>= 128).
  int x, y, x0 = LW, x1 = -1, y0 = LH, y1 = -1;
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  txt_desenhar_alpha(cor, X, Y, 1.0f);
  ler();
  for (y = (int)Y - 10; y < (int)Y + cor.h + 10; y++) for (x = (int)X - 10; x < (int)X + cor.w + 10; x++)
    if (px(x, y) >= 128) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
  assert(x1 > x0 && y1 > y0);
  glClearColor(.82f, .82f, .82f, 1); glClear(GL_COLOR_BUFFER_BIT);   // bright frame
  if (antigo) {
    txt_desenhar_alpha(bor, X + 2, Y + 2, .82f);
    txt_desenhar_alpha(bor, X - 2, Y, .82f);
    txt_desenhar_alpha(bor, X, Y - 2, .82f);
  } else txt_desenhar_borda(bor, X, Y, 1, 1.0f);
  txt_desenhar_alpha(cor, X, Y, 1.0f);
  ler();
  salvar(dir, antigo ? "contorno-antes" : "contorno-depois", (int)X - 30, (int)Y - 20, 100, cor.h + 40);

  int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
  int lado[4] = { px(x0 - 1, ym), px(x1 + 1, ym), px(xm, y0 - 1), px(xm, y1 + 1) };
  fprintf(stderr, "bbox %d..%d x %d..%d; esq %d dir %d cima %d baixo %d (0 = preto opaco)\n",
          x0, x1, y0, y1, lado[0], lado[1], lado[2], lado[3]);
  for (x = 0; x < 4; x++) if (lado[x] > 15) { printf("legenda_contorno_shot: FAIL (lado %d = %d)\n", x, lado[x]); return 1; }

  // OPACIDADE SELECIONADA: o contorno compoe o alfa UMA vez, nao por estampa.
  // A 25% sobre o quadro a 82% o lado do anel deve ler ~0.82*0.75 (~157);
  // empilhando as 8 estampas translucidas ele ia a 1-0.75^3=58% (~89 ou menos).
  glClearColor(.82f, .82f, .82f, 1); glClear(GL_COLOR_BUFFER_BIT);
  if (antigo) {
    txt_desenhar_alpha(bor, X + 2, Y + 2, .82f * .25f);
    txt_desenhar_alpha(bor, X - 2, Y, .82f * .25f);
    txt_desenhar_alpha(bor, X, Y - 2, .82f * .25f);
  } else txt_desenhar_borda(bor, X, Y, 1, .25f);
  txt_desenhar_alpha(cor, X, Y, .25f);
  ler();
  salvar(dir, antigo ? "contorno-opaco-antes" : "contorno-opaco-depois", (int)X - 30, (int)Y - 20, 100, cor.h + 40);
  { int l2[4] = { px(x0 - 1, ym), px(x1 + 1, ym), px(xm, y0 - 1), px(xm, y1 + 1) };
    fprintf(stderr, "alfa .25: esq %d dir %d cima %d baixo %d (~157 = uma passada de 25%%)\n",
            l2[0], l2[1], l2[2], l2[3]);
    for (x = 0; x < 4; x++) if (l2[x] < 120 || l2[x] > 200) {
      printf("legenda_contorno_shot: FAIL (alfa composto: lado %d = %d)\n", x, l2[x]); return 1; } }
  printf("legenda_contorno_shot: ok\n");
  return 0;
}
