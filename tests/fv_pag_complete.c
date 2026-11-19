// Issue #789 Phase-2: a paginated addon catalog must be consumed to its real
// end. Red on the old code: VT_STORE_MAX was 2000 and the dedupe was O(n^2).
// Fake network serves "big.invalid" catalogs:
//   /catalog/movie/big14k      14,000 items, 100 per page
//   /catalog/movie/big7        7,600 items, 100 per page
//   /catalog/movie/huge        17,000 items (larger than VT_STORE_MAX)
//   /catalog/movie/dup         page 0 repeats internally + repeats on page 1,
//                              one transient NULL page, ends with an empty page
//   /catalog/movie/mixed       same imdb id as movie AND series
//   /catalog/movie/watched     a seen item is projected out, not removed
//   /catalog/movie/hugeload    used with the reload path (page-0 republish)
// Dedup work is measured in strcmp comparisons (NV_VT_DEDUP_MEDIDA), not in
// wall clock, per the brief.
#include <assert.h>
#include <unistd.h>
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }
static int ocultar;
int ajustes_ocultar_nao_lancados(void) { return ocultar; }
static int ocultarVistos;
int ajustes_ocultar_filmes_vistos(void) { return ocultarVistos; }
unsigned cat_revisao_itens(void) { return 0; }
#include <stdlib.h>
// Store allocation hooks that count live bytes (nodes + payload), so the
// bounded-memory claim is measured, not asserted.
static long vtLiveBytes, vtPeakBytes, vtAllocs;
static void *vt_test_alloc(size_t n) {
  char *p = malloc(n + sizeof(size_t));
  if (!p) return NULL;
  *(size_t *)p = n;
  vtLiveBytes += (long)n; if (vtLiveBytes > vtPeakBytes) vtPeakBytes = vtLiveBytes;
  vtAllocs++;
  return p + sizeof(size_t);
}
static void vt_test_free(void *q) {
  char *p = (char *)q - sizeof(size_t);
  vtLiveBytes -= (long)*(size_t *)p; vtAllocs--;
  free(p);
}
#define NV_VT_STORE_ALLOC vt_test_alloc
#define NV_VT_STORE_FREE  vt_test_free
#define NV_VT_DEDUP_MEDIDA
#define NV_DUBLE_SESSAO
#include "../src/descoberta.c"
#include "../src/vt_store.h"
long vtDedupPares;   // declared by descoberta.c under NV_VT_DEDUP_MEDIDA
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_tmdb_ligado(void) { return 1; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *nuvem_trakt_cliente(void) { return ""; }
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
int trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t session) {
  (void)session; return trakt_enfeitar_lote(s, n);
}
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
void cat_trocar_continuar(const CatItem *l, int q) { (void)l; (void)q; }
static int vistoId;
int cat_visto(const CatItem *c) { return c && vistoId && !strcmp(c->imdb, "tt5000002"); }
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *s) { (void)id; (void)tipo; (void)s; return 0; }
static ColFolder pasta;
static ColSource fontePasta;
int col_n(void) { return 1; }
const ColFolder *col_folder(int i) { return i == 0 ? &pasta : NULL; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
#define LST_SO_ITENS
#include "../src/listas.c"
int caca_n_filmes(int i) { (void)i; return 0; }
int caca_filme(int i, int j, CacaFilme *out) { (void)i; (void)j; (void)out; return 0; }
int caca_itens(int i, int falta, void *out, int max) { (void)i; (void)falta; (void)out; (void)max; return 0; }
int caca_por_chave(const char *key) { (void)key; return -1; }
void caca_previa_lista(int tipo, long alvo, const char *lista, const char *nome, const char *foto) { (void)tipo; (void)alvo; (void)lista; (void)nome; (void)foto; }
int mus_listar(MusSalva *out, int max) { (void)out; (void)max; return 0; }
int servidores_pagina(const char *key, int start, int limit, CatItem *out, int max, int *total, int *raw) { (void)key; (void)start; (void)limit; (void)out; (void)max; (void)total; (void)raw; return -1; }

char *rede_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, timeout);
  *status = body ? 200 : 0;
  return body;
}
static int pagPedidos, falhaUmaVez, hugeMudou, hugeFalha;
static char *pageJson(int skip, int count, int total) {
  char *body = malloc(262144); assert(body);
  int used = snprintf(body, 262144, "{\"metas\":[");
  for (int pos = 0; pos < count && skip + pos < total; pos++) {
    int i = skip + pos;
    used += snprintf(body + used, 262144 - used,
      "%s{\"id\":\"tt%07d\",\"name\":\"Item %d%s\",\"poster\":\"p%d.jpg\",\"releaseInfo\":\"%d\"}",
      pos ? "," : "", 5000000 + i, i + 1, hugeMudou ? " v2" : "", i + 1, 1900 + i % 120);
  }
  snprintf(body + used, 262144 - used, "]}");
  return body;
}
char *rede_baixar(const char *url, int timeout) {
  (void)timeout;
  const char *p = strstr(url, "skip=");
  int skip = p ? atoi(p + 5) : 0;
  pagPedidos++;
  if (strstr(url, "/catalog/movie/big14k")) return pageJson(skip, 100, 14000);
  if (strstr(url, "/catalog/movie/big7"))   return pageJson(skip, 100, 7600);
  if (strstr(url, "/catalog/movie/huge")) {
    if (hugeFalha) { hugeFalha = 0; return NULL; }
    return pageJson(skip, 100, 17000);
  }
  if (strstr(url, "/catalog/movie/dup")) {
    // Page 0: 100 items + one intra-page duplicate. Page 1: full repeat of
    // page 0 (added==0 must still end pagination). Page 2: transient NULL.
    if (skip == 0) {
      char *b = pageJson(0, 100, 100);
      size_t u = strlen(b) - 2;
      snprintf(b + u, 262144 - u, ",{\"id\":\"tt5000001\",\"name\":\"Dup\",\"poster\":\"p.jpg\"}]}");
      return b;
    }
    if (skip <= 101) {
      if (falhaUmaVez) { falhaUmaVez = 0; return NULL; }
      return pageJson(0, 100, 100);   // same items again
    }
    return strdup("{\"metas\":[]}");   // terminal empty page
  }
  if (strstr(url, "/catalog/all/mixed"))
    return strdup("{\"metas\":[{\"id\":\"tt5000001\",\"name\":\"Filme\",\"poster\":\"p.jpg\",\"type\":\"movie\"},"
                  "{\"id\":\"tt5000001\",\"name\":\"Serie\",\"poster\":\"p.jpg\",\"type\":\"series\"}]}");
  if (strstr(url, "/catalog/movie/watched")) return pageJson(skip, 100, 100);
  return NULL;
}
static void waitDone(void) {
  for (int i = 0; i < 40000 && desc_vertudo_carregando(); i++) usleep(1000);
  assert(!desc_vertudo_carregando());
}
static int tem(const char *id) {
  CatItem it;
  for (int i = 0; i < desc_vertudo_n(); i++)
    if (desc_vertudo_item(i, &it) && !strcmp(it.imdb, id)) return 1;
  return 0;
}
static void drena(void) {
  for (int i = 0; i < 300 && !desc_vertudo_fim() && !desc_vertudo_erro(); i++) {
    desc_vertudo_mais(); waitDone();
  }
}

int main(void) {
  CatItem it;
  /* --- 14,000-item catalog: consumed whole, in order, no duplicates --- */
  desc_vertudo_abrir("https://big.invalid", "movie", "big14k"); waitDone();
  drena();
  { long pares = vtDedupPares;
    printf("big14k: n=%d fim=%d erro=%d pagina=%d dedupStrcmps=%ld\n",
           desc_vertudo_n_bruto(), desc_vertudo_fim(), desc_vertudo_erro(),
           desc_vertudo_pagina(), pares);
    assert(!desc_vertudo_erro() && desc_vertudo_fim());
    assert(desc_vertudo_n() == 14000 && desc_vertudo_n_bruto() == 14000);
    // Linear dedup bound: with unique ids the only strcmps are hash
    // collisions. A generous bound that an O(n^2) scan could never meet.
    assert(pares < 3000000);
    assert(desc_vertudo_item(0, &it) && !strcmp(it.imdb, "tt5000000") && it.ranking == 1);
    assert(desc_vertudo_item(13999, &it) && !strcmp(it.imdb, "tt5013999") && it.ranking == 14000);
    assert(tem("tt5007777")); }
  /* --- 7,600-item catalog --- */
  desc_vertudo_abrir("https://big.invalid", "movie", "big7"); waitDone();
  drena();
  { long pares = vtDedupPares;
    printf("big7: n=%d fim=%d erro=%d dedupStrcmps=%ld\n",
           desc_vertudo_n_bruto(), desc_vertudo_fim(), desc_vertudo_erro(), pares);
    assert(!desc_vertudo_erro() && desc_vertudo_fim() && desc_vertudo_n() == 7600);
    assert(desc_vertudo_item(7599, &it) && !strcmp(it.imdb, "tt5007599")); }
  /* --- duplicates: intra-page + whole-page repeat + transient failure --- */
  falhaUmaVez = 1;
  desc_vertudo_abrir("https://big.invalid", "movie", "dup"); waitDone();
  assert(!desc_vertudo_erro() && desc_vertudo_n() == 100 && !desc_vertudo_fim());
  desc_vertudo_mais(); waitDone();       // transient NULL on skip=101
  assert(desc_vertudo_erro() && desc_vertudo_n() == 100 && !desc_vertudo_fim());
  desc_vertudo_mais(); waitDone();       // retry: page repeats, added==0 ends
  assert(!desc_vertudo_erro() && desc_vertudo_fim() && desc_vertudo_n() == 100);
  /* --- same id, different type: both kept --- */
  desc_vertudo_abrir("https://big.invalid", "all", "mixed"); waitDone();
  { int tipos = 0;
    for (int i = 0; i < desc_vertudo_n(); i++)
      if (desc_vertudo_item(i, &it) && !strcmp(it.imdb, "tt5000001")) tipos++;
    assert(tipos == 2 && desc_vertudo_n() == 2); }
  /* --- hide-watched is a projection: list keeps 100, view shows 99 --- */
  vistoId = 1; ocultarVistos = 1;
  desc_vertudo_abrir("https://big.invalid", "movie", "watched"); waitDone();
  desc_vertudo_mais(); waitDone();       // drains the empty terminal page
  assert(desc_vertudo_fim() && desc_vertudo_n_bruto() == 100 && desc_vertudo_n() == 99);
  { int i = 0; for (; i < desc_vertudo_n(); i++) {
      assert(desc_vertudo_item(i, &it));
      assert(strcmp(it.imdb, "tt5000002")); } }
  ocultarVistos = 0;
  assert(desc_vertudo_n() == 100);
  /* --- memory: 14k measured live bytes must stay under the byte budget --- */
  { printf("store: live=%ld peak=%ld allocs=%ld budget=%u\n", vtLiveBytes,
           vtPeakBytes, vtAllocs, VT_ADDON_BYTES_MAX);
    assert(vtPeakBytes <= VT_ADDON_BYTES_MAX); }
  /* --- bigger-than-bound catalog: truthful partial, retry, then reload --- */
  desc_vertudo_abrir("https://big.invalid", "movie", "huge"); waitDone();
  drena();
  { int n = desc_vertudo_n();
    printf("huge: n=%d fim=%d erro=%d pagina=%d\n", n, desc_vertudo_fim(),
           desc_vertudo_erro(), desc_vertudo_pagina());
    assert(desc_vertudo_erro() && !desc_vertudo_fim() && n == VT_STORE_MAX);
    // Retry of an unchanged capped page: no loss, no duplicates.
    desc_vertudo_mais(); waitDone();
    assert(desc_vertudo_n() == n);
    // A changed capped page arms vtPaginaMudou -> staged reload on reopen.
    hugeMudou = 1; desc_vertudo_mais(); waitDone(); hugeMudou = 0;
    assert(desc_vertudo_n() == n);
    // Failed reload keeps the old list published (fail-safe).
    hugeFalha = 1;
    desc_vertudo_abrir("https://big.invalid", "movie", "huge"); waitDone();
    assert(desc_vertudo_erro() && desc_vertudo_n() == n);
    assert(desc_vertudo_item(0, &it) && !strcmp(it.imdb, "tt5000000"));
    // Successful staged reload republishes page 0 cleanly and ordered.
    desc_vertudo_abrir("https://big.invalid", "movie", "huge"); waitDone();
    assert(!desc_vertudo_erro() && desc_vertudo_n() == 100);
    assert(desc_vertudo_item(0, &it) && !strcmp(it.imdb, "tt5000000") && it.ranking == 1);
    assert(vtLiveBytes > 0 && vtAllocs == 100);
    drena();
    assert(desc_vertudo_n() == VT_STORE_MAX && desc_vertudo_erro());
    printf("store2: live=%ld peak=%ld allocs=%ld\n", vtLiveBytes, vtPeakBytes, vtAllocs);
  }
  puts("fv_pag_complete: PASS (14k/7.6k whole catalogs, dedup linear, bounded)");
  return 0;
}
