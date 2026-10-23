// Intervalo REAL entre swaps (src/swapint.h): mede, nao assume 16,7 ms.
#include <stdio.h>
#include <math.h>
#include "../src/swapint.h"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

static void roda(SwapInt *s, double ms, int n, double *t) {
  while (n-- > 0) { *t += ms; swapint_marca(s, *t); }
}

int main(void) {
  SwapInt s = {0}; double t = 1000;
  CHECK(fabs(swapint_periodo(&s) - 16.7) < 0.1, "sem amostra: padrao 16.7, deu %.2f", swapint_periodo(&s));
  roda(&s, 33.3, 30, &t);   /* TV travada em 30 Hz */
  printf("30 Hz: periodo=%.2f limite-jank=%.2f\n", swapint_periodo(&s), swapint_limite_jank(&s));
  CHECK(fabs(swapint_periodo(&s) - 33.3) < 0.5, "30 Hz medido, deu %.2f", swapint_periodo(&s));
  CHECK(swapint_limite_jank(&s) > 55.0, "limite de jank em 30 Hz deve seguir o periodo, deu %.2f", swapint_limite_jank(&s));
  t += 800; swapint_marca(&s, t);   /* suspensao: nao contamina */
  CHECK(fabs(swapint_periodo(&s) - 33.3) < 0.5, "quadro de 800 ms nao entra no periodo, deu %.2f", swapint_periodo(&s));
  SwapInt a = {0}; t = 5000;
  roda(&a, 16.67, 60, &t);
  printf("60 Hz: periodo=%.2f limite-jank=%.2f\n", swapint_periodo(&a), swapint_limite_jank(&a));
  CHECK(swapint_limite_jank(&a) >= 33.0 && swapint_limite_jank(&a) < 34.5, "60 Hz mantem ~33, deu %.2f", swapint_limite_jank(&a));
  double med, pior; int n = swapint_colher(&a, &med, &pior);
  CHECK(n == 59 && fabs(med - 16.67) < 0.05 && pior < 16.8, "janela: n=%d med=%.2f pior=%.2f", n, med, pior);
  CHECK(swapint_colher(&a, &med, &pior) == 0, "janela zera depois de colhida");
  /* 60 Hz com engasgo de 100 ms alternado: periodo nao sobe, jank continua < 100 ms */
  SwapInt j = {0}; t = 9000;
  roda(&j, 16.67, 60, &t);
  for (int i = 0; i < 30; i++) { roda(&j, 100, 1, &t); roda(&j, 16.67, 1, &t); }
  CHECK(swapint_limite_jank(&j) < 40.0, "engasgos contaminaram o periodo: limite %.2f", swapint_limite_jank(&j));
  /* troca real de cadencia 60 -> 30 Hz ainda e seguida */
  roda(&j, 33.3, 40, &t);
  CHECK(swapint_limite_jank(&j) > 55.0, "troca para 30 Hz nao seguida: limite %.2f", swapint_limite_jank(&j));
  return falhas ? 1 : 0;
}
