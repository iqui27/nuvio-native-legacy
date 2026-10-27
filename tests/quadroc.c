// Row 30 (mapa-erros-logs): o medidor de quadro contava o tempo SUSPENSO.
// `[quadro] pior=` saia com 4191 s, 881 s, 54 s e 30 s em seis registros, e a
// triagem passava a procurar jank que nunca existiu. O relogio tem de zerar
// quando o app suspende ou volta.
#include "quadroc.h"
#include <assert.h>
#include <stdio.h>

#define FREQ 1000000000.0   // 1 GHz: conta em ns, o mesmo papel do contador real

int main(void) {
  QuadroRelogio q = { 0, 0, 0, 0.0 };
  double dt;

  // Quadros normais: dt vem do relogio, como antes.
  dt = quadroc_quadro(&q, 1000.0 * FREQ / 1000.0, FREQ, 1000);
  assert(dt == 0.0);   // primeiro quadro: sem anterior
  dt = quadroc_quadro(&q, 1016.0 * FREQ / 1000.0, FREQ, 1016);
  assert(dt > 15.0 && dt < 17.0);

  // SUSPENDEU por 4191 s (o pior caso do mapa). Sem a zerada, o proximo quadro
  // devolveria 4191000 ms e viraria `pior=4191000.0ms`.
  quadroc_suspendeu(&q);
  dt = quadroc_quadro(&q, 4191000.0 * FREQ / 1000.0, FREQ, 4191000);
  assert(dt == 0.0);   // nao mede: a suspensao nao e quadro

  // O quadro SEGUINTE volta a medir normal (16 ms), sem carregar a suspensao.
  dt = quadroc_quadro(&q, 4191000.0 * FREQ / 1000.0 + 16 * FREQ / 1000.0, FREQ, 4191016);
  assert(dt > 15.0 && dt < 17.0);

  // A marca vale por UM quadro so.
  quadroc_suspendeu(&q);
  dt = quadroc_quadro(&q, 5000.0 * FREQ / 1000.0, FREQ, 5000);
  assert(dt == 0.0);
  dt = quadroc_quadro(&q, 5016.0 * FREQ / 1000.0, FREQ, 5016);
  assert(dt > 15.0 && dt < 17.0);

  // Sem marca, nada muda: 30 s de quadro TRAVADO de verdade continua aparecendo
  // (nao e suspensao, e jank).
  dt = quadroc_quadro(&q, 3016.0 * FREQ / 1000.0, FREQ, 3016);
  assert(dt > 30000.0);

  puts("quadroc: PASS");
  return 0;
}
