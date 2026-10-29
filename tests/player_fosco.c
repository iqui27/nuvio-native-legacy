// GLASS OVER LIVE VIDEO (owner, 17/10: "o player do nuvio arc mostrava o video
// atras dos botoes transparentes"). The video is a hardware plane behind the GL
// surface, so wherever the GL alpha is < 1 it shows through. Per scene this
// reads the RGBA framebuffer: pixels with partial alpha (translucent glass),
// pixels in the glass tint band, plus captures / blur passes / blurred glasses
// over every frame of the scene. It also composites the frame over a bright
// colorful pattern standing in for the plane and saves it as PNG.
// See tests/player_fosco.sh.
#include "gfx.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
static int foscoFonte, foscoChapado, foscoCap, foscoPas;
static void fosco_quadro(void) {
  foscoFonte += gfx_vidro_com_fonte; foscoChapado += gfx_vidro_chapados;
  foscoCap += gfx_vidro_capturas; foscoPas += gfx_vidro_passadas;
  gfx_novo_quadro();
}
#define tex_novo_quadro() (fosco_quadro(), tex_novo_quadro())
#define FOSCO_ZERA() (foscoFonte = foscoChapado = foscoCap = foscoPas = 0)
static void fosco_medir(const char *id);
#define FOSCO_N(id) fosco_medir(id)
#include "player_glass_shot.c"
static void fosco_medir(const char *id) {
  unsigned char *px = malloc((size_t)LW * LH * 4), *o;
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int x, y, parcial = 0, banda = 0, opaco = 0;
  char nome[800];
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (y = 0; y < LH; y++) for (x = 0; x < LW; x++) {
    const unsigned char *p = px + ((size_t)(LH - 1 - y) * LW + x) * 4;   // top-down
    float a = p[3] / 255.0f;
    // The plane: bright rainbow with diagonal stripes (worst case for the labels).
    float v[3] = { 0.5f + 0.5f * (x % 640 < 320 ? 1.0f : 0.6f), 0.5f + 0.5f * (float)x / LW, 0.55f + 0.45f * (float)y / LH };
    if (((x + y) / 48) & 1) { v[0] *= 0.8f; v[1] *= 0.8f; v[2] *= 0.8f; }
    o = (unsigned char *)s->pixels + (size_t)y * s->pitch + x * 3;
    // GL color is already weighted by its alpha (SRC_ALPHA blend onto black).
    { int k; for (k = 0; k < 3; k++) { float c = p[k] / 255.0f + (1.0f - a) * v[k]; o[k] = (unsigned char)(c > 1.0f ? 255.0f : c * 255.0f); } }
    parcial += p[3] > 0 && p[3] < 255; banda += p[3] >= 64 && p[3] <= 150; opaco += p[3] == 255;
  }
  snprintf(nome, sizeof nome, "%s/%s-%s.png", saida, id, material);
  IMG_SavePNG(s, nome);
  SDL_FreeSurface(s); free(px);
  printf("F %s parcial=%d banda=%d opaco=%d fonte=%d cap=%d pas=%d\n", id, parcial, banda, opaco, foscoFonte, foscoCap, foscoPas);
}
