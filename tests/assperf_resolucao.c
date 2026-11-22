// Render resolution modes (src/assperf.c, owner 19/11/2026): Auto 50 % = text
// 50 / blur 40 and Auto 75 % = text 75 / blur 55 while busy, 100 % quiet;
// "Custom auto" is gone (a saved one reads as Auto 75 %).
#include "assperf.h"
#include <stdio.h>

static int falhas;
#define OK(c, m) do { if (c) printf("ok   %s\n", m); else { printf("FAIL %s\n", m); falhas++; } } while (0)

static int quadro(double *tx, double *bl) { return assperf_resolucao_quadro(tx, bl); }

int main(void) {
  double tx, bl; int d;
  assperf_teste_definir(AP_ESCALA, AP_RES_AUTO75);
  d = quadro(&tx, &bl);
  OK(!d && tx == 1.0 && bl == 0.0, "auto 75, quiet scene: one pass at 100 %");
  OK(!assperf_reduzido() && assperf_escala_blur() == 0.0, "quiet: not reduced");
  assperf_previsao(400, 0, 50, 400, 0);   /* forecast: a busy scene */
  d = quadro(&tx, &bl);
  OK(d && tx == 0.75 && bl == 0.55, "auto 75, busy: text 75 %, blur 55 %");
  OK(assperf_reduzido() && assperf_escala_blur() == 0.55 && assperf_escala() == 0.75, "busy: reduced, scales logged");
  assperf_teste_definir(AP_ESCALA, AP_RES_AUTO50);
  d = quadro(&tx, &bl);
  OK(d && tx == 0.5 && bl == 0.4, "auto 50, busy: text 50 %, blur 40 %");
  OK(assperf_escala() == 0.5 && assperf_escala_blur() == 0.4, "auto 50: scales logged");
  assperf_teste_definir(AP_ESCALA, AP_RES_100);
  d = quadro(&tx, &bl);
  OK(!d && tx == 1.0 && !assperf_reduzido() && assperf_escala_blur() == 0.0, "fixed 100 %");
  assperf_teste_definir(AP_ESCALA, AP_RES_75);
  d = quadro(&tx, &bl);
  OK(!d && tx == 0.75 && !assperf_reduzido(), "fixed 75 %: one pass, not an Auto reduction");
  /* Saved files. */
  OK(assperf_teste_lido(AP_ESCALA, 6) == AP_RES_AUTO75, "a saved Custom auto (6) reads as Auto 75 %");
  OK(assperf_teste_lido(AP_ESCALA, AP_RES_AUTO50) == AP_RES_AUTO50 && assperf_teste_lido(AP_ESCALA, AP_RES_AUTO75) == AP_RES_AUTO75 &&
     assperf_teste_lido(AP_ESCALA, AP_RES_100) == AP_RES_100 && assperf_teste_lido(AP_ESCALA, AP_RES_75) == AP_RES_75,
     "the four modes read back as saved");
  OK(assperf_teste_lido(AP_ESCALA, 4) == -1 && assperf_teste_lido(AP_ESCALA, 7) == -1, "unknown modes refused (default kept)");
  OK(!AP_RES_AUTO(6), "6 is no mode any more");
  printf(falhas ? "assperf_resolucao: %d FAIL\n" : "assperf_resolucao: ok\n", falhas);
  return falhas != 0;
}
