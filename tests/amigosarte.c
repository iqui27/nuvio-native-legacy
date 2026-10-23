#include "socialvis.h"
#include "artemetahub.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SvEvento brutos[SV_EVENTOS_MAX], feed[SV_EVENTOS_MAX], *externo;
static SvAmigo amigos[SV_AMIGOS_MAX];
static int nBrutos, nFeed, nAmigos, nExterno, nCtts, temExterno;
static unsigned rev, hashUlt;
static CatItem catalog[8];
static int count = 1, batches;
int cat_indice_por_imdb(const char *id) {
  for (int i = 0; i < count; i++) if (!strcmp(catalog[i].imdb, id)) return i;
  return -1;
}
const CatItem *cat_item(int i) { return i >= 0 && i < count ? &catalog[i] : NULL; }
int cat_acrescentar_lote(const CatItem *v, int n, int *indices) {
  (void)indices; batches++;
  assert(count + n <= 8);
  memcpy(catalog + count, v, sizeof *v * n); count += n; return n;
}
static int svDoServidor(SvEvento *v, int n) { (void)v; (void)n; return -1; }
#define REC_CONTATOS_MAX 1
static int ctts[1];
static int recomenda_contatos(int *v, int n) { (void)v; (void)n; return 0; }
static void dasRecomendacoes(void) {}
static void svDoQueExiste(void) {}
static void agrupar(void) { nFeed = nBrutos; memcpy(feed, brutos, sizeof *feed * nFeed); }
static unsigned fnv(unsigned h, const void *p, size_t n) {
  const unsigned char *b = p; while (n--) h = (h ^ *b++) * 16777619u; return h;
}
#include "feed.inc"

int main(void) {
  static SvEvento events[3];
  temExterno = 1; externo = events; nExterno = 3;
  for (int i = 0; i < 3; i++) {
    snprintf(events[i].imdb, sizeof events[i].imdb, "%s", i == 2 ? "tt2222222" : "tt1111111");
    snprintf(events[i].tipo, sizeof events[i].tipo, "%s", i == 2 ? "movie" : "series");
    snprintf(events[i].titulo, sizeof events[i].titulo, "Feed title");
    events[i].temporada = 2; events[i].episodio = 3;
  }
  refazer();
  assert(nFeed == 3);
  assert(feed[0].poster[0] && feed[0].arte[0]);
  int k = cat_indice_por_imdb(events[0].imdb);
  assert(k >= 0 && catalog[k].logo[0] && catalog[k].backdrop[0]);
  assert(!strcmp(catalog[k].tipo, "series"));
  assert(!strstr(catalog[k].poster, ":2:3"));
  assert(count == 3 && batches == 1); // duplicate events share one seed/batch
  refazer();
  assert(count == 3 && batches == 1); // refresh never appends again
  snprintf(catalog[k].poster, sizeof catalog[k].poster, "https://cdn.example/poster.jpg");
  refazer();
  assert(!strcmp(feed[0].poster, catalog[k].poster));
  puts("amigosarte: PASS (missing titles, poster, hero, deduplication, refresh, existing art)");
}
