// Native trailer selection (tizen=0, no Trailer audio candidate) must print
// exactly what integracao/2.0.5 prints. trailer-nativo-igual.sh builds this
// against both trailerfonte.c versions and diffs the output.
#include "trailerfonte.h"
#include <stdio.h>
#include <string.h>

int ajustes_trailer_fonte(void) { return 0; }

int main(void) {
  static const char *u[] = { NULL, "" , "x" };
  int aj, a, ar, af, i, ir, y, yr, som, q;
  for (aj = 0; aj <= TRF_YOUTUBE; aj++) {
    for (q = 0; q <= TRF_YOUTUBE; q++)
      printf("d %d %d %d %d\n", aj, q, trailerfonte_depois(aj, 0, q), trailerfonte_depois_destaque(aj, 0, q));
    for (a = 0; a < 3; a++) for (ar = 0; ar < 2; ar++) for (af = 0; af < 2; af++)
    for (i = 0; i < 3; i++) for (ir = 0; ir < 2; ir++)
    for (y = 0; y < 3; y++) for (yr = 0; yr < 2; yr++) for (som = 0; som < 2; som++) {
      TrailerCandidatos c;
      const char *url; int qa, qc, qd, da, dc, dd;
      memset(&c, 0, sizeof c);
      c.apple = u[a]; c.appleRespondeu = ar; c.appleFalhou = af;
      c.imdb = u[i]; c.imdbRespondeu = ir;
      c.youtube = u[y]; c.youtubeRespondeu = yr;
      da = trailerfonte_escolher(aj, 0, &c, &url, &qa);
      dc = trailerfonte_escolher_cheia(aj, 0, som, &c, &url, &qc);
      dd = trailerfonte_escolher_destaque(aj, 0, &c, &url, &qd);
      printf("e %d %d%d%d%d%d%d%d%d %d%d %d%d %d%d\n", aj, a, ar, af, i, ir, y, yr, som, da, qa, dc, qc, dd, qd);
    }
  }
  return 0;
}
