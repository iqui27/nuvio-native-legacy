#include "arte_medida.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int arte_medida_ligada = -1;
// One slot per resource seen; as many as the cap, so it never fills first.
static struct { unsigned tex; int w, h; } vistos[ARTE_MEDIDA_TETO];
// Names whose lookup failed (not a ready cache texture, or a bad size).
static unsigned faltas[ARTE_MEDIDA_TETO];
static int nVistos, nLinhas, nFaltas, nBuscas;

int arte_medida_resolver(void) {
  const char *e = getenv("NV_ART_DRAW_MEASURE");
  arte_medida_ligada = e && !strcmp(e, "1");
  nVistos = nLinhas = nFaltas = nBuscas = 0;
  return arte_medida_ligada;
}

static int muda(int novo, int velho) { return abs(novo - velho) * 4 >= velho; }   // >= 25%

int arte_medida_amostra(unsigned tex, int modo, float texAsp, int contem,
                        float asp, const float *sub,
                        float x0, float y0, float x1, float y1, const int *clip,
                        int rtW, int rtH, float esc, float pxUnit,
                        int (*dims)(unsigned tex, int *w, int *h),
                        char *out, size_t n) {
  if (arte_medida_ligada <= 0 || !tex || rtW <= 0 || rtH <= 0 || !dims) return 0;
  // gfx_rect_furos strips: the quad is only a uSub window of the rect.
  if (!sub || sub[0] != 0.0f || sub[1] != 0.0f || sub[2] != 1.0f || sub[3] != 1.0f) return 0;
  if (!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1) || !isfinite(texAsp) ||
      !isfinite(asp) || asp <= 0.0f) return 0;
  float qw = x1 - x0, qh = y1 - y0;
  int dw = (int)lroundf(qw), dh = (int)lroundf(qh);
  if (dw <= 0 || dh <= 0) return 0;
  int s = 0;
  while (s < nVistos && vistos[s].tex != tex) s++;
  if (s < nVistos && !muda(dw, vistos[s].w) && !muda(dh, vistos[s].h)) return 0;
  if (s == ARTE_MEDIDA_TETO) return 0;
  for (int i = 0; i < nFaltas; i++) if (faltas[i] == tex) return 0;
  int tw = 0, th = 0, ok = dims(tex, &tw, &th) && tw > 0 && th > 0;
  if (++nBuscas >= ARTE_MEDIDA_BUSCAS) arte_medida_ligada = 0;
  if (!ok) {   // not a cache texture: never looked up again
    if (nFaltas < ARTE_MEDIDA_TETO) faltas[nFaltas++] = tex;
    return 0;
  }
  if (s == nVistos) vistos[nVistos++].tex = tex;
  vistos[s].w = dw; vistos[s].h = dh;
  // Physical size of the WHOLE decoded image as the shader maps it, in uv
  // space first (ra = uAspect / uTexAsp, as in gfx.c), then per axis to px.
  // fit 1 cover (coverAsp): fills the quad, the excess axis is cropped;
  // fit 2 contain (GFX_CARD, ratio off by more than 25%): fits inside the quad;
  // fit 0 stretch (texAsp 0): the quad itself.
  int fit = 0;
  float iw = qw, ih = qh;
  if (texAsp > 0.0f) {
    float ra = asp / texAsp;
    int fora = contem && texAsp > 0.05f && (ra < 0.80f || ra > 1.25f);
    fit = fora ? 2 : 1;
    if ((ra > 1.0f) != fora) ih = qh * ra;   // full width, height cropped/letterboxed
    else iw = qw / ra;
  }
  // Texels inside the quad (cover crops; contain/stretch show all of them).
  int sw = (int)lroundf(iw > qw ? tw * qw / iw : (float)tw);
  int sh = (int)lroundf(ih > qh ? th * qh / ih : (float)th);
  // Quad clipped by the render target and the scissor.
  float cx0 = fmaxf(x0, 0.0f), cy0 = fmaxf(y0, 0.0f), cx1 = fminf(x1, (float)rtW), cy1 = fminf(y1, (float)rtH);
  if (clip) {
    cx0 = fmaxf(cx0, (float)clip[0]); cy0 = fmaxf(cy0, (float)clip[1]);
    cx1 = fminf(cx1, (float)(clip[0] + clip[2])); cy1 = fminf(cy1, (float)(clip[1] + clip[3]));
  }
  int vw = cx1 > cx0 ? (int)lroundf(cx1 - cx0) : 0, vh = cy1 > cy0 ? (int)lroundf(cy1 - cy0) : 0;
  nLinhas++;
  snprintf(out, n, "[art-draw] art=%d n=%d mode=%d dst=%dx%d vis=%dx%d rt=%dx%d tex=%dx%d fit=%d "
           "src=%dx%d px_per_texel_milli=%ldx%ld layer_milli=%ld px_per_unit_milli=%ld\n",
           s + 1, nLinhas, modo, dw, dh, vw, vh, rtW, rtH, tw, th, fit, sw, sh,
           lroundf(iw * 1000.0f / tw), lroundf(ih * 1000.0f / th),
           lroundf(esc * 1000.0f), lroundf(pxUnit * 1000.0f));
  if (nLinhas >= ARTE_MEDIDA_TETO) arte_medida_ligada = 0;
  return 1;
}
