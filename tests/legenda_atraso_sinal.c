// Subtitle delay SIGN regression (QE43Q60DAUXXH / 2.0.5): the user's delay is
// positive = subtitle shown LATER, negative = earlier. A cue at 10.0 s with
// +2000 ms must be hidden at position 11.0 s and shown at 12.0 s; with
// -2000 ms it is already shown at 8.0 s. Covers the manual-offset sign that
// player.c, legenda2.c and legsync feed to the cue lookup.
//
//   bash tests/legenda_atraso_sinal.sh
#include "legenda2.h"
#include "legenda.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// No network in this test.
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); abort(); }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; abort(); }
const char *i18n(const char *s) { return s; }

static const char *SRT =
  "1\n00:00:10,000 --> 00:00:12,000\ncue dez\n\n";

static int baixar(const char *url, long maxBytes, unsigned prazoMs,
                  int (*parar)(void *), void *u, char **corpo, long *n) {
  (void)maxBytes; (void)prazoMs; (void)parar; (void)u;
  if (!strstr(url, "/cue")) return -1;
  *n = (long)strlen(SRT);
  *corpo = malloc((size_t)*n + 1);
  memcpy(*corpo, SRT, (size_t)*n + 1);
  return 0;
}

static int em(double pos) {
  LegendaCue c;
  return legenda2_cues(pos, &c, 1) == 1 && !strcmp(c.texto, "cue dez");
}

int main(void) {
  int i;
  legenda2_definir_baixador(baixar);
  legenda2_reiniciar();
  legenda2_escolher("cue-1", "mem://cue", "pt", "Teste");
  for (i = 0; i < 4000 && legenda2_estado() != LEG2_ATIVA; i++) usleep(1000);
  assert(legenda2_estado() == LEG2_ATIVA);

  legenda2_definir_offset_manual(0);
  assert(em(10.5) && !em(9.5) && !em(12.5));

  // +2000 ms delay = subtitle LATER: at 11.0 the lookup is t=9.0 (hidden),
  // at 12.0 the lookup is t=10.0 (shown).
  legenda2_definir_offset_manual(2000);
  assert(!em(11.0));
  assert(em(12.0));

  // -2000 ms = subtitle EARLIER: at 8.0 the lookup is t=10.0 (shown).
  legenda2_definir_offset_manual(-2000);
  assert(em(8.0));

  legenda2_definir_offset_manual(0);
  legenda2_reiniciar();
  legenda2_encerrar();
  puts("legenda_atraso_sinal: +delay shows later, -delay shows earlier ok");
  return 0;
}
