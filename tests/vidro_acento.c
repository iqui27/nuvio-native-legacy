// Selected/focused pill in glass mode (owner: the accent state must look like
// glass too, without losing the vivid colour). With glass on, a coloured accent
// pill keeps >= 90 % of the solid accent chroma, has a top specular band and a
// rim; with glass off it is the unchanged solid accent.
//   bash tests/vidro_acento.sh [pasta-de-capturas]
#include "gfx.h"
#include "corviva.h"
#include "ajustes.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;
static int falhas;

static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASSA" : "FALHA", nome);
  falhas += !ok;
}
static void media(int x, int y, int w, int h, double m[3]) {
  unsigned char *p = malloc((size_t)w * h * 4);
  int i;
  assert(p);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(x, LH - y - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);
  m[0] = m[1] = m[2] = 0;
  for (i = 0; i < w * h; i++) { m[0] += p[4*i]; m[1] += p[4*i+1]; m[2] += p[4*i+2]; }
  for (i = 0; i < 3; i++) m[i] /= (double)(w * h);
  free(p);
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
  assert(IMG_SavePNG(s, cam) == 0); SDL_FreeSurface(s); printf("captura: %s\n", cam);
}
// O quadro do Frost com "Vidro fosco": a luz da paleta do titulo (verde)
// preparada como antes era assada para o fosco, a forca devolvida a 0, e o
// fundo de tela cheia pintado por cima.
static void quadro(float fundo) {
  static const float verde[4][3] = { {0.10f,0.55f,0.12f}, {0.20f,0.70f,0.18f},
                                     {0.05f,0.45f,0.10f}, {0.15f,0.60f,0.15f} };
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro();
  glDisable(GL_SCISSOR_TEST);
  memcpy(nv_ambiente_viva, verde, sizeof verde);
  nv_ambiente_forca = 1.0f; nv_tempo_viva = 0.0f;
  gfx_ambiente_preparar();
  nv_ambiente_forca = 0.0f;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, fundo, fundo, fundo, 1.0f);
}
extern void ajustes_teste_tema(int tema, int vidro);
static void pixel(int x, int y, double m[3]) { media(x, y, 1, 1, m); }
static double croma(const double m[3]) {
  double mx = m[0] > m[1] ? (m[0] > m[2] ? m[0] : m[2]) : (m[1] > m[2] ? m[1] : m[2]);
  double mn = m[0] < m[1] ? (m[0] < m[2] ? m[0] : m[2]) : (m[1] < m[2] ? m[1] : m[2]);
  return mx - mn;
}
static double luma(const double m[3]) { return 0.2126 * m[0] + 0.7152 * m[1] + 0.0722 * m[2]; }

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : NULL;
  static const struct { int tema; const char *nome; } T[] = { { 1, "carmesim" }, { 3, "violeta" } };
  GfxRect pill = { 800, 400, 320, 96 };
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("vidro", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();

  for (unsigned t = 0; t < sizeof T / sizeof *T; t++) {
    double solido[3], vidro[3], meio[3], topo[3], aro[3];
    float ar, ag, ab;
    char n[160];
    // Glass OFF: the solid accent, unchanged.
    ajustes_teste_tema(T[t].tema, 0);
    assert(!ajustes_vidro());
    ajustes_acento(&ar, &ag, &ab);
    quadro(0.10f);
    gfx_vidro_pilula_cheia(pill, 0.5f, 1.0f, 1.0f);
    media((int)pill.x + 60, (int)pill.y + 30, 200, 36, solido);
    snprintf(n, sizeof n, "%s glass off: solid accent (%.0f,%.0f,%.0f)", T[t].nome, solido[0], solido[1], solido[2]);
    confere(fabs(solido[0] - ar * 255) <= 2 && fabs(solido[1] - ag * 255) <= 2 && fabs(solido[2] - ab * 255) <= 2, n);
    snprintf(n, sizeof n, "%s-solido.png", T[t].nome); salvar(dir, n);
    // Glass ON, over the glass pill the callers draw under it.
    ajustes_teste_tema(T[t].tema, 1);
    assert(ajustes_vidro() && ajustes_vidro_fosco());
    for (int q = 0; q < 2; q++) {
      quadro(0.10f);
      gfx_vidro_painel(pill, 0.5f, 0.55f, 1.0f);
      gfx_vidro_pilula_cheia(pill, 0.5f, 1.0f, 1.0f);
    }
    media((int)pill.x + 60, (int)pill.y + 40, 200, 24, meio);
    media((int)pill.x + 60, (int)pill.y + 3, 200, 8, topo);
    pixel((int)pill.x + 160, (int)pill.y, aro);
    snprintf(n, sizeof n, "%s glass on: chroma %.1f of solid %.1f (%.0f%%)", T[t].nome, croma(meio), croma(solido),
             100.0 * croma(meio) / croma(solido));
    confere(croma(meio) >= 0.90 * croma(solido), n);
    snprintf(n, sizeof n, "%s glass on: top specular (luma top %.1f > middle %.1f)", T[t].nome, luma(topo), luma(meio));
    confere(luma(topo) > luma(meio) + 4.0, n);
    snprintf(n, sizeof n, "%s glass on: rim (edge luma %.1f > middle %.1f)", T[t].nome, luma(aro), luma(meio));
    confere(luma(aro) > luma(meio) + 8.0, n);
    snprintf(n, sizeof n, "%s-vidro.png", T[t].nome); salvar(dir, n);
  }
  // Library tabs/chips have no glass underneath. A hard backdrop edge must
  // become a ramp inside the standalone accent, while retaining response.
  {
    double v[4][3];
    const int x[] = { 944, 956, 964, 976 };
    for (int q = 0; q < 2; q++) {
      quadro(0);
      gfx_recorte(960, 0, 960, LH);
      gfx_cor((GfxRect){0, 0, LW, LH}, 0, 1, 1, 1, 1);
      gfx_sem_recorte();
      gfx_vidro_pilula_cheia(pill, .5f, 1, 1);
    }
    for (int i = 0; i < 4; i++) pixel(x[i], 460, v[i]);
    printf("standalone ramp: %.2f %.2f %.2f %.2f\n", luma(v[0]), luma(v[1]), luma(v[2]), luma(v[3]));
    confere(luma(v[1]) > luma(v[0]) + .3 && luma(v[3]) > luma(v[2]) + .3,
            "standalone accent frosts both sides of backdrop edge");
    salvar(dir, "violeta-standalone.png");
  }
  printf("vidro_acento: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
