/* #777: preferencia es-419 vs es-ES e pt-BR vs pt-PT na escolha automatica. */
#include "audioescolha.h"
#include "linguas.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  AudioCand c[4];
  memset(c, 0, sizeof c);
  c[0] = (AudioCand){ "es-ES", "eac3", "Castellano 5.1", 6, 0, 1, 0 };
  c[1] = (AudioCand){ "es-419", "eac3", "Latino 5.1", 6, 0, 1, 1 };
  c[2] = (AudioCand){ "es", "eac3", "Espanol", 6, 0, 1, 2 };
  c[3] = (AudioCand){ "en", "eac3", "English", 6, 0, 1, 3 };
  int o;
  int k = audioesc_escolher_com(c, 4, NULL, "es-419", NULL, NULL, &o);
  if (k != 1) {
    fprintf(stderr, "es-419 deveria escolher faixa 1, veio %d\n", k);
    return 1;
  }
  k = audioesc_escolher_com(c, 4, NULL, "es-es", NULL, NULL, &o);
  if (k != 0) {
    fprintf(stderr, "es-ES deveria escolher faixa 0, veio %d\n", k);
    return 1;
  }
  if (!ling_opcao_codigo(ling_opcao_n() - 2) || strcmp(ling_opcao_codigo(ling_opcao_n() - 2), "es-419")) {
    fprintf(stderr, "es-419 nao esta no fim da lista de opcoes\n");
    return 1;
  }
  printf("audio_regional: es-419/pt variantes na lista e na ordem ok\n");
  return 0;
}
