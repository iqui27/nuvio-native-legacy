// Live-video glass: real shader + governor path without the player. The glass
// over a video hole must be a real glass look (translucent tint + in-shader
// rim/top highlight), never a flat box, and never an art/frost source. The
// scrim keeps labels readable over the brightest frame; an interrupted
// governor trial restores frost and ends governing. See tests/vidro_video.sh.
#include "gfx.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static const int LW = 1920, LH = 1080;
static int falhas;
static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", nome);
  falhas += !ok;
}
static float luma(float c) { return c <= .04045f ? c / 12.92f : powf((c + .055f) / 1.055f, 2.4f); }
static float branco(const unsigned char *px, int x, int y) {
  const unsigned char *p = px + ((size_t)(LH - 1 - y) * LW + x) * 4;
  return luma((p[0] + 255 - p[3]) / 255.0f);
}
int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("vidro", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  GLuint fbo, tex;
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1);
  setenv("NUVIO_SHOT_VIDRO_QUAL", "0", 1); ajustes_teste_vidro_env();

  unsigned char *px = malloc((size_t)LW * LH * 4); assert(px);
  GfxRect r = { 200, 200, 400, 200 };
  // One glass over a full-frame video hole, contour preference OFF: rim/top
  // highlight must come from the glass shader itself, no source, no capture.
  gfx_novo_quadro(); glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
  gfx_furo((GfxRect){ 0, 0, LW, LH });
  gfx_vidro_material(r, .1f, .8f, 1);
  confere(!gfx_vidro_com_fonte && !gfx_vidro_capturas && !gfx_vidro_passadas,
          "over live video: translucent glass, no blur source/capture");
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, px);
  int top = px[((LH - 1 - 200) * LW + 400) * 4];
  int side = px[((LH - 1 - 300) * LW + 200) * 4];
  int bottom = px[((LH - 1 - 399) * LW + 400) * 4];
  confere(top > side + 4 && side > 12 && bottom > 12,
          "glass rim/top highlight visible with contour off");
  printf("mid alpha=%d top=%d side=%d bottom=%d\n", px[((LH-1-300)*LW+400)*4+3], top, side, bottom);

  // Contrast of the panel's dimmest text over the worst case: a pure white frame.
  float lo = 1, hi = 0;
  for (int y = 205; y < 395; y++) for (int x = 205; x < 595; x++) {
    float v = branco(px, x, y); if (v < lo) lo = v; if (v > hi) hi = v;
  }
  float c = (hi + .05f) / (lo + .05f);
  printf("panel contrast over white: %.2f\n", c);
  confere(c >= 4.5f, "dim label contrast >= 4.5 over white video");

  // Governor through the drawn glass counters, as app.c feeds it: 4 janky
  // windows start one trial; a workload change drops it, restores frost and
  // ends governing — never a second trial.
  for (int phase = 0; phase < 3; phase++) {
    gfx_novo_quadro();
    gfx_cor((GfxRect){ 0, 0, LW, LH }, 0, .1f, .2f, .3f, 1);   // page behind, not a video hole
    gfx_vidro_material(phase == 1 ? (GfxRect){ 200, 200, 1400, 400 } : r, .1f, .8f, 1);
    int nf = gfx_vidro_com_fonte, nc = gfx_vidro_chapados, ar = gfx_vidro_area;
    printf("phase %d drawn: fonte=%d chapados=%d area=%d\n", phase, nf, nc, ar);
    for (int frame = (phase == 1 ? 150 : 600); frame--;) {
      gfx_vidro_com_fonte = nf; gfx_vidro_chapados = nc; gfx_vidro_area = ar;
      gfx_vidro_governar(40, 16.7);
    }
    confere(gfx_vidro_degradado() == (phase == 0), phase == 0 ? "governor: 4 janky windows start the trial" :
            phase == 1 ? "interrupted trial: frost back, governor done" : "governor: never a second trial");
  }
  free(px);
  printf("vidro_video: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
