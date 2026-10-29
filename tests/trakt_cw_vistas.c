// Issue traktcw (log PRPMM8): serie vista so no Trakt (fora das ultimas 100
// reproducoes) tem de dar "a seguir" no Continuar a Assistir, e serie toda vista
// tem de ganhar o selo no cartaz sem abrir o titulo. Rede falsa, sem rede real.
#include "trakt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback")) return strdup("[]");
  if (strstr(url, "/sync/history"))   // 1 reproducao recente, de outra serie
    return strdup("[{\"watched_at\":\"2026-03-01T10:00:00.000Z\",\"type\":\"episode\","
      "\"episode\":{\"season\":1,\"number\":1,\"title\":\"E\"},"
      "\"show\":{\"title\":\"Recente\",\"ids\":{\"imdb\":\"tt1000001\"}}}]");
  if (strstr(url, "/sync/last_activities"))
    return strdup("{\"movies\":{\"watched_at\":\"2026-03-01T00:00:00.000Z\"}}");
  if (strstr(url, "/progress/watched")) {
    if (strstr(url, "tt3000003") || strstr(url, "tt4000"))   // tudo visto, sem proximo
      return strdup("{\"aired\":3,\"completed\":3,\"next_episode\":null,\"seasons\":[]}");
    if (strstr(url, "tt2000002"))
      return strdup("{\"aired\":10,\"completed\":4,\"seasons\":[],"
        "\"next_episode\":{\"season\":1,\"number\":2,\"title\":\"x\"}}");
    return strdup("{\"aired\":5,\"completed\":1,\"seasons\":[],"
      "\"next_episode\":{\"season\":1,\"number\":2,\"title\":\"x\"}}");
  }
  return NULL;
}
char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "/meta/")) {
    // #595: Trakt confirms E2; an unavailable/stale metadata service must
    // neither remove it nor silently advance it to E3.
    if (strstr(url, "tt2000002")) {
      const char *mode = getenv("TRAKT_META");
      if (mode && !strcmp(mode, "missing")) return NULL;
      if (mode && !strcmp(mode, "stale"))
        return strdup("{\"name\":\"So Trakt\",\"videos\":[{\"season\":1,\"episode\":3}]}");
    }
    const char *t = strchr(strstr(url, "/meta/") + 6, '/');
    char id[24] = "", *o = malloc(600);
    if (t) snprintf(id, sizeof id, "%.9s", t + 1);
    snprintf(o, 600, "{\"imdbRating\":\"7.9\",\"name\":\"x\",\"description\":\"d\",\"runtime\":\"40 min\","
      "\"videos\":[{\"id\":\"%s:1:2\",\"season\":1,\"episode\":2,\"name\":\"e\",\"released\":\"2026-01-01T00:00:00.000Z\"}]}", id);
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
  if (st) *st = 200;
  if (strstr(u, "/sync/watched/shows")) {
    if (strstr(u, "page=1"))
      return strdup("[{\"last_watched_at\":\"2026-02-10T10:00:00.000Z\","
        "\"show\":{\"title\":\"So Trakt\",\"ids\":{\"imdb\":\"tt2000002\"}}},"
        "{\"last_watched_at\":\"2026-02-01T10:00:00.000Z\","
        "\"show\":{\"title\":\"Toda Vista\",\"ids\":{\"imdb\":\"tt3000003\"}}}]");
    if (strstr(u, "page=2")) {
      char *r = calloc(1, 16000);
      strcat(r, "[");
      for (int i = 0; i < 70; i++) {
        char item[180];
        snprintf(item, sizeof item, "%s{\"show\":{\"ids\":{\"imdb\":\"tt400%04d\"}}}", i ? "," : "", i);
        strcat(r, item);
      }
      strcat(r, "]");
      return r;
    }
    return strdup("[]");
  }
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
static int proximo = 1;
int ajustes_cw_proximo(void) { return proximo; }

unsigned long long cat_historico_geracao(void) { return 1; }
static char marcados[256][24];
static int marcadoVisto[256], nMarcados;
int cat_historico_definir_se_geracao(const char *id, const char *tipo, int visto,
                                     unsigned long long g) {
  (void)g;
  if (tipo && !strcmp(tipo, "series") && nMarcados < 256) {
    snprintf(marcados[nMarcados], sizeof marcados[0], "%s", id);
    marcadoVisto[nMarcados++] = visto;
  }
  return 1;
}
static int marca(const char *id) {   // -1 nunca escrito, senao 0/1 (ultima escrita)
  int i, r = -1;
  for (i = 0; i < nMarcados; i++) if (!strcmp(marcados[i], id)) r = marcadoVisto[i];
  return r;
}

int main(void) {
  static CatItem v[12];
  int n, i, achou = 0;
  trakt_definir("token", "client");
  n = trakt_continuar(v, 12);
  for (i = 0; i < n; i++) if (!strcmp(v[i].imdb, "tt2000002:1:2")) achou = 1;
  printf("[trakt-cw-vistas] continuar=%d so-trakt=%d marca(toda)=%d marca(so-trakt)=%d\n",
         n, achou, marca("tt3000003"), marca("tt2000002"));
  assert(achou);                       // Bug A: "a seguir" de serie fora do historico
  assert(marca("tt3000003") == 1);     // Bug B: selo sem abrir o titulo
  assert(marca("tt2000002") == 0);     // incompleta nao ganha selo
  assert(marca("tt4000069") == 1);     // selo alem das 64 candidatas antigas
  proximo = 0;
  nMarcados = 0;
  n = trakt_continuar(v, 12);
  assert(n == 0);
  assert(marca("tt3000003") == 1);
  assert(marca("tt4000069") == 1);
  puts("trakt_cw_vistas: PASS");
  return 0;
}
