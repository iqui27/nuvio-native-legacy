// Glass budget per screen (tests/vidro_telas.sh). With NUVIO_VIDRO_TELAS set,
// a *_shot.c harness runs its screen static and while moving focus and
// prints captures, passes and blur pixels per frame. Each frame also runs
// corviva_quadro like main.c does (the live clock that uTempo shaders read).
#ifndef NV_TESTS_VIDRO_TELAS_H
#define NV_TESTS_VIDRO_TELAS_H
#include "ajustes.h"
#include "corviva.h"
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int vtCap, vtPas, vtMax, vtN, vtFalhas;
static double vtFill, vtTela;
static int vtRect, vtTxt0, vtTxt;

static int vt_ligado(void) { return getenv("NUVIO_VIDRO_TELAS") != NULL; }
// Glass UI with the frosted backdrop on, as on the owner's C9.
static void vt_vidro(void) {
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1);
  ajustes_teste_vidro_env();
}
static void vt_antes(void) {
  if (vt_ligado()) corviva_quadro(1.0f / 60.0f, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
}
static void vt_depois(void) {
  vtTela += gfx_fill; vtRect += gfx_n_rect; vtTxt += txt_rasterizadas - vtTxt0; vtTxt0 = txt_rasterizadas;
  vtCap += gfx_vidro_capturas; vtPas += gfx_vidro_passadas; vtFill += gfx_vidro_fill; vtN++;
  if (gfx_vidro_capturas > vtMax) vtMax = gfx_vidro_capturas;
}
static void vt_zerar(void) { vtCap = vtPas = vtMax = vtN = 0; vtFill = vtTela = 0; vtRect = vtTxt = 0; vtTxt0 = txt_rasterizadas; }
// parado = 1: budget 0 captures; parado = 0: at most 1 per frame. A screen
// with a live animated background (Explore sky) passes parado = 2: at most
// one capture per 30 frames, the 2 Hz the sky's clock is hashed at.
static void vt_relatar(const char *tela, int parado) {
  int ok = parado == 2 ? vtMax <= 1 && vtCap * 30 <= vtN + 29 : parado ? vtCap == 0 : vtMax <= 1;
  double n = vtN ? vtN : 1;
  // Fill budget (full 1080p screens per frame) and lines rasterized per frame.
  // Before the overdraw pass: comments 3.92, sources 6.27, search 2.64 with
  // 3.50 lines/frame moving, explore 2.45, settings 4.40.
  static const struct { const char *tela; double fill, txt; } orc[] = {
    { "detail-comments", 3.35, 0.5 }, { "detail-sources", 5.40, 0.5 }, { "search", 2.55, 0.5 },
    { "explore", 2.20, 0.5 }, { "settings", 4.20, 0.5 } };
  for (size_t k = 0; k < sizeof orc / sizeof orc[0]; k++)
    if (!strcmp(orc[k].tela, tela) && (vtTela / n > orc[k].fill || vtTxt / n > orc[k].txt)) {
      printf("FAIL: %s %s fill %.2f > %.2f or text-raster %.2f > %.2f\n", tela, parado ? "static" : "moving",
             vtTela / n, orc[k].fill, vtTxt / n, orc[k].txt);
      ok = 0;
    }
  printf("vidro_telas %s %s: captures/frame=%.2f passes/frame=%.2f blur-pixels/frame=%.0f max=%d frames=%d fill/frame=%.2f rects/frame=%.0f text-raster/frame=%.2f\n",
         tela, parado ? "static" : "moving", vtCap / n, vtPas / n, vtFill / n * 1920 * 1080, vtMax, vtN,
         vtTela / n, vtRect / n, vtTxt / n);
  printf("%s: %s %s budget\n", ok ? "PASS" : "FAIL", tela, parado ? "static" : "moving");
  vtFalhas += !ok;
  vt_zerar();
}
// 1920x1080 PNG of the bound framebuffer into $NUVIO_VIDRO_TELAS/<nome>.png.
static void vt_png(const char *nome) {
  char cam[700];
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  if (!pix || !s) exit(2);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s/%s.png", getenv("NUVIO_VIDRO_TELAS"), nome);
  if (IMG_SavePNG(s, cam) != 0) exit(2);
  SDL_FreeSurface(s); free(pix);
}
#endif
