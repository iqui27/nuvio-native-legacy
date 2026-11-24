// Render resolution modes (src/assperf.c, owner 22/11/2026): Low / Medium /
// High auto, 100 % in quiet scenes (Custom auto removed 23/11/2026). While busy: text 50 / 75 /
// 90 %, blur the clean value (no lines on strip gradients) nearest above
// 40 / 50 / 55 % up to 75 %, worked out from the frame height (layout x k, k =
// 2 on a 4K UI) and the script's PlayResY. Scripts of 540 lines or fewer:
// text 100 %, blur the highest clean value up to 100 %.
#include "assperf.h"
#include <math.h>
#include <stdio.h>

static int falhas;
#define OK(c, m) do { if (c) printf("ok   %s\n", m); else { printf("FAIL %s\n", m); falhas++; } } while (0)
#define PERTO(a, b) (fabs((a) - (b)) < 0.0005)

static int quadro(double *tx, double *bl) { return assperf_resolucao_quadro(tx, bl); }
static void modo(int v, double *tx, double *bl, int *d) {
  assperf_teste_definir(AP_ESCALA, v);
  *d = quadro(tx, bl);
}

int main(void) {
  double tx, bl; int d;
  assperf_grade_definir(1080, 1080);
  modo(AP_RES_BAIXO, &tx, &bl, &d);
  OK(!d && tx == 1.0 && bl == 0.0, "Low auto, quiet scene: one pass at 100 %");
  OK(!assperf_reduzido() && assperf_escala_blur() == 0.0, "quiet: not reduced");
  assperf_previsao(400, 0, 50, 400, 0);   /* forecast: a busy scene */

  /* 1080 UI (k = 1), 1080p script: clean f = p. */
  modo(AP_RES_BAIXO, &tx, &bl, &d);
  OK(!d && tx == 0.5 && bl == 0.0, "Low, 1080 UI, 1080p: 50 is clean (f 1/2): one pass 50/50, no split (log 1, 22/11)");
  OK(assperf_reduzido() && assperf_escala_blur() == 0.0 && assperf_escala() == 0.5, "busy: reduced, one pass logged");
  modo(AP_RES_MEDIO, &tx, &bl, &d);
  OK(d && tx == 0.75 && PERTO(bl, 0.50), "Medium, 1080 UI, 1080p: text 75, blur 50");
  modo(AP_RES_ALTO, &tx, &bl, &d);
  OK(d && tx == 1.0 && PERTO(bl, 0.50), "High, 1080 UI, 1080p: text 100, blur 50");

  /* 4K UI (k = 2): frame 2160 px. */
  assperf_grade_definir(2160, 1080);
  modo(AP_RES_BAIXO, &tx, &bl, &d);
  OK(!d && tx == 0.5 && bl == 0.0, "Low, 4K UI, 1080p: blur 50 (f 1; 40 would be f 0.8) = text 50: one pass");
  modo(AP_RES_MEDIO, &tx, &bl, &d);
  OK(d && PERTO(bl, 0.50), "Medium, 4K UI, 1080p: blur 50");
  modo(AP_RES_ALTO, &tx, &bl, &d);
  OK(d && PERTO(bl, 0.50), "High, 4K UI, 1080p: blur 50");
  assperf_grade_definir(2160, 720);
  modo(AP_RES_BAIXO, &tx, &bl, &d);
  OK(d && tx == 0.5 && PERTO(bl, 2.0 / 3.0), "Low, 4K UI, 720p: blur 66.7 (f 2)");
  assperf_grade_definir(1080, 720);
  modo(AP_RES_MEDIO, &tx, &bl, &d);
  OK(d && PERTO(bl, 2.0 / 3.0), "Medium, 1080 UI, 720p: blur 66.7 (f 1)");

  /* 480p: text 100, blur the highest clean value up to 100. */
  assperf_grade_definir(2160, 480);
  modo(AP_RES_BAIXO, &tx, &bl, &d);
  OK(d && tx == 1.0 && PERTO(bl, 480.0 * 4.0 / 2160.0), "Low, 4K UI, 480p: text 100, blur 88.9 (f 4)");
  assperf_grade_definir(1080, 480);
  modo(AP_RES_ALTO, &tx, &bl, &d);
  OK(d && tx == 1.0 && PERTO(bl, 480.0 * 2.0 / 1080.0), "High, 1080 UI, 480p: text 100, blur 88.9 (f 2)");
  /* Saved files. */
  OK(assperf_teste_lido(AP_ESCALA, 0) == AP_RES_ALTO, "a saved fixed 100 % reads as High auto");
  OK(assperf_teste_lido(AP_ESCALA, 2) == AP_RES_MEDIO, "a saved fixed 75 % reads as Medium auto");
  OK(assperf_teste_lido(AP_ESCALA, AP_RES_CUSTOM_VELHO) == -1, "a saved Custom auto (6, removed) is refused: the TV's default stays");
  OK(assperf_teste_lido(AP_ESCALA, AP_RES_BAIXO) == AP_RES_BAIXO && assperf_teste_lido(AP_ESCALA, AP_RES_MEDIO) == AP_RES_MEDIO &&
     assperf_teste_lido(AP_ESCALA, AP_RES_ALTO) == AP_RES_ALTO, "the Autos read back as saved");
  OK(assperf_teste_lido(AP_ESCALA, 4) == -1 && assperf_teste_lido(AP_ESCALA, 8) == -1, "unknown modes refused (default kept)");
  /* Default mode by the TV (owner 23/11/2026), nothing saved (or a saved Custom): 7+ High, 6 Medium, 4/5 Low, unknown Medium. */
  assperf_tizen_definir(9); OK(assperf_get(AP_ESCALA) == AP_RES_ALTO, "Tizen 9, nothing saved: High auto");
  assperf_tizen_definir(6); OK(assperf_get(AP_ESCALA) == AP_RES_MEDIO, "Tizen 6, nothing saved: Medium auto");
  assperf_tizen_definir(5); OK(assperf_get(AP_ESCALA) == AP_RES_BAIXO, "Tizen 5, nothing saved: Low auto");
  assperf_tizen_definir(4); OK(assperf_get(AP_ESCALA) == AP_RES_BAIXO, "Tizen 4, nothing saved: Low auto");
  assperf_tizen_definir(0); OK(assperf_get(AP_ESCALA) == AP_RES_MEDIO, "Tizen unknown: Medium auto");
  printf(falhas ? "assperf_resolucao: %d FAIL\n" : "assperf_resolucao: ok\n", falhas);
  return falhas != 0;
}
