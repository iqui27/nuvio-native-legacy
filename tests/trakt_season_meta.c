// Issue #512: anime split seasons must keep addon S/E for next-episode and Trakt
// scrobble must carry episode ids from the selected meta (TVDB/TMDB/Trakt).
#include "trakt.h"
#include "traktscrobble.h"
#include "metaep.h"
#include "js.h"
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Standalone tracker fixture: one fixed account opening, not account I/O.
// Match the real recursive lease contract and reject every other epoch.
static const uint64_t sessaoFixture = 37;
static pthread_mutex_t sessaoMutex;
static pthread_once_t sessaoOnce = PTHREAD_ONCE_INIT;
static _Thread_local unsigned sessaoProfundidade;
static unsigned sessaoLocks, sessaoUnlocks;
static void sessaoPreparar(void) {
  pthread_mutexattr_t a;
  assert(!pthread_mutexattr_init(&a));
  assert(!pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE));
  assert(!pthread_mutex_init(&sessaoMutex, &a));
  assert(!pthread_mutexattr_destroy(&a));
}
uint64_t perfis_sessao_geracao(void) { return sessaoFixture; }
int perfis_sessao_atual(uint64_t g) { return g == sessaoFixture; }
int perfis_sessao_travar(uint64_t g) {
  if (!perfis_sessao_atual(g)) return 0;
  assert(!pthread_once(&sessaoOnce, sessaoPreparar));
  assert(!pthread_mutex_lock(&sessaoMutex));
  sessaoProfundidade++;
  __atomic_add_fetch(&sessaoLocks, 1, __ATOMIC_RELAXED);
  return 1;
}
void perfis_sessao_destravar(void) {
  assert(sessaoProfundidade);
  sessaoProfundidade--;
  __atomic_add_fetch(&sessaoUnlocks, 1, __ATOMIC_RELAXED);
  assert(!pthread_mutex_unlock(&sessaoMutex));
}

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

static char *ler(const char *nome) {
  char path[200]; FILE *f; char *b; long n;
  snprintf(path, sizeof path, "tests/fixtures/%s", nome);
  f = fopen(path, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f); return b;
}

char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "tt29422052")) return ler("kaiju_tvdb_s2.json");
  return NULL;
}
char *rede_baixar_com(const char *u, int s, const char *const *c) {
  (void)s; (void)c;
  if (strstr(u, "/sync/playback")) return strdup("[]");
  if (strstr(u, "/sync/last_activities")) {
    return strdup("{\"movies\":{\"watched_at\":\"2026-03-01T00:00:00.000Z\"}}");
  }
  if (strstr(u, "/progress/watched")) {
    return strdup("{\"aired\":13,\"completed\":12,\"next_episode\":{\"season\":1,\"number\":13,\"title\":\"x\"}}");
  }
  if (strstr(u, "/sync/history")) {
    return strdup("[{\"watched_at\":\"2026-03-01T10:00:00.000Z\",\"type\":\"episode\","
                  "\"episode\":{\"season\":1,\"number\":12,\"title\":\"finale\"},"
                  "\"show\":{\"title\":\"Kaiju\",\"ids\":{\"imdb\":\"tt29422052\"}}}]");
  }
  return NULL;
}
char *rede_apagar(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c;
  if (st) *st = 200;
  if (strstr(u, "/sync/watched/shows")) return strdup("[]");
  if (strstr(u, "/sync/watched/movies")) return strdup("[]");
  if (st) *st = 0;
  return NULL;
}
int cwo_conta_a_seguir(const char *id) { (void)id; return 0; }
void cwo_conta_trocar(const char *a, const char *b) { (void)a; (void)b; }
void cwo_marcar_estreia(const char *id, long long ms) { (void)id; (void)ms; }
int cwo_virada_aceita(long long e, long long a) { (void)e; (void)a; return 1; }
int ajustes_tmdb_cw(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "en-US"; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "en"; }
const char *i18n(const char *s) { return s; }
unsigned long long cat_historico_geracao(void) { return 1; }
int cat_historico_definir_se_geracao(const char *a, const char *b, int c, unsigned long long g) {
  (void)a; (void)b; (void)c; (void)g; return 1;
}

static void episodio_correto(const char *body) {
  char ep[300], ids[220], show[300];
  assert(js_bruto(body, NULL, "episode", ep, sizeof ep));
  assert(js_bruto(ep, NULL, "ids", ids, sizeof ids));
  assert(js_num(ids, NULL, "tvdb", 0) == 10600123);
  assert(js_num(ids, NULL, "trakt", 0) == 9923456);
  assert(!js_tem(ids, NULL, "tmdb") && !js_tem(ids, NULL, "imdb"));
  assert(!js_tem(ep, NULL, "season") && !js_tem(ep, NULL, "number"));
  assert(!js_bruto(body, NULL, "show", show, sizeof show));
  assert(js_num(body, NULL, "progress", -1) == 41.0);
}

int main(void) {
  static CatItem v[4];
  char corpo[500];
  MetaEpInfo ep;
  ScrobbleEpIds ids = { "10600123", "5234501", "9923456" };
  int n;
  char *meta = ler("kaiju_tvdb_s2.json");

  assert(metaep_existe(meta, 2, 1));
  assert(!metaep_existe(meta, 1, 13));
  assert(metaep_proximo(meta, 1, 12, &ep));
  assert(ep.temporada == 2 && ep.episodio == 1);
  // Fixture stores episode ids as JSON numbers, not strings.
  assert(metaep_buscar(meta, 2, 1, &ep));
  assert(!strcmp(ep.tvdb, "10600123"));
  assert(!strcmp(ep.tmdb, "5234501"));
  assert(!strcmp(ep.trakt, "9923456"));
  metaep_scrobble_corpo(corpo, sizeof corpo, "tt29422052", 2, 1, &ep, 41.0);
  episodio_correto(corpo);

  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 41.0, &ids);
  episodio_correto(corpo);

  assert(perfis_sessao_geracao() != 0 && perfis_sessao_atual(sessaoFixture));
  assert(!perfis_sessao_atual(0) && !perfis_sessao_atual(sessaoFixture + 1));
  assert(!perfis_sessao_travar(0) && !perfis_sessao_travar(sessaoFixture + 1));
  assert(perfis_sessao_travar(sessaoFixture) && perfis_sessao_travar(sessaoFixture));
  perfis_sessao_destravar(); perfis_sessao_destravar();
  unsigned antes = __atomic_load_n(&sessaoLocks, __ATOMIC_RELAXED);
  trakt_definir("token", "client");
  n = trakt_continuar(v, 4);
  assert(n == 1);
  assert(v[0].temporada == 2 && v[0].episodio == 1);
  assert(!strcmp(v[0].imdb, "tt29422052:2:1"));

  assert(__atomic_load_n(&sessaoLocks, __ATOMIC_RELAXED) > antes);
  assert(!sessaoProfundidade && __atomic_load_n(&sessaoLocks, __ATOMIC_RELAXED) ==
                               __atomic_load_n(&sessaoUnlocks, __ATOMIC_RELAXED));
  free(meta);
  puts("trakt_season_meta: PASS");
  return 0;
}
