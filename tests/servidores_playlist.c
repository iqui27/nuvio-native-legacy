// Real protocol parsers plus the real grid worker; no server or renderer.
#include "../src/descoberta.c"
#include "jellyfin.h"
#include "plex.h"
#include <assert.h>

static int phase, calls;
int servidores_pagina(const char *key, int start, int limit, CatItem *out, int max, int *total, int *raw) {
  (void)key; (void)limit; (void)max;
  assert(start == calls * 60);
  calls++;
  *total = 121;
  if (raw) *raw = start < 120 ? 60 : 1;
  memset(out, 0, sizeof *out);
  return start < 120 ? 59 : 1;
}
int ajustes_busca_cinemeta(void) { return 0; }
int ajustes_tmdb_ligado(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "en-US"; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int m, const char *s) { (void)m; return s; }
const char *nuvem_trakt_cliente(void) { return ""; }
char *rede_baixar(const char *u, int t) { (void)u; (void)t; return NULL; }
char *rede_baixar_com(const char *u, int t, const char *const *h) { (void)h; return rede_baixar(u,t); }
int addons_n(void) { return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }

int main(int argc, char **argv) {
  (void)argc;
  phase = atoi(argv[1]);
  CatItem *item = calloc(2, sizeof *item);
  int total;
  if (phase < 2) {
    JfConta c = {0};
    c.tipo = phase ? JF_TIPO_EMBY : JF_TIPO_JELLYFIN;
    strcpy(c.servidorId, "0123456789abcdef");
    const char *json = "{\"TotalRecordCount\":1,\"Items\":[{\"Id\":\"eeee\",\"Type\":\"Episode\",\"SeriesId\":\"abcd\",\"SeriesName\":\"Show\",\"Name\":\"Pilot\",\"ParentIndexNumber\":1,\"IndexNumber\":2,\"ProviderIds\":{\"Imdb\":\"tt999\"}}]}";
    assert(jf_ler_itens(&c, json, strlen(json), item, 2, &total) == 1);
    assert(!strcmp(item->imdb, phase ? "em:01234567.abcd" : "jf:01234567.abcd"));
  } else if (phase == 2) {
    PxConta c = {0}; strcpy(c.servidorId, "0123456789abcdef");
    const char *json = "{\"MediaContainer\":{\"totalSize\":1,\"Metadata\":[{\"ratingKey\":\"99\",\"type\":\"episode\",\"grandparentRatingKey\":\"42\",\"grandparentTitle\":\"Show\",\"title\":\"Pilot\",\"parentIndex\":1,\"index\":2,\"Guid\":[{\"id\":\"imdb://tt999\"}]}]}}";
    assert(px_ler_itens(&c, json, strlen(json), item, 2, &total) == 1);
    assert(!strcmp(item->imdb, "px:01234567.42"));
  } else {
    strcpy(vtBase, "servidores");
    for (int i = 0; i < 3; i++) {
      fioVerTudo(NULL);
      assert(vtPagina == (i < 2 ? (i + 1) * 60 : 121));
      assert(vtFim == (i == 2));
    }
    assert(vtN == 119 && calls == 3);
    free(item); return 0;
  }
  assert(total == 1 && !strcmp(item->tipo, "series"));
  assert(!strcmp(item->titulo, "Show") && !strcmp(item->nomeEpisodio, "Pilot"));
  assert(item->temporada == 1 && item->episodio == 2);
  free(item);
  return 0;
}
