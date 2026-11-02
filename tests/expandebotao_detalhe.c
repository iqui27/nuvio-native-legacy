// #205, REACHABLE PATH: runs the production "Assistir do comeco" block,
// acoesAgrupadas() and desenhaSecundario() extracted verbatim from
// src/detail.c by expandebotao.sh (no parallel model). Drawing/text are stubs
// that record what the block would paint.
//
// What this proves: acoesAgrupadas() is 0 in production (13b4a5c83), so the
// grouped expanding pill (anim_expande + txt_linha) is NOT reachable; the
// shipped button is the plain secondary pill. Its width is fixed from the
// first frame, and its label is drawn whole on every frame, focused or not.
// The "label typed in by a growing width" bug cannot happen on this path.
// The grouped branch stays covered only as the anim_expande rule
// (tests/expandebotao.c) plus the .sh source greps.
#include <assert.h>
#include <stdio.h>
#include "../src/anim.h"
#include "expandebotao_bloco.h" // generated: NV_DETW* constants

typedef unsigned Uint32;
typedef struct { float x, y, w, h; } GfxRect;
typedef struct { float w, h; int cortado; } TxtLinha;

#define TXT_DET_BOTAO 0
#define NV_RAIO_PILL .5f
#define LABEL_W 150.0f   // ink width of "Assistir do comeco" in the stub

static Uint32 agora = 1000;
static int retrato = 1, inicio = 1, nivel, botao, idx;
static int pilulas, rotulos, cortes;
static GfxRect ultimaPilula;
static float ultimoAlpha;

static Uint32 SDL_GetTicks(void) { return agora; }
static int detalheRetrato(void) { return retrato; }
static int temInicio(void) { return inicio; }
static int ajustes_vidro(void) { return 0; }
static int gfx_vidro_tinta(float f) { (void)f; return 235; }
static float ajustes_acento_tinta(void *r, void *g, void *b) { (void)r; (void)g; (void)b; return 1; }
static float larguraTextoHero(void) { return 2000; }   // wide hero: no clamp
static const char *i18n(const char *s) { return s; }
#define foco_anel(...) ((void)0)
#define gfx_icone(...) ((void)0)
#define ponteiro_alvo(...) ((void)0)
static void botao_pilula(GfxRect r, const char *t, const void *i, float f, int a, int b, float al) {
  (void)t; (void)i; (void)f; (void)a; (void)b; (void)al;
  pilulas++; ultimaPilula = r;
}
static TxtLinha txt_linha(int e, const char *s, int r, int g, int b, int a) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)a;
  return (TxtLinha){LABEL_W, 24, 0};
}
static TxtLinha txt_linha_corta(int e, const char *s, int r, int g, int b, int a, float max) {
  TxtLinha l = txt_linha(e, s, r, g, b, a);
  if (max < l.w) { l.w = max; l.cortado = 1; }
  return l;
}
static void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) {
  (void)x; (void)y; rotulos++; cortes += l.cortado; ultimoAlpha = a;
}
static GfxRect heroAcaoRect(float *x, float *y, float w, float h) {
  return (GfxRect){*x, *y - h * .5f, w, h};
}

#include "expandebotao_bloco.c" // generated: production functions + block

int main(void) {
  // Production gate: the grouped (expanding) branch is off.
  assert(acoesAgrupadas() == 0);

  const float largura = NV_DETW_BTN2_PADX * 2 + LABEL_W;
  // Focus the button for 30 frames, then leave it for 30 (60 fps).
  for (int q = 0; q < 60; q++) {
    botao = q < 30 ? 1 : 0;
    pilulas = rotulos = cortes = 0;
    agora += 16;
    bloco();
    assert(pilulas == 1);
    assert(ultimaPilula.w == largura);      // fixed width, no growth
    assert(ultimaPilula.h == NV_DETW2_BTN_H);
    assert(rotulos == 1 && cortes == 0);    // whole label, every frame
    assert(ultimoAlpha == 1.0f);            // no fade-in either
  }

  // Off the gate (landscape or no restart), the block draws nothing.
  retrato = 0; pilulas = rotulos = 0; bloco();
  assert(pilulas == 0 && rotulos == 0);
  retrato = 1; inicio = 0; bloco();
  assert(pilulas == 0 && rotulos == 0);

  puts("ok #205 reachable: restart pill is fixed-width secondary, label whole every frame");
  return 0;
}
