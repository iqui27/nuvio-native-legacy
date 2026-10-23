// FILME ASSISTIDO MOSTRA "REPRODUZIR", NAO "RETOMAR" (relato da Shield TV Pro
// 2019, teste 318.3: "todo filme assistido mostra Resume em vez de Play").
// O player ja nao retoma do Percentual assistido em diante (player.c,
// retomarPct: progresso < ajustes_cw_concluido) e o botao "do comeco"
// (temInicio) usa o mesmo corte; so o rotulo lia `progresso > 0`, entao um
// filme a 95% ou 100% dizia "Retomar" e comecava do zero.
#include "../src/detrotulo.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void conf(int prog, int concl, const char *esperado) {
  const char *r = det_rotulo_primario(prog, concl);
  if (strcmp(r, esperado)) {
    fprintf(stderr, "progresso %d, concluido %d: \"%s\", esperado \"%s\"\n", prog, concl, r, esperado);
    falhas++;
  }
}

int main(void) {
  conf(0, 90, "Reproduzir");
  conf(1, 90, "Retomar");
  conf(40, 90, "Retomar");
  conf(89, 90, "Retomar");
  conf(90, 90, "Reproduzir");   // assistido: o player comeca do zero
  conf(95, 90, "Reproduzir");
  conf(100, 90, "Reproduzir");
  conf(92, 95, "Retomar");      // o corte e o do ajuste, nao 90 fixo
  if (falhas) return 1;
  puts("detrotulo: tudo ok");
  return 0;
}
