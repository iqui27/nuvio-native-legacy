// Passo do CH+/CH- da legenda (legsyncpasso.h): toques e tecla segurada
// simulados como o Smart Remote entrega (KEYDOWNs separados, repeat=0).
#include <stdio.h>
#include "../src/legsyncpasso.h"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

// Segura a tecla por `ms`: primeiro KEYDOWN em t0, a primeira repeticao depois
// de `atraso` e as seguintes a cada `rep`. Devolve o total andado (ms).
static int segurar(LegSyncPasso *s, int dir, unsigned t0, unsigned ms, unsigned atraso, unsigned rep,
                   int *ultPasso) {
  int tot = legsyncpasso_tecla(s, dir, t0, 0), p = 0;
  unsigned t;
  for (t = t0 + atraso; t <= t0 + ms; t += rep) { p = legsyncpasso_tecla(s, dir, t, 0); tot += p; }
  if (ultPasso) *ultPasso = p;
  return tot;
}

int main(void) {
  LegSyncPasso s = {0};
  int i, tot, ult;

  // Toques deliberados (>= 250 ms entre eles): sempre 0,1 s.
  for (i = 0; i < 8; i++) CHECK(legsyncpasso_tecla(&s, 1, 1000u + i * 250u, 0) == 100, "toque %d", i);
  // Toques rapidos mas humanos (200 ms): continuam 0,1 s.
  s = (LegSyncPasso){0};
  for (i = 0; i < 6; i++) CHECK(legsyncpasso_tecla(&s, -1, 5000u + i * 200u, 0) == 100, "toque rapido %d", i);

  // Segurar 2 s no Smart Remote (atraso inicial 400 ms, repeticao 110 ms):
  // comeca em 0,1 s e acelera ate 0,5 s por repeticao.
  s = (LegSyncPasso){0};
  tot = segurar(&s, 1, 10000u, 2000u, 400u, 110u, &ult);
  CHECK(ult == 500, "segurando 2 s o passo chega a 0,5 s (foi %d)", ult);
  CHECK(tot % 100 == 0, "total na grade de 0,1 s (%d)", tot);
  CHECK(tot > 3000 && tot < 9000, "segurar 2 s anda alguns segundos (%d ms)", tot);
  printf("segurar 2 s: %d ms\n", tot);

  // Segurar pouco (meio segundo depois da 1a repeticao) ainda e suave.
  s = (LegSyncPasso){0};
  tot = segurar(&s, 1, 20000u, 650u, 400u, 110u, &ult);
  CHECK(ult <= 200 && tot <= 600, "segurar curto e suave (passo %d, total %d)", ult, tot);

  // Soltar e tocar de novo: volta a 0,1 s.
  CHECK(legsyncpasso_tecla(&s, 1, 20000u + 650u + 600u, 0) == 100, "toque depois de soltar");

  // Trocar de direcao no meio da rajada recomeca a rampa.
  s = (LegSyncPasso){0};
  segurar(&s, 1, 30000u, 1500u, 400u, 110u, &ult);
  CHECK(ult == 500, "rampa cheia antes de virar");
  CHECK(legsyncpasso_tecla(&s, -1, 31560u, 0) == 100, "virar de direcao volta a 0,1 s");

  // A repeticao do proprio SDL (teclado) conta como segurar mesmo com o
  // intervalo longo.
  s = (LegSyncPasso){0};
  legsyncpasso_tecla(&s, 1, 40000u, 0);
  legsyncpasso_tecla(&s, 1, 40500u, 1);
  CHECK(legsyncpasso_tecla(&s, 1, 41000u, 1) == 200, "repeat do SDL acelera");

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  puts("legsyncpasso: toque 0,1 s, segurar acelera ate 0,5 s, virar recomeca");
  return 0;
}
