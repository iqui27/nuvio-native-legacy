/* Android speed test: 4 connections summed, 1 connection control. */
#include "streamfitdiag.h"
#include "redemarca.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int nc(void *u) { (void)u; return 0; }
int main(int argc, char **argv) {
  char url[256], fim[512]; int k[48]; RedeVazao r;
  assert(argc == 2);
  snprintf(url, sizeof url, "http://127.0.0.1:%s/unknown", argv[1]);
  redemarca_observar(1, 1);
  StreamfitDiagControle ctl = { .rede = redemarca_atual(), .cancelado = nc };
  int n = streamfitdiag_medir(url, NULL, 3, 1000, 100000000ULL, &ctl, k, 48, &r, fim, sizeof fim);
  long long soma = 0; for (int i = 0; i < n; i++) soma += k[i];
  printf("conexoes=%d n=%d kbps_sum=%lld bytes=%lld\n", STREAMFITDIAG_CONEXOES, n, soma, r.bytes);
  assert(n >= 2 && r.status == 206);
  /* one connection streams ~40 KB/s (~320 kbps): 4 must sum to well above 2x */
  if (STREAMFITDIAG_CONEXOES == 4) assert(soma > 2 * 320 * n);
  else assert(soma < 2 * 400 * n);
  return 0;
}
