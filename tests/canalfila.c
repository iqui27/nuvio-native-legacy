// A fila de fontes de canal (streams.c: stream_canal_primeira_viva,
// stream_canal_proxima, stream_canal_prazo_longo) contra um servidor local
// que imita o que a C9 mediu em 25/09 na HBO Mundi: a 4K devolve 71 KB sem
// segmento e TOCA, as outras nao respondem a sonda em 3 s.
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int porta;
static void fonte(Stream *s, const char *caminho) {
  memset(s, 0, sizeof *s);
  snprintf(s->url, sizeof s->url, "http://127.0.0.1:%d/%s", porta, caminho);
}

int main(int argc, char **argv) {
  Stream l[5];
  int e, p;
  porta = argc > 1 ? atoi(argv[1]) : 18771;

  // 1. Nenhuma viva: 0 grande (incerta), 1 e 3 mudas, 2 morta.
  fonte(&l[0], "grande"); fonte(&l[1], "muda1"); fonte(&l[2], "morta");
  fonte(&l[3], "muda3");
  stream_definir_lista(l, 4);
  e = stream_canal_primeira_viva(8);
  printf("caso 1: escolhida %d\n", e);
  assert(e == 0);                       // a incerta vai na frente das mudas
  p = stream_canal_proxima(0); assert(p == 1);
  p = stream_canal_proxima(1); assert(p == 3);
  p = stream_canal_proxima(3); assert(p == 2);   // a morta entra, por ultimo
  p = stream_canal_proxima(2); assert(p == -1);
  // Sonda sem nenhuma viva nao informou nada: prazo cheio para todas.
  assert(stream_canal_prazo_longo(1) && stream_canal_prazo_longo(3));

  // 2. Com uma viva: ela abre, e a muda depois dela tem prazo curto.
  fonte(&l[0], "muda0"); fonte(&l[1], "viva"); fonte(&l[2], "grande");
  stream_definir_lista(l, 3);
  e = stream_canal_primeira_viva(8);
  printf("caso 2: escolhida %d\n", e);
  assert(e == 1);
  assert(stream_canal_proxima(1) == 2);
  assert(stream_canal_proxima(2) == 0);
  assert(stream_canal_prazo_longo(1) && stream_canal_prazo_longo(2));
  assert(!stream_canal_prazo_longo(0));

  // 3. Lista nova sem sonda: a fila velha nao vale, volta a ordem do addon.
  stream_definir_lista(l, 3);
  assert(stream_canal_proxima(1) == 2 && stream_canal_proxima(2) == -1);
  puts("canalfila: ok");
  return 0;
}
