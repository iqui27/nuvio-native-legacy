#include "metaep.h"
#include "js.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

int metaep_temporada(const char *video, const char *fim) {
  int t = (int)js_num(video, fim, "season", -1);
  if (t < 0 && js_num(video, fim, "episode", -1) > 0) t = 1;
  return t;
}

static void id_num_ou_texto(const char *ini, const char *fim, const char *chave,
                            char *dst, size_t n) {
  double v;
  if (js_texto(ini, fim, chave, dst, n) && dst[0]) return;
  if (!js_tem(ini, fim, chave)) return;
  v = js_num(ini, fim, chave, -1);
  if (v <= 0 || v != (double)(long long)v) return;
  snprintf(dst, n, "%.0f", v);
}

static void ids_do_video(const char *video, const char *fim, MetaEpInfo *out) {
  const char *ids = strstr(video, "\"ids\"");
  const char *ini;
  const char *fids;
  char direto[sizeof out->tvdb] = "", aninhado[sizeof out->tvdb] = "";
  id_num_ou_texto(video, fim, "tvdb_id", direto, sizeof direto);
  if (ids && ids < fim) {
    ini = strchr(ids, '{');
    if (ini && ini < fim) {
      fids = js_fim(ini);
      if (fids && fids <= fim) {
        id_num_ou_texto(ini, fids, "tvdb", aninhado, sizeof aninhado);
        id_num_ou_texto(ini, fids, "tmdb", out->tmdb, sizeof out->tmdb);
        id_num_ou_texto(ini, fids, "trakt", out->trakt, sizeof out->trakt);
      }
    }
  }
  // Two disagreeing declarations are not proof of either episode.
  if (direto[0] && aninhado[0] && strcmp(direto, aninhado)) return;
  snprintf(out->tvdb, sizeof out->tvdb, "%s", direto[0] ? direto : aninhado);
}

static int id_inteiro(const char *s) {
  unsigned long long v = 0;
  if (!s || *s < '1' || *s > '9') return 0;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return 0;
    v = v * 10 + (unsigned)(*s - '0');
    if (v > INT_MAX) return 0;
  }
  return (int)v;
}

int metaep_tvdb_video(const char *video, const char *fim) {
  MetaEpInfo info;
  if (!video) return 0;
  if (!fim) fim = video + strlen(video);
  memset(&info, 0, sizeof info);
  ids_do_video(video, fim, &info);
  return id_inteiro(info.tvdb);
}

int metaep_buscar(const char *corpo, int t, int e, MetaEpInfo *out) {
  const char *p;
  MetaEpInfo achado;
  int encontrou = 0;
  unsigned conflito = 0;
  if (!corpo || t < 0 || e <= 0 || js_videos_temporadas_falsas(corpo)) return 0;
  memset(&achado, 0, sizeof achado);
  p = js_array(corpo, NULL, "videos");
  while (p) {
    const char *f = js_fim(p);
    int tv = metaep_temporada(p, f);
    int ev = (int)js_num(p, f, "episode", 0);
    if (tv == t && ev == e) {
      MetaEpInfo atual;
      memset(&atual, 0, sizeof atual);
      atual.temporada = tv;
      atual.episodio = ev;
      ids_do_video(p, f, &atual);
      if (!encontrou) achado = atual;
      else {
#define MESCLAR_ID(campo, bit) do { \
        if (!(conflito & (bit)) && achado.campo[0] && atual.campo[0] && \
            strcmp(achado.campo, atual.campo)) { \
          achado.campo[0] = 0; conflito |= (bit); \
        } else if (!(conflito & (bit)) && !achado.campo[0] && atual.campo[0]) \
          snprintf(achado.campo, sizeof achado.campo, "%s", atual.campo); \
      } while (0)
        MESCLAR_ID(tvdb, 1u); MESCLAR_ID(tmdb, 2u); MESCLAR_ID(trakt, 4u);
#undef MESCLAR_ID
      }
      encontrou = 1;
    }
    p = js_prox(f);
  }
  if (encontrou && out) *out = achado;
  return encontrou;
}

int metaep_buscar_tvdb(const char *corpo, int tvdb, MetaEpInfo *out) {
  const char *p;
  MetaEpInfo achado;
  int encontrou = 0;
  if (!corpo || tvdb <= 0 || js_videos_temporadas_falsas(corpo)) return 0;
  memset(&achado, 0, sizeof achado);
  p = js_array(corpo, NULL, "videos");
  while (p) {
    const char *f = js_fim(p);
    if (metaep_tvdb_video(p, f) == tvdb) {
      MetaEpInfo atual;
      memset(&atual, 0, sizeof atual);
      atual.temporada = metaep_temporada(p, f);
      atual.episodio = (int)js_num(p, f, "episode", 0);
      if (atual.temporada < 0 || atual.episodio <= 0) return 0;
      ids_do_video(p, f, &atual);
      if (encontrou && (achado.temporada != atual.temporada ||
                        achado.episodio != atual.episodio)) return 0;
      achado = atual;
      encontrou = 1;
    }
    p = js_prox(f);
  }
  if (encontrou) {
    MetaEpInfo porPar;
    if (!metaep_buscar(corpo, achado.temporada, achado.episodio, &porPar) ||
        id_inteiro(porPar.tvdb) != tvdb) return 0;
    achado = porPar;
  }
  if (encontrou && out) *out = achado;
  return encontrou;
}

int metaep_existe(const char *corpo, int t, int e) {
  return metaep_buscar(corpo, t, e, NULL);
}

static const char *video_por_se(const char *corpo, int t, int e,
                                const char **fim) {
  const char *p = js_videos_temporadas_falsas(corpo) ? NULL : js_array(corpo, NULL, "videos");
  while (p) {
    const char *f = js_fim(p);
    int tv = metaep_temporada(p, f);
    int ev = (int)js_num(p, f, "episode", 0);
    if (tv == t && ev == e) {
      if (fim) *fim = f;
      return p;
    }
    p = js_prox(f);
  }
  if (fim) *fim = NULL;
  return NULL;
}

int metaep_nome(const char *corpo, int t, int e, char *dst, size_t n) {
  const char *f;
  const char *p = video_por_se(corpo, t, e, &f);
  if (!p || !dst || !n) return 0;
  dst[0] = 0;
  if (js_texto(p, f, "name", dst, n)) return 1;
  return js_texto(p, f, "title", dst, n);
}

int metaep_lancamento(const char *corpo, int t, int e, char *dst, size_t n) {
  const char *f;
  const char *p = video_por_se(corpo, t, e, &f);
  if (!p || !dst || !n) return 0;
  dst[0] = 0;
  if (js_texto(p, f, "released", dst, n)) return 1;
  return js_texto(p, f, "firstAired", dst, n);
}

int metaep_proximo(const char *corpo, int t, int e, MetaEpInfo *out) {
  const char *p;
  int melhorT = 0, melhorE = 0;
  MetaEpInfo melhor;
  if (!corpo || t < 0 || e <= 0 || js_videos_temporadas_falsas(corpo)) return 0;
  memset(&melhor, 0, sizeof melhor);
  p = js_array(corpo, NULL, "videos");
  while (p) {
    const char *f = js_fim(p);
    int tv = metaep_temporada(p, f);
    int ev = (int)js_num(p, f, "episode", 0);
    if (tv < 0 || ev <= 0) { p = js_prox(f); continue; }
    if (tv < t || (tv == t && ev <= e)) { p = js_prox(f); continue; }
    if (!melhorE || tv < melhorT || (tv == melhorT && ev < melhorE)) {
      melhorT = tv; melhorE = ev;
      memset(&melhor, 0, sizeof melhor);
      melhor.temporada = tv;
      melhor.episodio = ev;
      ids_do_video(p, f, &melhor);
    }
    p = js_prox(f);
  }
  if (!melhorE) return 0;
  if (out) *out = melhor;
  return 1;
}

void metaep_scrobble_corpo(char *dst, size_t n, const char *showImdb,
                           int t, int e, const MetaEpInfo *ep, double pct) {
  if (!dst || !n) return;
  if (pct < 0.0) pct = 0.0;
  if (pct > 100.0) pct = 100.0;
  if (!showImdb || !showImdb[0]) { dst[0] = 0; return; }
  if (t >= 0 && e > 0) {
    if (ep && (ep->tvdb[0] || ep->trakt[0])) {
      char ids[220] = "";
      // These IDs identify the episode, not the show. Trakt's episode
      // contract supports TVDB/Trakt; TMDB-only metadata uses the pair below.
      if (ep->tvdb[0]) {
        size_t u = strlen(ids);
        snprintf(ids + u, sizeof ids - u, "%s\"tvdb\":%s",
                 ids[0] ? "," : "", ep->tvdb);
      }
      if (ep->trakt[0]) {
        size_t u = strlen(ids);
        snprintf(ids + u, sizeof ids - u, "%s\"trakt\":%s",
                 ids[0] ? "," : "", ep->trakt);
      }
      snprintf(dst, n,
               "{\"episode\":{\"ids\":{%s}},\"progress\":%.2f}",
               ids, pct);
      return;
    }
    snprintf(dst, n,
             "{\"show\":{\"ids\":{\"imdb\":\"%s\"}},"
             "\"episode\":{\"season\":%d,\"number\":%d},\"progress\":%.2f}",
             showImdb, t, e, pct);
    return;
  }
  snprintf(dst, n, "{\"movie\":{\"ids\":{\"imdb\":\"%s\"}},\"progress\":%.2f}",
           showImdb, pct);
}
