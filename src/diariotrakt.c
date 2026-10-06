// O TRAKT COMO FONTE DO DIARIO (diariotrakt.h).
#include "diariotrakt.h"
#include "js.h"
#include "perfis.h"
#include "rede.h"
#include "trakt.h"
#include "dados.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VOLTA_S       (6 * 3600)
#define PAGINAS_MAX   20
#define LOTE_TRAKT    4096

// Bloco "<nome>":{...} DENTRO de [p, f). NULL se nao ha.
static const char *bloco(const char *p, const char *f, const char *nome, const char **fim) {
  char k[24];
  const char *b, *a;
  snprintf(k, sizeof k, "\"%s\"", nome);
  b = strstr(p, k);
  if (!b || b >= f) return NULL;
  a = strchr(b + strlen(k), '{');
  if (!a || a >= f) return NULL;
  *fim = js_fim(a);
  return *fim ? a : NULL;
}

static void idsDe(const char *o, const char *fo, char *imdb, size_t n, int *tmdb) {
  const char *fi, *ids = bloco(o, fo, "ids", &fi);
  imdb[0] = 0; *tmdb = 0;
  if (!ids) return;
  js_texto(ids, fi, "imdb", imdb, n);
  *tmdb = (int)js_num(ids, fi, "tmdb", 0);
}

static void nomeDe(const char *o, const char *fo, const char *imdb) {
  char t[160];
  if (imdb[0] && js_texto(o, fo, "title", t, sizeof t) && t[0])
    diario_nome_definir(imdb, t, (int)js_num(o, fo, "year", 0), NULL);
}

int diariotrakt_ler_historico(const char *json, DiarioEntrada *v, int max, long long *maisNovo) {
  const char *p = json ? js_raiz_array(json) : NULL;
  int n = 0;
  if (maisNovo) *maisNovo = 0;
  for (; p && *p == '{' && n < max; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *o, *fo, *ep, *fe;
    char quando[40], imdb[24];
    int tmdb;
    DiarioEntrada *d = &v[n];
    if (!f) break;
    memset(d, 0, sizeof *d);
    if (!js_texto(p, f, "watched_at", quando, sizeof quando)) continue;
    d->quando = js_ms_iso(quando) / 1000;
    if (d->quando <= 0) continue;
    ep = bloco(p, f, "episode", &fe);
    if (ep) {
      o = bloco(p, f, "show", &fo);
      if (!o) continue;
      d->temporada = (short)js_num(ep, fe, "season", 0);
      d->episodio = (short)js_num(ep, fe, "number", 0);
      d->tipo = DIARIO_SERIE;
    } else {
      o = bloco(p, f, "movie", &fo);
      if (!o) continue;
      d->tipo = DIARIO_FILME;
    }
    idsDe(o, fo, imdb, sizeof imdb, &tmdb);
    if (!imdb[0]) continue;                 // o app inteiro fala imdb
    snprintf(d->id, sizeof d->id, "%s", imdb);
    d->tmdb = d->tipo == DIARIO_FILME ? tmdb : 0;   // tmdb da serie nao e o do filme
    d->origem = DIARIO_ORIG_TRAKT;
    d->hora = -1;
    nomeDe(o, fo, imdb);
    if (maisNovo && d->quando > *maisNovo) *maisNovo = d->quando;
    n++;
  }
  return n;
}

int diariotrakt_ler_vistos(const char *json, int series, DiarioEntrada *v, int max) {
  const char *p = json ? js_raiz_array(json) : NULL;
  int n = 0;
  for (; p && *p == '{' && n < max; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *o, *fo;
    char imdb[24], quando[40];
    int tmdb;
    if (!f) break;
    o = bloco(p, f, series ? "show" : "movie", &fo);
    if (!o) continue;
    idsDe(o, fo, imdb, sizeof imdb, &tmdb);
    if (!imdb[0]) continue;
    nomeDe(o, fo, imdb);
    if (!series) {
      DiarioEntrada *d = &v[n];
      if (!js_texto(p, f, "last_watched_at", quando, sizeof quando)) continue;
      memset(d, 0, sizeof *d);
      snprintf(d->id, sizeof d->id, "%s", imdb);
      d->tipo = DIARIO_FILME; d->tmdb = tmdb; d->origem = DIARIO_ORIG_TRAKT; d->hora = -1;
      d->quando = js_ms_iso(quando) / 1000;
      if (d->quando > 0) n++;
    } else {
      const char *temps = js_array(p, f, "seasons"), *s;
      for (s = temps; s && *s == '{' && n < max; s = js_prox(js_fim(s))) {
        const char *fs = js_fim(s), *eps, *e;
        int t;
        if (!fs) break;
        t = (int)js_num(s, fs, "number", 0);
        eps = js_array(s, fs, "episodes");
        for (e = eps; e && *e == '{' && n < max; e = js_prox(js_fim(e))) {
          const char *fe = js_fim(e);
          DiarioEntrada *d = &v[n];
          if (!fe) break;
          if (!js_texto(e, fe, "last_watched_at", quando, sizeof quando)) continue;
          memset(d, 0, sizeof *d);
          snprintf(d->id, sizeof d->id, "%s", imdb);
          d->tipo = DIARIO_SERIE; d->origem = DIARIO_ORIG_TRAKT; d->hora = -1;
          d->temporada = (short)t; d->episodio = (short)js_num(e, fe, "number", 0);
          d->quando = js_ms_iso(quando) / 1000;
          if (d->quando > 0) n++;
        }
      }
    }
  }
  return n;
}

// ------------------------------------------------------------------ fio

static _Atomic int vivo;
static long long proxima[CONTA_PERFIL_MAX + 1];

typedef struct { int perfil; long long cursor; } Volta;

static long long lerCursor(int p) {
  char n[40], *b;
  long long c = 0;
  snprintf(n, sizeof n, "diario-trakt-p%d.txt", p);
  b = dados_ler(n);
  if (b) { c = atoll(b); free(b); }
  return c;
}

static void gravarCursor(int p, long long c) {
  char n[40], v[32];
  snprintf(n, sizeof n, "diario-trakt-p%d.txt", p);
  snprintf(v, sizeof v, "%lld\n", c);
  dados_gravar(n, v);
}

static void *fio(void *u) {
  Volta *vt = u;
  const char *cab[4];
  char aut[400], chave[160], url[320], inicio[40];
  DiarioEntrada *v = calloc(LOTE_TRAKT, sizeof *v);
  long long novo = vt->cursor;
  int pag, n, total = 0;
  if (!v || !trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) goto fim;
  // 1. Completude: so na primeira volta (sem cursor). Depois o historico basta.
  if (!vt->cursor) {
    char *c = rede_baixar_com("https://api.trakt.tv/sync/watched/movies", 30, cab);
    if (c) { n = diariotrakt_ler_vistos(c, 0, v, LOTE_TRAKT); free(c); if (n > 0) total += diario_adicionar_lote(vt->perfil, v, n); }
    c = rede_baixar_com("https://api.trakt.tv/sync/watched/shows", 45, cab);
    if (c) { n = diariotrakt_ler_vistos(c, 1, v, LOTE_TRAKT); free(c); if (n > 0) total += diario_adicionar_lote(vt->perfil, v, n); }
  }
  // 2/3. Historico: do 1 de janeiro (primeira volta) ou do cursor.
  { long long desde = vt->cursor;
    struct tm tmv;
    time_t t;
    if (!desde) {
      time_t agora = time(NULL);
      localtime_r(&agora, &tmv);
      tmv.tm_mon = 0; tmv.tm_mday = 1; tmv.tm_hour = tmv.tm_min = tmv.tm_sec = 0; tmv.tm_isdst = -1;
      desde = (long long)mktime(&tmv);
    } else desde += 1;
    t = (time_t)desde;
    gmtime_r(&t, &tmv);
    strftime(inicio, sizeof inicio, "%Y-%m-%dT%H%%3A%M%%3A%SZ", &tmv); }
  for (pag = 1; pag <= PAGINAS_MAX; pag++) {
    long long mais = 0;
    char *c;
    snprintf(url, sizeof url, "https://api.trakt.tv/sync/history?start_at=%s&page=%d&limit=100", inicio, pag);
    c = rede_baixar_com(url, 30, cab);
    if (!c) break;
    n = diariotrakt_ler_historico(c, v, LOTE_TRAKT, &mais);
    free(c);
    if (n > 0) total += diario_adicionar_lote(vt->perfil, v, n);
    if (mais > novo) novo = mais;
    if (n < 100) break;
  }
  if (novo > vt->cursor) gravarCursor(vt->perfil, novo);
  else if (!vt->cursor) gravarCursor(vt->perfil, (long long)time(NULL) - 86400);
  printf("[diariotrakt] perfil %d: %d novas\n", vt->perfil, total);
  fflush(stdout);
fim:
  free(v);
  free(vt);
  atomic_store(&vivo, 0);
  return NULL;
}

void diariotrakt_passo(void) {
  long long agora;
  int p, z = 0;
  pthread_t t;
  Volta *vt;
  static long long arranque;
  if (!trakt_ativo() || atomic_load(&vivo)) return;
  p = perfis_ativo();
  if (p < 1 || p > CONTA_PERFIL_MAX) return;
  agora = (long long)time(NULL);
  // Nao disputa com a Home: a primeira volta so 90 s depois do arranque.
  if (!arranque) arranque = agora;
  if (agora - arranque < 90) return;
  if (agora < proxima[p]) return;
  proxima[p] = agora + VOLTA_S;
  vt = calloc(1, sizeof *vt);
  if (!vt) return;
  vt->perfil = p;
  vt->cursor = lerCursor(p);
  if (!atomic_compare_exchange_strong(&vivo, &z, 1)) { free(vt); return; }
  if (pthread_create(&t, NULL, fio, vt)) { atomic_store(&vivo, 0); free(vt); return; }
  pthread_detach(t);
}
