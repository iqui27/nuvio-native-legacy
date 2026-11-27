// Subtitle Outline (borda 1) = Stremio's look (owner 14/11/2026, ring
// 16/11/2026): 8 CSS shadows `±0.15rem ±0.15rem 0.15rem rgb(34,34,34)`, a
// dark ring on every side; and the subtitle fades as ONE picture
// (gfx_grupo_*), so at a low opacity the ring never shows through the letters.
//   bash tests/legenda_contorno_shot.sh <dir> [antigo]
// "antigo" fades border and text separately (the old way): the letter check
// must fail.
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
#ifdef __linux__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
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
  TxtLinha cor = txt_linha_corta_enfase(est, "I", 255, 255, 255, 255, 1e9f, TXT_FAMILIA_JAKARTA, TXT_ENF_NEGRITO);
  TxtLinha bor = txt_linha_corta_enfase(est, "I", 0x22, 0x22, 0x22, 255, 1e9f, TXT_FAMILIA_JAKARTA, TXT_ENF_NEGRITO | TXT_ENF_SUAVE);
  float m = txt_suave_margem(est);
  assert(cor.tex && bor.tex && m > 0.0f);
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
  int xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;

  // 1. Stremio's ring: dark on every side of the stem, and even (no
  //    down-right lean like the old single shadow).
  glClearColor(.82f, .82f, .82f, 1); glClear(GL_COLOR_BUFFER_BIT);   // bright frame
  txt_desenhar_borda(bor, X, Y, 1, m, 1.0f);
  txt_desenhar_alpha(cor, X, Y, 1.0f);
  ler();
  salvar(dir, "contorno-stremio", (int)X - 30, (int)Y - 20, 100, cor.h + 40);
  { int esq = px(x0 - 2, ym), dir2 = px(x1 + 2, ym), baixo = px(xm, y1 + 2), cima = px(xm, y0 - 2);
    fprintf(stderr, "bbox %d..%d x %d..%d; esq %d dir %d cima %d baixo %d (fundo 209)\n", x0, x1, y0, y1, esq, dir2, cima, baixo);
    if (!(esq < 110 && dir2 < 110 && cima < 110 && baixo < 110 && abs(esq - dir2) < 20 && abs(cima - baixo) < 20)) {
      printf("legenda_contorno_shot: FAIL (not Stremio's ring)\n"); return 1; } }

  // 1b. At 40 % the ring still darkens a bright frame next to the letter
  //     (the old shadow vanished there: the owner's 16/11 clip).
  glClearColor(.82f, .82f, .82f, 1); glClear(GL_COLOR_BUFFER_BIT);
  { static GfxGrupo g;
    float a = .40f, d = gfx_grupo_comecar(&g, (GfxRect){ X - m, Y - m, cor.w + 2 * m + 6, cor.h + 2 * m + 6 }, a) ? 1.0f : a;
    txt_desenhar_borda(bor, X, Y, 1, m, d);
    txt_desenhar_alpha(cor, X, Y, d);
    gfx_grupo_terminar(&g, a); }
  ler();
  salvar(dir, "contorno-40", (int)X - 30, (int)Y - 20, 100, cor.h + 40);
  { int esq = px(x0 - 2, ym), meio = px(xm, ym);
    fprintf(stderr, "alfa .40: borda esq %d, haste %d (fundo 209)\n", esq, meio);
    if (!(esq < 170 && meio > 215)) { printf("legenda_contorno_shot: FAIL (ring gone at 40 %%)\n"); return 1; } }

  // 2. Faded as one: at 25 % the stem's middle reads 25 % white over the
  //    frame (~221), the shadow under the letter doesn't darken it.
  glClearColor(.82f, .82f, .82f, 1); glClear(GL_COLOR_BUFFER_BIT);
  { static GfxGrupo g;
    float a = .25f, d = !antigo && gfx_grupo_comecar(&g, (GfxRect){ X - m, Y - m, cor.w + 2 * m + 6, cor.h + 2 * m + 6 }, a) ? 1.0f : a;
    txt_desenhar_borda(bor, X, Y, 1, m, d);
    txt_desenhar_alpha(cor, X, Y, d);
    if (!antigo) gfx_grupo_terminar(&g, a); }
  ler();
  salvar(dir, antigo ? "contorno-25-antes" : "contorno-25-depois", (int)X - 30, (int)Y - 20, 100, cor.h + 40);
  { int meio = px(xm, ym), esperado = (int)(0.25f * 255 + 0.75f * 0.82f * 255 + 0.5f);
    fprintf(stderr, "alfa .25: meio da haste %d (esperado ~%d)\n", meio, esperado);
    if (abs(meio - esperado) > 4) { printf("legenda_contorno_shot: FAIL (shadow through the letter: %d)\n", meio); return 1; } }
  printf("legenda_contorno_shot: ok\n");
  return 0;
}
