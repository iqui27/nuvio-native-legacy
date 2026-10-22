/* #203 (TCL, 2.0.3: 18.6 Mbps vs 50 in 1.7.4): a debrid link redirects to a
 * CDN on another origin. rede_pedir drops the caller headers there, Range
 * included, so every connection got 200 from byte 0 and the 3 extra
 * connections were never summed (only a 206 adds). Range must survive. */
#include "streamfitdiag.h"
#include "redemarca.h"
#include <assert.h>
#include <stdio.h>
static int nc(void *u) { (void)u; return 0; }
int main(int argc, char **argv) {
  char url[256], fim[512]; int k[48]; RedeVazao r;
  const char *cab[] = { "X-Private-Key: segredo", NULL };
  long long soma = 0; int n, i;
  assert(argc == 2);
  snprintf(url, sizeof url, "http://127.0.0.1:%s/xorigem", argv[1]);
  redemarca_observar(1, 1);
  StreamfitDiagControle ctl = { .rede = redemarca_atual(), .cancelado = nc };
  n = streamfitdiag_medir(url, cab, 3, 5L * 1024 * 1024, 100000000ULL, &ctl, k, 48, &r, fim, sizeof fim);
  for (i = 0; i < n; i++) soma += k[i];
  printf("conexoes=%d n=%d status=%d kbps_sum=%lld kbps_media=%lld bytes=%lld\n",
         STREAMFITDIAG_CONEXOES, n, r.status, soma, n ? soma / n : 0, r.bytes);
  assert(n >= 2);
  assert(r.status == 206);                 /* Range reached the CDN hop */
  if (STREAMFITDIAG_CONEXOES == 4) assert(soma > 2 * 320 * n);  /* extras summed */
  return 0;
}
