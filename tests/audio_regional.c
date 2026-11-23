/* #777: preferencia es-419 vs es-ES e pt-BR vs pt-PT na escolha automatica. */
#include "audioescolha.h"
#include "linguas.h"
#include <stdio.h>
#include <string.h>

static int falhas;

static void espera(const char *rotulo, int veio, int quer) {
  if (veio != quer) {
    fprintf(stderr, "%s: esperava %d, veio %d\n", rotulo, quer, veio);
    falhas++;
  }
}

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

  /* Regressoes F48-r2: a variante regional so DESEMPATA dentro do idioma.
   * Sem a etiqueta exata, a mesma familia do idioma principal vence qualquer
   * preferencia secundaria; e preferencia sem variante nao reordena a lista. */
  { AudioCand r[2];
    memset(r, 0, sizeof r);
    r[0] = (AudioCand){ "eng", "aac", "English", 2, 0, 1, 0 };
    r[1] = (AudioCand){ "por", "aac", "Portugues", 2, 0, 1, 1 };
    espera("pt-br+en [eng,por]", audioesc_escolher_com(r, 2, NULL, "pt-br", "en", NULL, &o), 1);
    espera("pt-br+en [eng,por] origem", o == AUDIOESC_PREF1 ? 1 : -1, 1);
    r[1].idioma = "spa";
    espera("es-419+en [eng,spa]", audioesc_escolher_com(r, 2, NULL, "es-419", "en", NULL, &o), 1);
    espera("es-419+en [eng,spa] origem", o == AUDIOESC_PREF1 ? 1 : -1, 1);
    r[1].idioma = "es-ES";
    espera("es-419+en [eng,es-ES]", audioesc_escolher_com(r, 2, NULL, "es-419", "en", NULL, &o), 1);
    r[0].idioma = "eng"; r[1].idioma = "en";
    espera("en [eng,en]", audioesc_escolher_com(r, 2, NULL, "en", NULL, NULL, &o), 0);
    r[0].idioma = "pob"; r[1].idioma = "pt";
    espera("pt [pob,pt]", audioesc_escolher_com(r, 2, NULL, "pt", NULL, NULL, &o), 0);
    r[0].idioma = "spa"; r[1].idioma = "spa";
    espera("es-es so [spa]", audioesc_escolher_com(r, 2, NULL, "es-es", "en", NULL, &o), 0);
    espera("es-es so [spa] origem", o == AUDIOESC_PREF1 ? 1 : -1, 1);
  }
  /* Idem nas legendas: a afinidade so reordena quando a preferencia tem
   * variante — "en" com [eng,en] fica na primeira, na embutida, na
   * pre-busca e na escolha por idioma do addon. */
  { const char *embEn[] = { "eng", "en" };
    const int tipos[] = { LING_LEG_COMUM, LING_LEG_COMUM };
    espera("legenda embutida en [eng,en]",
           ling_legenda_auto_tipo2("en", "ja", 0, 1, embEn, tipos, 2, 1, NULL, 0, 1), 0);
    espera("legenda prebusca en [eng,en]", ling_legenda_auto("en", embEn, 2, 1, NULL, 0, 1), 0);
    espera("legenda addon en [eng,en]",
           ling_legenda_auto_tipo2("en", "ja", 0, 1, NULL, NULL, 0, 1, embEn, 2, 1), 0);
    const char *embPt[] = { "por", "pob" };
    espera("legenda prebusca pt [por,pob]", ling_legenda_auto("pt", embPt, 2, 1, NULL, 0, 1), 0);
    const char *embEs[] = { "spa" };
    espera("legenda embutida es-es so [spa]",
           ling_legenda_auto_tipo2("es-es", "ja", 0, 1, embEs, tipos, 1, 1, NULL, 0, 1), 0);
    const char *embLa[] = { "es-ES", "es-419" };
    espera("legenda embutida es-419 [es-ES,es-419]",
           ling_legenda_auto_tipo2("es-419", "ja", 0, 1, embLa, tipos, 2, 1, NULL, 0, 1), 1);
  }
  if (falhas) return 1;
  printf("audio_regional: es-419/es-es/pt e legendas embutidas ok\n");
  return 0;
}
