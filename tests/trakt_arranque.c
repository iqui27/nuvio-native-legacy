// B2 (arranque): tempo de parede de trakt_continuar + trakt_social contra uma rede
// FALSA com latencia por pedido (mesma de uma TV lenta), e o resultado tem de ser
// identico ao serial. Imprime os ms para comparar antes/depois.
#include "trakt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <time.h>
#include <unistd.h>
#include <pthread.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

extern int trakt_operacao_estado(int tipo);

static int lat(const char *k, int padrao) {
  const char *v = getenv(k);
  return v ? atoi(v) : padrao;
}
static void dorme(int ms) { usleep((useconds_t)ms * 1000); }
static double agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

#define N_SERIES 21
static char *historico(void) {
  char *o = malloc(20000), *p = o;
  int i;
  p += sprintf(p, "[");
  for (i = 0; i < N_SERIES; i++)
    p += sprintf(p, "%s{\"watched_at\":\"2026-03-%02dT10:00:00.000Z\",\"type\":\"episode\","
      "\"episode\":{\"season\":1,\"number\":1,\"title\":\"E\"},"
      "\"show\":{\"title\":\"S%d\",\"ids\":{\"imdb\":\"tt%07d\"}}}", i ? "," : "", 1 + i, i, 5000 + i);
  strcpy(p, "]");
  return o;
}

char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback")) {
    dorme(lat("L_PLAYBACK", 700));
    return strdup("[{\"id\":77,\"progress\":40,\"paused_at\":\"2026-01-01T00:00:00.000Z\","
      "\"type\":\"movie\",\"movie\":{\"title\":\"Outro\",\"year\":2019,\"runtime\":100,\"overview\":\"S.\","
      "\"ids\":{\"imdb\":\"tt7777777\"},\"images\":{\"poster\":[\"x.test/p2.jpg\"],\"fanart\":[\"x.test/f2.jpg\"]}}}]");
  }
  if (strstr(url, "/sync/history")) { dorme(lat("L_HISTORICO", 700)); return historico(); }
  if (strstr(url, "/sync/last_activities")) {
    dorme(lat("L_ATIV", 300));
    return strdup("{\"movies\":{\"watched_at\":\"2026-03-01T00:00:00.000Z\"}}");
  }
  if (strstr(url, "/progress/watched")) {
    dorme(lat("L_PROX", 600));
    return strdup("{\"next_episode\":{\"season\":1,\"number\":2,\"title\":\"x\"}}");
  }
  if (strstr(url, "/friends/activities") || strstr(url, "/following/activities")) {
    dorme(lat("L_SOCIAL", 400));
    return strdup("[{\"action\":\"watch\",\"user\":{\"username\":\"ana\",\"name\":\"Ana\"},"
      "\"movie\":{\"title\":\"Amigo\",\"year\":2019,\"ids\":{\"imdb\":\"tt8888888\"},"
      "\"images\":{\"poster\":[\"x.test/p3.jpg\"],\"fanart\":[\"x.test/f3.jpg\"]},\"overview\":\"S.\",\"runtime\":90}}]");
  }
  return NULL;
}
char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "cinemeta") || strstr(url, "/meta/")) {
    const char *t = strstr(url, "/meta/");
    char id[24] = "", *o = malloc(600);
    if (t) { t = strchr(t + 6, '/'); if (t) { t++; snprintf(id, sizeof id, "%.9s", t); } }
    dorme(lat("L_CINEMETA", 250));
    snprintf(o, 600, "{\"imdbRating\":\"7.9\",\"name\":\"x\",\"description\":\"d\",\"runtime\":\"40 min\","
      "\"videos\":[{\"id\":\"%s:1:2\",\"name\":\"e\",\"released\":\"2026-01-01T00:00:00.000Z\"}]}", id);
    return o;
  }
  return NULL;
}
char *rede_apagar(const char *url, int s, const char *const *cab, int *st) {
  (void)url; (void)s; (void)cab; if (st) *st = 204; return strdup("");
}
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)s; (void)c;
  if (strstr(u, "/sync/watched/movies")) {
    int pg = atoi(strstr(u, "page=") + 5);
    dorme(lat("L_FILMES", 300));
    if (st) *st = 200;
    if (pg == 1) return strdup("[{\"movie\":{\"title\":\"A\",\"ids\":{\"imdb\":\"tt1111111\"}}}]");
    if (pg == 2) return strdup("[{\"movie\":{\"title\":\"B\",\"ids\":{\"imdb\":\"tt2222222\"}}}]");
    return strdup("[]");
  }
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
static int nHist;
int cat_historico_definir_se_geracao(const char *a, const char *b, int c, unsigned long long g) {
  (void)a; (void)b; (void)c; (void)g; nHist++; return 1;
}

int main(void) {
  static CatItem v[12], s[8];
  int n, ns, i;
  double t0, t1, t2;
  trakt_definir("token", "client");
  t0 = agoraMs();
  n = trakt_continuar(v, 12);
  t1 = agoraMs();
  ns = trakt_social(s, 8);
  t2 = agoraMs();
  printf("[arranque-trakt] continuar=%d itens em %.0f ms, social=%d em %.0f ms\n",
         n, t1 - t0, ns, t2 - t1);
  // 1 pausado + 21 "a seguir" de series diferentes, cortado em 12 (mais recentes).
  { unsigned h = 5381;
    for (i = 0; i < n; i++) { const char *c = v[i].imdb; while (*c) h = h * 33 + (unsigned char)*c++; }
    printf("[arranque-trakt] n=%d hash=%u\n", n, h); }
  assert(ns == 1 && !strcmp(s[0].imdb, "tt8888888"));
  // Pausado preservado primeiro; sugestoes por recencia nos lugares restantes.
  assert(n == 12 && !strcmp(v[0].imdb, "tt7777777") && v[0].progresso == 40);
  for (i = 2; i < n; i++) assert(v[i - 1].retomadoMs >= v[i].retomadoMs);
  (void)0;
  assert(nHist >= 2);          // historico + filmes vistos chegaram ao mapa
  { double a = agoraMs(); int m;
    static CatItem v2[12];
    m = trakt_continuar(v2, 12);
    printf("[arranque-trakt] 2a volta (memoria): %d itens em %.0f ms\n", m, agoraMs() - a);
    assert(m == n); }
  puts("trakt_arranque: PASS");
  return 0;
}
