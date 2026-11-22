// Regra do peso da legenda ASS (src/asspeso.c): quando desce a resolucao do
// render, quando volta, e que so o modo Automatico mexe.
#include "../src/asspeso.h"
#include <assert.h>
#include <stdio.h>

static int janelas(AssPeso *p, int modo, int n, double ms, size_t bytes) {
  int i, mudou = 0;
  for (i = 0; i < n * ASSPESO_JANELA; i++) mudou += asspeso_amostra(p, modo, ms, bytes, 0);
  return mudou;
}

int main(void) {
  AssPeso p;
  asspeso_zerar(&p);
  assert(asspeso_divisor(&p) == 1);

  // Dialogo comum: nunca desce.
  assert(janelas(&p, ASSPESO_AUTO, 50, 3.0, 200 << 10) == 0 && p.nivel == 0);

  // Uma janela ruim sozinha (pico) nao desce; duas seguidas descem.
  assert(janelas(&p, ASSPESO_AUTO, 1, 80.0, 1 << 20) == 0 && p.nivel == 0);
  assert(janelas(&p, ASSPESO_AUTO, 1, 3.0, 1 << 20) == 0 && p.nivel == 0);
  assert(janelas(&p, ASSPESO_AUTO, 2, 80.0, 1 << 20) == 1 && p.nivel == 1);
  assert(asspeso_divisor(&p) == 2);

  // Ainda pesado na metade: vai a um terco e para ali.
  assert(janelas(&p, ASSPESO_AUTO, 2, 60.0, 1 << 20) == 1 && p.nivel == 2);
  assert(asspeso_divisor(&p) == 3);
  assert(janelas(&p, ASSPESO_AUTO, 20, 60.0, 1 << 20) == 0 && p.nivel == ASSPESO_NIVEL_MAX);

  // Rapido, mas nao o bastante para aguentar 2,25x a area: fica.
  assert(janelas(&p, ASSPESO_AUTO, 30, 15.0, 1 << 20) == 0 && p.nivel == 2);
  // A cena pesada passou: volta um degrau por vez, so depois de ASSPESO_BOAS.
  assert(janelas(&p, ASSPESO_AUTO, ASSPESO_BOAS - 1, 1.0, 50 << 10) == 0 && p.nivel == 2);
  assert(janelas(&p, ASSPESO_AUTO, 1, 1.0, 50 << 10) == 1 && p.nivel == 1);
  assert(janelas(&p, ASSPESO_AUTO, ASSPESO_BOAS, 1.0, 50 << 10) == 1 && p.nivel == 0);
  assert(janelas(&p, ASSPESO_AUTO, 40, 1.0, 50 << 10) == 0 && p.nivel == 0);

  // Uma janela ruim no meio zera a contagem de boas.
  janelas(&p, ASSPESO_AUTO, 2, 80.0, 0);
  assert(p.nivel == 1);
  janelas(&p, ASSPESO_AUTO, ASSPESO_BOAS - 1, 1.0, 0);
  janelas(&p, ASSPESO_AUTO, 1, 20.0, 0);            // nao e ruim, mas nao cabe 4x
  assert(janelas(&p, ASSPESO_AUTO, ASSPESO_BOAS - 1, 1.0, 0) == 0 && p.nivel == 1);
  assert(janelas(&p, ASSPESO_AUTO, 1, 1.0, 0) == 1 && p.nivel == 0);

  // Bytes por quadro tambem contam: rapido de renderizar, caro de enviar.
  assert(janelas(&p, ASSPESO_AUTO, 2, 5.0, 8u << 20) == 1 && p.nivel == 1);

  // Estouro do teto de copia (o quadro nem aparece): desce na hora.
  asspeso_zerar(&p);
  assert(asspeso_amostra(&p, ASSPESO_AUTO, 10.0, 0, 1) == 1 && p.nivel == 1);
  assert(asspeso_amostra(&p, ASSPESO_AUTO, 10.0, 0, 1) == 1 && p.nivel == 2);
  assert(asspeso_amostra(&p, ASSPESO_AUTO, 10.0, 0, 1) == 0 && p.nivel == 2);

  // "Sempre completo" e "Texto simples": o render nunca e reduzido, e quem
  // estava reduzido volta na primeira amostra.
  assert(asspeso_amostra(&p, ASSPESO_COMPLETO, 500.0, 16u << 20, 1) == 1 && p.nivel == 0);
  assert(janelas(&p, ASSPESO_COMPLETO, 30, 500.0, 16u << 20) == 0 && p.nivel == 0);
  assert(janelas(&p, ASSPESO_SIMPLES, 30, 500.0, 16u << 20) == 0 && p.nivel == 0);
  // O que sobrou das janelas fora do Automatico nao conta depois.
  assert(janelas(&p, ASSPESO_AUTO, 1, 80.0, 0) == 0 && p.nivel == 0);

  assert(!asspeso_sem_estilo(ASSPESO_AUTO) && !asspeso_sem_estilo(ASSPESO_COMPLETO));
  assert(asspeso_sem_estilo(ASSPESO_SIMPLES));
  puts("asspeso: ok");
  return 0;
}
