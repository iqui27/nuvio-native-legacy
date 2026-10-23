// OK afundado: o card cede no KEYDOWN e volta no KEYUP (src/pressdip.h).
#include <stdio.h>
#include <math.h>
#include "../src/pressdip.h"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

int main(void) {
  float e = 1.0f;
  int q;
  for (q = 0; q < 6; q++) e = pressdip_passo(e, 1, 1.0f / 60);   // 100 ms apertado
  printf("dip em 100 ms: %.3f (alvo %.3f)\n", e, PRESSDIP_ESCALA);
  CHECK(e < 0.94f, "100 ms apertado deveria ter afundado (< 0.94), deu %.3f", e);
  CHECK(e >= PRESSDIP_ESCALA - 0.001f, "nunca abaixo do alvo, deu %.3f", e);
  for (q = 0; q < 120; q++) e = pressdip_passo(e, 1, 1.0f / 60);   // segurado 2 s: nao afunda mais
  CHECK(fabsf(e - PRESSDIP_ESCALA) < 0.002f, "segurado deve assentar no alvo, deu %.3f", e);
  for (q = 0; q < 6; q++) e = pressdip_passo(e, 0, 1.0f / 60);     // soltou: 100 ms
  printf("volta em 100 ms: %.3f\n", e);
  CHECK(e > 0.99f, "100 ms depois de soltar deveria estar em ~1, deu %.3f", e);
  /* animacoes reduzidas: sem mola, vai direto */
  anim_politica_reduzida = 1;
  CHECK(pressdip_passo(1.0f, 1, 1.0f / 60) == PRESSDIP_ESCALA, "reduzida vai direto ao alvo");
  anim_politica_reduzida = 0;
  return falhas ? 1 : 0;
}
