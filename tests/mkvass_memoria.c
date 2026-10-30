// Exercise the app's actual memory callback with the embedded reader state.
#include "../src/mkvass.c"
#include <assert.h>

char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; return NULL; }
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *n,
                           int *st, int *er, char *fin, unsigned tf) {
  (void)u; (void)s; (void)a; (void)b; (void)n; (void)st; (void)er; (void)fin; (void)tf;
  return NULL;
}
int rede_resto_recusado(void) { return 0; }
long rede_corte_host(const char *u) { (void)u; return 0; }
const char *rede_url_publica(const char *u, char *dst, unsigned n) {
  (void)u; snprintf(dst, n, "local"); return dst;
}
static int trims;
static void tex_aliviar(void) { trims++; }
static void txt_aliviar(void) { trims++; }
static void memlog_devolver(void) { trims++; }
#include "memory_callback.h"

int main(void) {
  Fio f = {0};
  int inWindow;
  const char *first = "1\n00:03:15,000 --> 00:03:17,000\nBefore pressure\n";
  const char *next = "1\n00:03:30,000 --> 00:03:32,000\nAfter cached window\n";
  LegendaCue cue;
  S.estado = MKVASS_COLHENDO; S.vivos = 1; S.parar = 0;
  S.pos = 195.636; S.geracao = 5;
  f.g = 5; f.lateral = 1; f.escala = 1000000;
  f.nPontos = 1; f.pontos = calloc(1, sizeof *f.pontos);
  assert(f.pontos);
  f.pontos[0].tempo = 210000; f.pontos[0].rel = 1;
  legenda_definir_corpo(first); f.legG = legenda_geracao(); f.entregas = 1;
  assert(legenda_cues(196, 0, &cue, 1) == 1);
  assert(proximoPendente(&f, S.pos - MKVASS_ATRAS_SEG,
                         S.pos + alcanceFrente(&f), &inWindow) == -1);
  aliviarMemoria();
  assert(trims == 3);
  assert(mkvass_estado() == MKVASS_COLHENDO && minhaVez(&f));
  mkvass_passo(210);
  assert(S.pos == 210);
  assert(proximoPendente(&f, S.pos - MKVASS_ATRAS_SEG,
                         S.pos + alcanceFrente(&f), &inWindow) == 0 && inWindow);
  assert(entregarCorpoSeAtual(&f, next));
  assert(legenda_cues(211, 0, &cue, 1) == 1);
  assert(!strcmp(cue.texto, "After cached window"));
  free(f.pontos);
  puts("embedded subtitles continue beyond cached window after memory pressure: ok");
  return 0;
}
