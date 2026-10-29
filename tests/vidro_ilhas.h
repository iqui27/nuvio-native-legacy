// Frost proof for the islands that sit over a busy Home (Recommend to a friend,
// Add people). Scene = art backdrop + hard black/white stripes (the rows) + a
// glass bar crossing the panel (the Home sidebar). Under a frosted panel the
// stripes are blurred away; under a flat translucent one they show through.
// ilhaGrad = mean |dx| of the luma in a rect (0 = flat, ~100+ = sharp stripes).
#ifndef NV_TESTS_VIDRO_ILHAS_H
#define NV_TESTS_VIDRO_ILHAS_H
#include "gfx.h"
#include "vidro_fundo.h"
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ilhaCena(void) {
  int i;
  glClearColor(.03f, .03f, .035f, 1); glClear(GL_COLOR_BUFFER_BIT);
  vidroFundoDesenhar();
  for (i = 0; i < 160; i++) gfx_cor((GfxRect){ i * 12.0f, 150, 6, 780 }, 0, 1, 1, 1, .95f);
  if (getenv("NUVIO_ILHA_BARRA")) gfx_vidro_folha((GfxRect){ 40, 100, 600, 880 }, 28.0f / 880, 1);
}
static double ilhaVariancia;
static double ilhaGrad(int x, int y, int w, int h) {
  unsigned char *p = malloc((size_t)w * h * 4);
  double s = 0, sum = 0, sum2 = 0;
  int i, j;
  if (!p) exit(2);
  glFinish();
  glReadPixels(x, 1080 - y - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);
  for (j = 0; j < h; j++)
    for (i = 1; i < w; i++) {
      unsigned char *a = p + ((size_t)j * w + i - 1) * 4, *b = a + 4;
      s += fabs((b[0] + b[1] + b[2]) / 3.0 - (a[0] + a[1] + a[2]) / 3.0);
    }
  for (i = 0; i < w * h; i++) {
    double luma = (p[i * 4] + p[i * 4 + 1] + p[i * 4 + 2]) / 3.0;
    sum += luma; sum2 += luma * luma;
  }
  ilhaVariancia = sum2 / (w * h) - pow(sum / (w * h), 2);
  free(p);
  return s / ((double)(w - 1) * h);
}
static void ilhaPng(const char *dir, const char *nome) {
  char cam[800];
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  if (!pix || !s) exit(2);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s/%s.png", dir, nome);
  if (IMG_SavePNG(s, cam) != 0) exit(2);
  SDL_FreeSurface(s); free(pix);
}
// A strip of the panel padding (no text there): only what is behind shows.
// Open stripes read ~26; a frosted panel blurs them to < 2.
static int ilhaConfere(const char *nome, int x, int y, int w, int h) {
  double g = ilhaGrad(x, y, w, h);
  // Require backdrop variation as well as softened edges and an actual source.
  int ok = g < 2.0 && ilhaVariancia > 0.25 && gfx_vidro_com_fonte > 0;
  printf("%s: %s under-panel dx=%.2f variance=%.2f source=%d (stripes alone ~26)\n",
         ok ? "PASS" : "FAIL", nome, g, ilhaVariancia, gfx_vidro_com_fonte);
  return ok;
}
#endif
