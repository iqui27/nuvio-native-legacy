// Rampas do hero sobre o trailer em janela (#290). A home desenha o trailer
// ATRAS do canvas: furo no retangulo do hero e, desde o #290, as rampas do
// proprio hero (GFX_HERO com uPar.x = 1) por cima, com alfa. Aqui o plano de
// video e simulado: um quadro sintetico (1421x670, o aspecto do hero, entao
// cover = 1:1) e composto sob a UI como a TV faz com o framebuffer
// pre-multiplicado: final = ui.rgb + video * (1 - ui.a).
//
// Prova:
//   1. sem rampa (antes): a borda esquerda do retangulo e o video cru (borda dura);
//   2. com rampa (depois): a borda funde no fundo e o MIOLO do video fica intacto;
//   3. trailer com rampa == arte parada (GFX_HERO com a mesma imagem), pixel a
//      pixel dentro de uma tolerancia de dither: mesmo enquadramento.
// Capturas opcionais em argv[1]: antes.bmp, depois.bmp, arte.bmp.
//   bash tests/hero_trailer_rampa_shot.sh [pasta]
#include "gfx.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int LW = 1920, LH = 1080;
static GLuint fbo, fboTex;
static const float BG = 13.0f / 255.0f;   // #0d0d0d, o fundo da home

static unsigned char *ler(void) {
  unsigned char *p = malloc((size_t)LW * LH * 4);
  assert(p);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, p);
  return p;   // linha 0 = base da tela
}
static const unsigned char *px(const unsigned char *p, int x, int y) { return p + ((size_t)(LH - 1 - y) * LW + x) * 4; }
static void salvar(const char *dir, const char *nome, const unsigned char *p) {
  SDL_Surface *s; char cam[700]; int y;
  if (!dir) return;
  s = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 32, SDL_PIXELFORMAT_RGBA32); assert(s);
  for (y = 0; y < LH; y++) memcpy((char *)s->pixels + y * s->pitch, px(p, 0, y), (size_t)LW * 4);
  snprintf(cam, sizeof cam, "%s/%s", dir, nome);
  assert(SDL_SaveBMP(s, cam) == 0); SDL_FreeSurface(s); printf("captura: %s\n", cam);
}
static void quadro(void) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro(); glDisable(GL_SCISSOR_TEST);
  glClearColor(BG, BG, BG, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
}
// Quadro de "video": faixas de cor saturada e uma grade, sem nada escuro.
static GLuint quadroVideo(unsigned char **copia, int *vw, int *vh) {
  int w = (int)NV_HERO_ARTE_W, h = (int)NV_HERO_ARTE_H, x, y;
  unsigned char *p = malloc((size_t)w * h * 4);
  GLuint t;
  assert(p);
  for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
    unsigned char *q = p + ((size_t)y * w + x) * 4;
    float u = (float)x / w, v = (float)y / h;
    int grade = (x % 120) < 3 || (y % 120) < 3;
    q[0] = grade ? 255 : (unsigned char)(80 + 175 * u);
    q[1] = grade ? 255 : (unsigned char)(200 - 120 * v);
    q[2] = grade ? 255 : (unsigned char)(220 - 140 * u);
    q[3] = 255;
  }
  glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  *copia = p; *vw = w; *vh = h;
  return t;
}
// O plano de video sob a UI: final = ui + video * (1 - alfa da UI).
static void compor(unsigned char *ui, GfxRect r, const unsigned char *v, int vw, int vh) {
  int x, y;
  for (y = 0; y < LH; y++) for (x = 0; x < LW; x++) {
    unsigned char *q = (unsigned char *)px(ui, x, y);
    float a = q[3] / 255.0f;
    int k, vx = x - (int)r.x, vy = y - (int)r.y;
    const unsigned char *s = NULL;
    if (vx >= 0 && vy >= 0 && vx < vw && vy < vh) s = v + ((size_t)vy * vw + vx) * 4;
    for (k = 0; k < 3; k++) {
      float c = q[k] + (s ? s[k] : 0) * (1.0f - a);
      q[k] = (unsigned char)(c > 255.0f ? 255.0f : c + 0.5f);
    }
    q[3] = 255;
  }
}
static int dif(const unsigned char *a, const unsigned char *b) {
  int k, m = 0;
  for (k = 0; k < 3; k++) { int d = abs(a[k] - b[k]); if (d > m) m = d; }
  return m;
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : NULL;
  GfxRect r = { NV_HERO_ARTE_X, 0, NV_HERO_ARTE_W, NV_HERO_ARTE_H };
  unsigned char *video, *antes, *depois, *arte;
  int vw, vh, x, y, piorMiolo = 0, piorArte = 0;
  GLuint tv;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("rampa", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  tv = quadroVideo(&video, &vw, &vh);

  // 1. ANTES: so o furo (o 2.0.1).
  quadro(); gfx_furo(r);
  antes = ler(); compor(antes, r, video, vw, vh);
  // 2. DEPOIS: furo + rampas do hero com alfa 1 (trailer ja entrou).
  quadro(); gfx_furo(r);
  gfx_rect(r, 0, GFX_HERO, 0, 1.0f, 0, 0.0f, 0, 0, 0, 1.0f);
  depois = ler();
  { const unsigned char *q = px(depois, (int)(r.x + 0.7f * r.w), 300), *q2 = px(depois, (int)r.x + 1, 300);
    printf("ui depois: miolo %d,%d,%d,%d borda %d,%d,%d,%d\n", q[0], q[1], q[2], q[3], q2[0], q2[1], q2[2], q2[3]); }
  compor(depois, r, video, vw, vh);
  // 3. ARTE PARADA: a mesma imagem como arte do hero.
  quadro();
  gfx_tex_aspect_atual = NV_HERO_ARTE_W / NV_HERO_ARTE_H;
  gfx_rect(r, tv, GFX_HERO, 0, 0, 0, 0, 0, 0, 0, 1.0f);
  gfx_tex_aspect_atual = 0.0f;
  arte = ler();

  // Antes: a primeira coluna do video e o video cru (borda dura).
  { const unsigned char *e = px(antes, (int)r.x + 1, 300), *v = video + ((size_t)300 * vw + 1) * 4;
    printf("antes, borda esquerda: %d,%d,%d (video %d,%d,%d)\n", e[0], e[1], e[2], v[0], v[1], v[2]);
    assert(dif(e, v) <= 2); }
  // Depois: a borda funde no fundo da home.
  { const unsigned char *e = px(depois, (int)r.x + 1, 300);
    printf("depois, borda esquerda: %d,%d,%d (fundo 13)\n", e[0], e[1], e[2]);
    assert(e[0] <= 16 && e[1] <= 16 && e[2] <= 16); }
  // Depois: o miolo (fora das rampas: x > 45% e y < 82%) e o video intacto.
  for (y = 20; y < (int)(0.80f * r.h); y += 23)
    for (x = (int)(r.x + 0.46f * r.w); x < LW - 2 && x < (int)(r.x + r.w) - 2; x += 29) {
      const unsigned char *v = video + ((size_t)(y - (int)r.y) * vw + (x - (int)r.x)) * 4;
      int d = dif(px(depois, x, y), v);
      if (d > piorMiolo) piorMiolo = d;
    }
  printf("miolo do trailer: diferenca maxima %d\n", piorMiolo);
  assert(piorMiolo <= 1);
  // Trailer com rampa == arte parada (mesmo enquadramento), no retangulo todo.
  for (y = 2; y < (int)r.h - 2; y += 11)
    for (x = (int)r.x + 2; x < LW - 2 && x < (int)(r.x + r.w) - 2; x += 13) {
      int d = dif(px(depois, x, y), px(arte, x, y));
      if (d > piorArte) piorArte = d;
    }
  printf("trailer com rampa x arte parada: diferenca maxima %d\n", piorArte);
  assert(piorArte <= 4);

  salvar(dir, "antes.bmp", antes);
  salvar(dir, "depois.bmp", depois);
  salvar(dir, "arte.bmp", arte);
  free(antes); free(depois); free(arte); free(video);
  puts("hero_trailer_rampa: ok");
  return 0;
}
