// Roda UM scraper de plugin Nuvio no Mac, com rede de verdade, e imprime as
// fontes como a folha as veria (stream_extrair) e o custo: tempo, pico de
// memoria do QuickJS, DOM, fetches e RSS do processo.
//
//   /tmp/nuvio-plugin-rodar <arquivo.js> <id> <tmdb> <movie|tv> [temporada episodio]
#include "pluginjs.h"
#include "rede.h"
#include "streams.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <pthread.h>

typedef struct { PjPedido p; PjResultado r; } Arg;
static void *fio(void *u) { Arg *a = u; pj_executar(&a->p, &a->r); return NULL; }

int main(int argc, char **argv) {
  Arg a; FILE *f; long n; char *src; pthread_t t; pthread_attr_t at;
  Stream *l = NULL; int k, m; struct rusage ru;
  if (argc < 5) { fprintf(stderr, "uso: %s arquivo.js id tmdb movie|tv [t e]\n", argv[0]); return 2; }
  f = fopen(argv[1], "rb"); if (!f) return 2;
  if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return 2; }
  src = malloc((size_t)n + 1);
  if (!src || fread(src, 1, (size_t)n, f) != (size_t)n) { free(src); fclose(f); return 2; }
  src[n] = 0; fclose(f);
  rede_preparar();
  memset(&a, 0, sizeof a);
  a.p.codigo = src; a.p.nCodigo = (size_t)n; a.p.arquivo = argv[1];
  a.p.idScraper = argv[2]; a.p.nomeScraper = argv[2];
  a.p.tmdbId = argv[3]; a.p.tipo = argv[4];
  a.p.temporada = argc > 5 ? atoi(argv[5]) : 0; a.p.episodio = argc > 6 ? atoi(argv[6]) : 0;
  a.p.prazoMs = getenv("PRAZO") ? atoi(getenv("PRAZO")) : 20000;
  a.p.verLog = getenv("VERLOG") != NULL;
  a.p.ajustesJson = "{}"; a.p.tmdbChave = getenv("TMDB_CHAVE") ? getenv("TMDB_CHAVE") : "";
  pthread_attr_init(&at); pthread_attr_setstacksize(&at, pj_pilha());
  k = pthread_create(&t, &at, fio, &a); pthread_attr_destroy(&at);
  if (k) { free(src); return 2; }
  pthread_join(t, NULL);
  m = a.r.json ? stream_extrair(a.r.json, argv[2], &l) : 0;
  for (k = 0; k < m && k < 8; k++)
    printf("  fonte %d: [%s] %s | %dp mp4=%d | %.80s\n", k, l[k].provedor, l[k].rotulo, l[k].altura, l[k].mp4, l[k].url);
  getrusage(RUSAGE_SELF, &ru);
  printf("RESULTADO %s: fontes=%d (mapeadas %d) ms=%lu compilar=%lu memPicoKB=%zu domPicoKB=%zu fetches=%d falhos=%d redeKB=%ld cripto=%d estourou=%d rssMB=%.1f cpuMs=%.0f erro=%s\n",
         argv[2], a.r.n, m, a.r.ms, a.r.msCompilar, a.r.memPico / 1024, a.r.domPico / 1024,
         a.r.fetches, a.r.fetchesFalhos, a.r.bytesRede / 1024, a.r.criptoUsado, a.r.estourou,
         ru.ru_maxrss / 1048576.0,
         (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1e3 + (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e3,
         a.r.erro[0] ? a.r.erro : "-");
  free(l); pj_resultado_soltar(&a.r); free(src);
  return a.r.erro[0] || m <= 0 ? 1 : 0;
}
