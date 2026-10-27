// #512: a tracker episode and an addon episode can have different numbering.
// The transport is fake; trakt_continuar, its enrichment, and Next Up are real.
// The Trakt payload is synthetic. Cinemeta's four episode objects are reduced
// from the public response with provenance in fixtures/episode_identity_512/.
#include "trakt.h"
#include "proximo.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

static const char *modo;
static int apagados, metaGets;

static char *ler(const char *nome) {
  char p[240];
  FILE *f;
  long n;
  char *b;
  snprintf(p, sizeof p, "tests/fixtures/episode_identity_512/%s", nome);
  f = fopen(p, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); n = ftell(f); assert(n >= 0);
  assert(!fseek(f, 0, SEEK_SET));
  b = malloc((size_t)n + 1); assert(b);
  assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f); return b;
}

char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback")) {
    char *b = malloc(1600); assert(b);
    int normal = !strcmp(modo, "normal");
    const char *tvdb = !strcmp(modo, "missing") || normal ? "" : ",\"tvdb\":11209906";
    snprintf(b, 1600,
      "[{\"id\":477001,\"progress\":40,\"paused_at\":\"2026-10-01T00:00:00.000Z\","
      "\"type\":\"episode\",\"episode\":{\"season\":%d,\"number\":%d,"
      "\"title\":\"The Next Generation's Trial\",\"ids\":{\"trakt\":9477001%s}},"
      "\"show\":{\"title\":\"Kaiju No. 8\",\"year\":2024,\"runtime\":24,"
      "\"overview\":\"An ordinary series description.\",\"ids\":{\"imdb\":\"tt21975436\"}}}]",
      normal ? 2 : 1, normal ? 2 : 14, tvdb);
    return b;
  }
  if (strstr(url, "/sync/history") || strstr(url, "/sync/watched/movies")) return strdup("[]");
  if (strstr(url, "/sync/last_activities")) return strdup("{}");
  return NULL;
}
char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "/meta/series/tt21975436")) {
    metaGets++;
    if (!strcmp(modo, "offline")) return NULL;
    return ler(!strcmp(modo, "duplicate") ? "catalog_duplicate_episode_id.json"
                                           : "cinemeta_kaiju_reduced.json");
  }
  return NULL;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; if (st) *st = 200; return strdup("[]");
}
char *rede_baixar_st_retry(const char *url, int s, const char *const *cab, int *st, int *retry) {
  char *body = rede_baixar_com(url, s, cab);
  *st = body ? 200 : 0; *retry = 0; return body;
}
int rede_pedir(const RedePedido *pedido, RedeResposta *resposta) {
  (void)pedido; resposta->erro = REDE_INDISPONIVEL; return 0;
}
void rede_resposta_limpar(RedeResposta *resposta) {
  free(resposta->corpo); free(resposta->cabecalhos);
}
char *rede_apagar(const char *u, int s, const char *const *c, int *st) {
  (void)s; (void)c;
  assert(strstr(u, "/sync/playback/477001"));
  apagados++; if (st) *st = 204; return strdup("");
}
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }
int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
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

int main(int argc, char **argv) {
  static CatItem v[4];
  int esperadoT, esperadoE;
  modo = argc > 1 ? argv[1] : "mapped";
  assert(!strcmp(modo, "mapped") || !strcmp(modo, "normal") ||
         !strcmp(modo, "missing") || !strcmp(modo, "duplicate") || !strcmp(modo, "offline"));
  trakt_definir("synthetic-fixture-token", "synthetic-fixture-client");
  assert(trakt_continuar(v, 4) == 1);
  esperadoT = !strcmp(modo, "mapped") || !strcmp(modo, "normal") ? 2 : 1;
  esperadoE = esperadoT == 2 ? 2 : 14;
  printf("#512 mode=%s actual=S%02dE%02d expected=S%02dE%02d progress=%d metaGets=%d\n",
         modo, v[0].temporada, v[0].episodio, esperadoT, esperadoE, v[0].progresso, metaGets);
  fflush(stdout);
  assert(v[0].progresso == 40);
  assert(v[0].temporada == esperadoT && v[0].episodio == esperadoE);
  { char esperado[64];
    snprintf(esperado, sizeof esperado, "tt21975436:%d:%d", esperadoT, esperadoE);
    assert(!strcmp(v[0].imdb, esperado)); }
  if (!strcmp(modo, "mapped")) {
    CatEp eps[3] = {{0}};
    CatItem concluido = v[0];
    ProxSugestao prox;
    for (int i = 0; i < 3; i++) { eps[i].temporada = 2; eps[i].episodio = i + 1; }
    concluido.progresso = 100;
    assert(prox_seguinte(&concluido, eps, 3, 90, 1800000000000LL, &prox));
    assert(prox.temporada == 2 && prox.episodio == 3);
  }
  // Translating a visible card must retain the separate playback-record ID.
  assert(trakt_playback_remover(v[0].imdb) == 1 && apagados == 1);
  puts("trakt_episode_identity_512: PASS");
  return 0;
}
