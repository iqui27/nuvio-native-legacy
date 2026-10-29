#include "redefit.h"
#include <stdio.h>
static int falhas;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); falhas++; } } while (0)
// Ten 1 s ticks at `mbps` with the loader active `carga` of them; 1 = closed.
static int janela(RedeFit *r, long long *bytes, double mbps, int carga, int kbps, double *saida) {
  int i, fechou = 0;
  for (i = 0; i < REDEFIT_JANELA_S; i++) {
    int ativo = i < carga;
    if (ativo) *bytes += (long long)(mbps * 1e6 / 8);
    fechou |= redefit_segundo(r, *bytes, ativo, kbps, saida);
  }
  return fechou;
}
int main(void) {
  double m = 0;
  long long b = 0;
  // 52.8 Mbps remux, loader always on, server delivers 40 Mbps: flag after
  // REDEFIT_BAIXAS consecutive short intervals, not before.
  { RedeFit r = {0}; int k;
    redefit_segundo(&r, b, 1, 52800, &m);   // first sample only arms the counter
    for (k = 1; k < REDEFIT_BAIXAS; k++) { CHECK(janela(&r, &b, 40, 10, 52800, &m)); CHECK(!r.sinal); }
    CHECK(m > 39 && m < 41);
    CHECK(janela(&r, &b, 40, 10, 52800, &m));
    CHECK(r.sinal == REDEFIT_VAZAO); }
  // Fast server whose loader pauses because the buffer is full: low average
  // bytes per window is not a deficit.
  { RedeFit r = {0}; int k;
    redefit_segundo(&r, b, 1, 52800, &m);
    for (k = 0; k < 6; k++) janela(&r, &b, 200, 2, 52800, &m);
    CHECK(!r.sinal); }
  // Short intervals broken by a good one do not accumulate.
  { RedeFit r = {0};
    redefit_segundo(&r, b, 1, 52800, &m);
    janela(&r, &b, 40, 10, 52800, &m); janela(&r, &b, 40, 10, 52800, &m);
    janela(&r, &b, 90, 10, 52800, &m);
    janela(&r, &b, 40, 10, 52800, &m); janela(&r, &b, 40, 10, 52800, &m);
    CHECK(!r.sinal); }
  // Unknown declared bitrate or unknown byte counter (HLS): never flags.
  { RedeFit r = {0}; int k;
    for (k = 0; k < 6; k++) janela(&r, &b, 1, 10, 0, &m);
    CHECK(!r.sinal);
    for (k = 0; k < 60; k++) redefit_segundo(&r, -1, 1, 52800, &m);
    CHECK(!r.sinal && m < 0); }
  // Rebuffers: two within 60 s flag; two spread out do not.
  { RedeFit r = {0};
    redefit_rebuffer(&r, 1000); redefit_rebuffer(&r, 70000);
    CHECK(!r.sinal && r.rebufs == 2);
    redefit_rebuffer(&r, 100000);
    CHECK(r.sinal == REDEFIT_REBUFFER && r.rebufs == 3); }
  if (!falhas) printf("redefit: ok\n");
  return falhas != 0;
}
