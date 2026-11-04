#include "arte_medida.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int arte_medida_ligada = -1;
// One slot per resource seen; as many as the cap, so it never fills first.
static struct { unsigned tex; int w, h; } vistos[ARTE_MEDIDA_TETO];
static int nVistos, nLinhas;

int arte_medida_resolver(void) {
  const char *e = getenv("NV_ART_DRAW_MEASURE");
  arte_medida_ligada = e && !strcmp(e, "1");
  nVistos = nLinhas = 0;
  return arte_medida_ligada;
}

static int muda(int novo, int velho) { return abs(novo - velho) * 4 >= velho; }   // >= 25%

int arte_medida_amostra(unsigned tex, int modo, float texAsp, int contem,
                        float x0, float y0, float x1, float y1, const int *clip,
                        int rtW, int rtH, float esc, float pxUnit,
                        int (*dims)(unsigned tex, int *w, int *h),
                        char *out, size_t n) {
  if (arte_medida_ligada <= 0 || !tex || rtW <= 0 || rtH <= 0 || !dims) return 0;
  if (!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1) || !isfinite(texAsp)) return 0;
  float qw = x1 - x0, qh = y1 - y0;
  int dw = (int)lroundf(qw), dh = (int)lroundf(qh);
  if (dw <= 0 || dh <= 0) return 0;
  int s = 0;
  while (s < nVistos && vistos[s].tex != tex) s++;
  if (s < nVistos && !muda(dw, vistos[s].w) && !muda(dh, vistos[s].h)) return 0;
  int tw = 0, th = 0;
  if (!dims(tex, &tw, &th) || tw <= 0 || th <= 0) return 0;   // not a cache texture
  if (s == nVistos) { if (nVistos == ARTE_MEDIDA_TETO) return 0; vistos[nVistos++].tex = tex; }
  vistos[s].w = dw; vistos[s].h = dh;
  // Physical size of the WHOLE decoded image as the shader maps it.
  // cover (coverAsp in gfx.c): fills the quad, the excess axis is cropped;
  // contain (GFX_CARD, aspect off by more than 25%): fits inside the quad;
  // stretch (texAsp 0): the quad itself.
  const char *fit = "stretch";
  float iw = qw, ih = qh, qa = qw / qh;
  if (texAsp > 0.0f) {
    float ra = qa / texAsp;
    int fora = contem && texAsp > 0.05f && (ra < 0.80f || ra > 1.25f);
    fit = fora ? "contain" : "cover";
    if ((qa > texAsp) != fora) ih = qw / texAsp;   // full width, height cropped/letterboxed
    else iw = qh * texAsp;
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
  snprintf(out, n, "[art-draw] art=%d n=%d mode=%d dst=%dx%d vis=%dx%d rt=%dx%d tex=%dx%d fit=%s "
           "src=%dx%d px_per_texel_milli=%ldx%ld layer_milli=%ld px_per_unit_milli=%ld\n",
           s + 1, nLinhas, modo, dw, dh, vw, vh, rtW, rtH, tw, th, fit, sw, sh,
           lroundf(iw * 1000.0f / tw), lroundf(ih * 1000.0f / th),
           lroundf(esc * 1000.0f), lroundf(pxUnit * 1000.0f));
  if (nLinhas >= ARTE_MEDIDA_TETO) arte_medida_ligada = 0;
  return 1;
}
