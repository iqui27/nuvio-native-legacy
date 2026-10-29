// O VIDRO FOSCO SOBRE VIDEO COM FAIXA PRETA (S90D, filme 2.39:1): a ilha do
// relogio fica na faixa de cima, fora do furo, e desfoca o que esta atras. Com
// o video tocando nada muda ali, mas a captura (uma copia + 5 passadas) saia
// todo quadro. Imprime, a cada quadro, capturas e passadas do anterior.
#include "gfx.h"
#include "tex_cache.h"
#include <stdio.h>
static void perf_quadro(void) {
  printf("Q cap=%d pas=%d\n", gfx_vidro_capturas, gfx_vidro_passadas);
  gfx_novo_quadro();
}
#define tex_novo_quadro() (perf_quadro(), tex_novo_quadro())
#include "player_glass_shot.c"
