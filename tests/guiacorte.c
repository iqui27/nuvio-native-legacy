// Aviso de corte do guia: canais que a fonte tem e a TV nao guarda.
// Alimenta o contador com 3000 canais em 60 categorias, como o Xtream faz.
#include "guiacorte.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)

int main(void) {
  GuiaCorte k; int n = 0, i;
  memset(&k, 0, sizeof k);
  // 3000 canais, 60 categorias de 50 (o teto de categorias nao pesa aqui).
  for (i = 0; i < 3000; i++) {
    int catOk = (i % 60) < GUIACORTE_MAX_CAT;
    if (guiacorte_entra(&k, n, catOk)) n++;
  }
  printf("[canais] n=%d de %d (corte: %s)\n", n, guiacorte_total(&k, n), guiacorte_motivo(&k));
#ifdef NV_ANDROID
  CHECA(GUIACORTE_MAX_CANAL >= 3000);
  CHECA(n == 3000 && !guiacorte_cortou(&k));
#else
  CHECA(GUIACORTE_MAX_CANAL == 900);
  CHECA(n == 900 && guiacorte_total(&k, n) == 3000);
  CHECA(guiacorte_cortou(&k) && guiacorte_por_canais(&k));   // 60 categorias passam das 48 da TV: os dois motivos
  { char b[96];
    guiacorte_linha(&k, n, "Mostrando %d de %d canais", b, sizeof b);
    CHECA(!strcmp(b, "Mostrando 900 de 3000 canais")); }
#endif
  // Prova do "so Esportes": servidor manda por categoria (5 x 600). Com o teto
  // de 900 as categorias 3 a 5 somem INTEIRAS; na TV de 3000 todas entram.
  memset(&k, 0, sizeof k); n = 0;
  { int vistas[5] = {0}, c;
    for (i = 0; i < 3000; i++) if (guiacorte_entra(&k, n, 1)) { vistas[i / 600]++; n++; }
    c = (vistas[0] > 0) + (vistas[1] > 0) + (vistas[2] > 0) + (vistas[3] > 0) + (vistas[4] > 0);
#ifdef NV_ANDROID
    CHECA(c == 5);
#else
    CHECA(c == 2 && vistas[2] == 0 && vistas[4] == 0);
#endif
  }
  // Categoria fora do teto: os canais dela contam como perdidos por categoria.
  memset(&k, 0, sizeof k); n = 0;
  for (i = 0; i < 100; i++) if (guiacorte_entra(&k, n, i < 80)) n++;
  CHECA(n == 80 && guiacorte_total(&k, n) == 100 && guiacorte_por_cats(&k) && !guiacorte_por_canais(&k));
  // Nada perdido: sem aviso.
  memset(&k, 0, sizeof k); n = 0;
  for (i = 0; i < 10; i++) if (guiacorte_entra(&k, n, 1)) n++;
  CHECA(!guiacorte_cortou(&k) && !strcmp(guiacorte_motivo(&k), "nenhum"));
  printf(falhas ? "%d falha(s)\n" : "guiacorte: ok\n", falhas);
  return falhas ? 1 : 0;
}
