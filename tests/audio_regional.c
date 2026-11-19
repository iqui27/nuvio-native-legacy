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
  AudioCand dts[2];
  memset(dts, 0, sizeof dts);
  dts[0] = (AudioCand){ "es-ES", "dts", "Castellano DTS", 6, 1, 1, 0 };
  dts[1] = (AudioCand){ "es-419", "dts", "Latino DTS", 6, 1, 1, 1 };
  k = audioesc_escolher_com(dts, 2, NULL, "es-419", NULL, NULL, &o);
  if (k != 1) {
    fprintf(stderr, "es-419 com DTS deveria escolher faixa 1, veio %d\n", k);
    return 1;
  }
  AudioCand pt[2];
  memset(pt, 0, sizeof pt);
  pt[0] = (AudioCand){ "por", "eac3", "Portugues PT", 6, 0, 1, 0 };
  pt[1] = (AudioCand){ "pob", "eac3", "Portugues BR", 6, 0, 1, 1 };
  k = audioesc_escolher_com(pt, 2, NULL, "pt", NULL, NULL, &o);
  if (k != 0) {
    fprintf(stderr, "pt generico deveria manter a primeira faixa, veio %d\n", k);
    return 1;
  }
  const char *embEs[] = { "es-ES", "es-419" };
  if (ling_legenda_auto("es-419", embEs, 2, 1, NULL, 0, 1) != 1) {
    fprintf(stderr, "legenda embutida es-419 deveria escolher faixa 1\n");
    return 1;
  }
  const char *embPt[] = { "pt-BR", "pt-PT" };
  if (ling_legenda_auto("pt-pt", embPt, 2, 1, NULL, 0, 1) != 1) {
    fprintf(stderr, "legenda embutida pt-pt deveria escolher faixa 1\n");
    return 1;
  }
  if (!ling_opcao_codigo(ling_opcao_n() - 1) || strcmp(ling_opcao_codigo(ling_opcao_n() - 1), "es-es")) {
    fprintf(stderr, "es-es nao esta nas opcoes regionais\n");
    return 1;
  }
  printf("audio_regional: es-419/es-es/pt e legendas embutidas ok\n");
  return 0;
}
