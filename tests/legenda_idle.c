/* Regression guard, not a reproduction of webOS issue #484: exercise the
 * public overlay and OLED-idle APIs without player internals or real time. */
#include "legenda.h"
#include "esmaecer.h"
#include <assert.h>
const char *i18n(const char *text) { return text; }
#include <stdio.h>
#include <string.h>

char *rede_baixar(const char *url, int segundos) {
  (void)url; (void)segundos; return NULL;
}
char *rede_baixar_bin(const char *url, int segundos, long *n) {
  (void)url; (void)segundos; (void)n; return NULL;
}

int main(void) {
  const char *documents[] = {
    "1\n00:00:01,000 --> 00:00:03,000\nEarly\n\n"
    "2\n00:05:00,000 --> 00:05:02,000\nFive minutes\n\n"
    "3\n00:25:00,000 --> 00:25:02,000\nTwenty five minutes\n\n",
    "WEBVTT\n\n00:00:01.000 --> 00:00:03.000\nEarly\n\n"
    "00:05:00.000 --> 00:05:02.000\nFive minutes\n\n"
    "00:25:00.000 --> 00:25:02.000\nTwenty five minutes\n\n"
  };
  for (unsigned format = 0; format < sizeof documents / sizeof documents[0]; format++) {
    esmaecer_reiniciar();
    esmaecer_escolha(ESM_PADRAO);
    esmaecer_entrada(1000, 0);
    legenda_definir_corpo(documents[format]);
    unsigned generation = legenda_geracao();
    for (unsigned second = 1; second <= 1502; second++) {
      unsigned now = 1000 + second * 1000;
      esmaecer_quadro(now, .1f, 1);
      assert(esmaecer_estagio() == ESM_ACESO && esmaecer_veu() == 0);
      assert(legenda_ligada_em(generation));
      if (second == 2 || second == 301 || second == 1501) {
        const char *expected = second == 2 ? "Early" :
                               second == 301 ? "Five minutes" : "Twenty five minutes";
        char text[768];
        assert(legenda_texto(second, 0, text, sizeof text));
        assert(!strcmp(text, expected));
        printf("%s at %us: '%s'\n", format ? "VTT" : "SRT", second, text);
      }
    }
    char text[768];
    assert(!legenda_texto(1503, 0, text, sizeof text));
    /* Paused playback must still enter OLED protection after its timeout. */
    esmaecer_quadro(1503000 + esmaecer_ms(ESM_PADRAO), .1f, 0);
    assert(esmaecer_estagio() != ESM_ACESO);
    legenda_desligar();
    assert(!legenda_ligada_em(generation));
  }
  puts("PASS: external SRT/VTT and OLED-idle regression guard (not #484 reproduction)");
}
