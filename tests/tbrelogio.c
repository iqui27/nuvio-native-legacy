// tbrelogio.h with log_18's numbers (Love Is War S03E02 burned at 149.3 s).
#include "tbrelogio.h"
#include <math.h>
#include <stdio.h>

static int falhas;
#define PERTO(a, b, msg) do { double _a = (a), _b = (b); if (fabs(_a - _b) > 0.0005) { printf("FAIL %s: %.4f != %.4f\n", msg, _a, _b); falhas++; } else printf("ok   %s\n", msg); } while (0)
#define VERDADE(c, msg) do { if (!(c)) { printf("FAIL %s\n", msg); falhas++; } else printf("ok   %s\n", msg); } while (0)

static double ptsReal(int k) { return 0.021 + 5.005 * k + (k & 1 ? 1.0 : 0.0); }

int main(void) {
  double d[300], a = -1;
  unsigned g, g2, gg;
  int i, seg = -1;
  for (i = 0; i < 300; i++) d[i] = 5.0;
  d[299] = 1.996;

  VERDADE(tbr_segmento_de(d, 300, 149.3, &a) == 29 && fabs(a - 145.0) < 1e-9, "149.3 s is segment 29 (playlist start 145)");
  VERDADE(tbr_segmento_de(d, 300, 0, NULL) == 0, "0 s is segment 0");
  VERDADE(tbr_segmento_de(d, 300, -3, NULL) == 0, "before the start: segment 0");
  VERDADE(tbr_segmento_de(d, 300, 99999, NULL) == 299, "past the end: the last segment");
  VERDADE(tbr_segmento_de(d, 0, 5, NULL) == -1, "no segments: -1");

  PERTO(tbr_correcao(), 0, "no burned stream: no correction");
  g = tbr_comecar();
  tbr_segmentos(g, d, 300);
  PERTO(tbr_correcao(), 0, "origin unknown: 0");
  VERDADE(tbr_falta(&gg, &seg) && gg == g && seg == 0, "first thing to read: segment 0");
  tbr_origem(g, 0.021);
  tbr_ancorar(0);
  PERTO(tbr_correcao(), 0, "opened at 0: 0");
  tbr_pts(g, 1, ptsReal(1));
  tbr_servido(g, 0);

  tbr_ancorar(149.3);
  PERTO(tbr_correcao(), 0, "landing segment's PTS not known yet: 0");
  VERDADE(tbr_falta(&gg, &seg) && seg == 29, "it asks for segment 29");
  tbr_servido(g, 29);
  tbr_pts(g, 29, 146.167);
  PERTO(tbr_correcao(), 1.146, "log_18: landed in 29 -> +1146 ms");
  PERTO(tbr_correcao(), 1.146, "Landing: fixed until the next seek");
  VERDADE(!tbr_falta(&gg, &seg), "nothing more to read in Landing");

  // The next segments' PTS arrive while playing on: the landing value stays
  // (the TV does not re-tie its clock at every segment; owner 16/11).
  for (i = 30; i < 40; i++) tbr_pts(g, i, ptsReal(i));
  PERTO(tbr_correcao(), 1.146, "playing into segment 30: still the landing value");
  PERTO(tbr_correcao(), 1.146, "playing into segment 31: still the landing value");

  tbr_ancorar(150.5);   // a seek into an even segment
  PERTO(tbr_correcao(), ptsReal(30) - 150.0 - 0.021, "seek to 150.5 s: segment 30 -> +150 ms");

  tbr_pts(g, 5000, 1.0);  tbr_pts(g, -1, 1.0);   // out of range: ignored
  g2 = tbr_comecar();     // a new burned stream (audio change, retry)
  tbr_pts(g, 0, 0.021);   // the old session's late answer
  PERTO(tbr_correcao(), 0, "new stream: nothing known yet, stale answers ignored");
  tbr_segmentos(g2, d, 300); tbr_origem(g2, 0.021); tbr_ancorar(1440.0);
  tbr_pts(g2, 288, ptsReal(288));
  PERTO(tbr_correcao(), ptsReal(288) - 1440.0 - 0.021, "near the end, even segment: about +1.44 s");
  tbr_pts(g2, 1, 50.0);   // nonsense for an unrelated segment: not used
  tbr_ancorar(5.0); tbr_pts(g2, 1, 30.0);
  PERTO(tbr_correcao(), 0, "a correction over 10 s is not believed");

  tbr_encerrar();
  PERTO(tbr_correcao(), 0, "burn-in Off: 0");
  VERDADE(!tbr_ativo() && !tbr_geracao(), "inactive after the end");
  printf(falhas ? "%d FAILED\n" : "tbrelogio ok\n", falhas);
  return falhas != 0;
}
