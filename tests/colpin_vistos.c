// #788 revisao: o buffer de colpinItens tem que honrar o contrato de
// descoberta.h — limite vagas de prefixo canonico mais limite vagas de
// reserva de itens nao vistos. Antes eram so 24 vagas e a reserva escrevia
// em itens[24] com "Ocultar vistos" ativo (ASan global-buffer-overflow).
// Funcoes de producao extraidas de src/descoberta.c por colpin_vistos.sh
// (producao.inc); so transporte, conta e vistos sao dubles.
//
//   bash tests/colpin_vistos.sh
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "descoberta.h"
#include "colecoes.h"
#include "addons.h"
#include "rede.h"
#include "js.h"
#include "perfisessao.h"

static int requests, watched;
static const char *scenario;
static int descSessaoVincular(NvPerfilSessao session) { return session == 0; }
static int descSessaoAtual(void) { return 1; }
const char *i18n(const char *label) { return label; }
const char *desc_chave_tmdb(void) { return "fixture"; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
const char *nuvem_trakt_cliente(void) { return "fixture"; }
int ajustes_ocultar_filmes_vistos(void) { return watched; }
int desc_filme_visto(const CatItem *item) {
  return watched && (item->tmdb ? item->tmdb : strtol(item->imdb + 2, NULL, 10)) <= 24;
}
static int descOcultaNaoLancado(const CatItem *item) { (void)item; return 0; }
static int descEnfeitar(CatItem *items, int count) { (void)items; return count; }

static char *fixture_json(int page, int trakt) {
  char data[8192];
  const char *key = !strcmp(scenario, "discover") ? "results" : "items";
  int used = trakt ? snprintf(data, sizeof data, "[") :
                     snprintf(data, sizeof data, "{\"page\":%d,\"total_pages\":2,\"%s\":[", page, key);
  int count = trakt ? 30 : 20;
  for (int index = 0; index < count; index++) {
    int id = (page - 1) * 20 + index + 1;
    const char *comma = index ? "," : "";
    used += trakt ?
      snprintf(data + used, sizeof data - used, "%s{\"movie\":{\"title\":\"Fixture %d\",\"ids\":{\"imdb\":\"tt%07d\"}}}", comma, id, id) :
      snprintf(data + used, sizeof data - used, "%s{\"id\":%d,\"title\":\"Fixture %d\",\"poster_path\":\"/a.jpg\"}", comma, id, id);
    assert(used < (int)sizeof data - 3);
  }
  strcat(data, trakt ? "]" : "]}");
  return strdup(data);
}

char *rede_baixar(const char *url, int seconds) {
  (void)seconds;
  assert(strstr(url, !strcmp(scenario, "discover") ? "/discover/movie?" : "/list/42?"));
  if (!strcmp(scenario, "discover"))
    assert(strstr(url, "&with_genres=18") && strstr(url, "&vote_average.gte=7"));
  printf("HTTP simulado TMDB %s\n", strstr(url, "/3/") + 2);
  requests++;
  return fixture_json(strstr(url, "page=2") ? 2 : 1, 0);
}

char *rede_baixar_com(const char *url, int seconds, const char *const *headers) {
  (void)seconds;
  assert(strstr(url, "/lists/42/items/movie?page=1&limit=40") && headers[1]);
  printf("HTTP simulado Trakt %s\n", strstr(url, "/lists/"));
  requests++;
  return fixture_json(1, 1);
}

char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)url; (void)seconds; (void)headers; (void)status; abort();
}
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
int addons_base_desligada(const char *base) { (void)base; abort(); }
int cat_n_fileiras(void) { return 0; }
const CatFileira *cat_fileira(int index) { (void)index; abort(); }
const CatItem *cat_item(int index) { (void)index; abort(); }
static int lerCatalogo(const char *base, const char *type, const char *id, CatItem *out,
                       int max, int count, const char *genre, int *replied) {
  (void)base; (void)type; (void)id; (void)out; (void)max; (void)count; (void)genre; (void)replied; abort();
}
static int catalogoUrl(char *url, size_t size, const char *base, const char *type, const char *id, const char *extra) {
  (void)url; (void)size; (void)base; (void)type; (void)id; (void)extra; abort();
}
static int descPedidoCoube(const char *base, int result, size_t size) {
  (void)base; (void)result; (void)size; abort();
}
static int deMeta(const char *start, const char *end, const char *type, CatItem *item) {
  (void)start; (void)end; (void)type; (void)item; abort();
}
static void origemDaBase(const char *base, char *dst, size_t size) { (void)base; (void)dst; (void)size; abort(); }
static void colecaoLog(const char *title, const char *base, const char *type, const char *id,
                      const char *genre, int skip, int status, int count, const char *why) {
  (void)title; (void)base; (void)type; (void)id; (void)genre; (void)skip; (void)status; (void)count; (void)why; abort();
}

#include "producao.inc"

int main(int argc, char **argv) {
  assert(argc == 4);
  setvbuf(stdout, NULL, _IONBF, 0);
  scenario = argv[1];
  watched = atoi(argv[2]);
  int expected = atoi(argv[3]);
  static ColSource src;
  static ColFolder folder = {.sources = &src, .nSources = 1};
  static CatItem items[DESC_ITENS_POR_FILEIRA];
  char dead[COLPIN_MORTAS][sizeof src.base] = {{0}};
  int dead_count = 0;
  if (!strcmp(scenario, "discover")) {
    snprintf(src.filtros, sizeof src.filtros, "{\"withGenres\":\"18\",\"voteAverageGte\":7}");
  } else {
    snprintf(src.prov, sizeof src.prov, "%s", scenario);
    snprintf(src.midia, sizeof src.midia, "MOVIE");
    if (!strcmp(scenario, "tmdb")) {
      snprintf(src.tmdbTipo, sizeof src.tmdbTipo, "LIST");
      src.tmdbId = 42;
    } else {
      src.traktLista = 42;
    }
  }
  printf("cenário=%s limite=%d ocultar_vistos=%d\n", scenario, DESC_ITENS_POR_FILEIRA, watched);
  int count = colpinItens(&folder, items, DESC_ITENS_POR_FILEIRA, NULL, 0, NULL, dead, &dead_count);
  printf("pedidos=%d itens=%d primeiro=%s último=%s\n", requests, count, items[0].imdb,
         count ? items[count - 1].imdb : "");
  assert(count == expected && (!!requests == !!expected));
  if (count)
    assert(!strcmp(items[0].titulo, "Fixture 1"));
}
