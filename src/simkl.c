// Simkl como fonte da retomada e destino do "+". Ver simkl.h: la estao as
// rotas (todas conferidas na documentacao oficial), o que ficou de fora e o
// modelo de fios.
#include "simkl.h"
#include "perfisessao.h"
#include "ajustes.h"
#include "simklauth.h"
#include "nuvem.h"
#include "rede.h"
#include "js.h"
#include "idbase.h"
#include "idioma.h"
#include "trakt.h"
#include "traktscrobble.h"
#include "artemetahub.h"
#include "dados.h"
#include "fichameta.h"
#include "metaprov.h"
#include "metaep.h"
#include "vistoep.h"
#include "simklbib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include "provedorhttp.h"

// Optional imports belong to consumers: public declarations keep producers strong.
extern int trakt_enfeitar_lote_sessao(CatItem *, int, uint64_t) NV_PERFIL_OPCIONAL;

#define SMK_API "https://api.simkl.com"
// Retomada: o mesmo teto da fileira (CONT_MAX de descoberta.c) com folga para
// o "a seguir" disputar lugar com os pausados.
#define SMK_PLAY_MAX 64
// Plan to Watch conhecido, para a guarda do "-". 400 ids x 24 B = ~10 KB; a
// lista de "quero ver" de uma pessoa real cabe com sobra.
#define SMK_PTW_MAX 400

typedef struct {
  long simkl;
  char imdb[24];
} SimklAnimeId;

static SimklAnimeId simklAnimeIds[SMK_PLAY_MAX];
static int simklAnimeIdProximo;

int simkl_ativo(void) { return simklauth_token()[0] != 0; }
int simkl_aguardando(void) { return provedorEspera(); }

const char *simkl_aviso_sem_vinculo(int querSimkl) {
  return (querSimkl && !simkl_ativo()) ? SIMKL_VINCULE : NULL;
}

// ------------------------------------------------------------ JSON por nivel

// Valor da chave `chave` na PROFUNDIDADE 1 do primeiro objeto de [ini,fim).
// js_texto para no primeiro "imdb" da faixa inteira; num item de playback de
// serie o bloco "episode" pode vir antes do "show", e num item de all-items o
// "show" e irmao de campos como "last_watched". Ler por nivel e o que garante
// que o titulo e o imdb sao DA OBRA. *vi/*vf delimitam o valor cru.
static int valorRaiz(const char *ini, const char *fim, const char *chave,
                     const char **vi, const char **vf) {
  const char *p = ini;
  size_t n = strlen(chave);
  int prof = 0;
  if (!p) return 0;
  if (!fim) fim = p + strlen(p);
  while (p < fim && *p != '{') p++;
  for (; p < fim && *p; p++) {
    if (*p == '"') {
      const char *a = p + 1, *q = a;
      while (q < fim && *q && *q != '"') q += (*q == '\\' && q[1]) ? 2 : 1;
      if (prof == 1 && (size_t)(q - a) == n && !strncmp(a, chave, n)) {
        const char *v = q + 1;
        while (v < fim && (unsigned char)*v <= ' ') v++;
        if (v < fim && *v == ':') {
          for (v++; v < fim && (unsigned char)*v <= ' '; v++) ;
          *vi = v;
          if (*v == '{' || *v == '[') *vf = js_fim(v);
          else if (*v == '"') {
            const char *r = v + 1;
            while (r < fim && *r && *r != '"') r += (*r == '\\' && r[1]) ? 2 : 1;
            *vf = r < fim ? r + 1 : fim;
          } else {
            const char *r = v;
            while (r < fim && *r != ',' && *r != '}' && *r != ']') r++;
            *vf = r;
          }
          return *vf > *vi;
        }
      }
      if (q >= fim || !*q) break;
      p = q;
      continue;
    }
    if (*p == '{' || *p == '[') prof++;
    else if ((*p == '}' || *p == ']') && --prof == 0) break;
  }
  return 0;
}

// O objeto `chave` da raiz ("show", "movie", "episode"). 0 quando falta ou e
// null — all-items manda "next_to_watch": null e playback pode nao ter
// "episode".
static int objetoRaiz(const char *ini, const char *fim, const char *chave,
                      const char **oi, const char **of) {
  return valorRaiz(ini, fim, chave, oi, of) && **oi == '{';
}

// Titulo, ano e ids.imdb de um objeto de obra (show/movie/anime). 0 sem imdb.
static int lerObra(const char *oi, const char *of, CatItem *d, char *imdb,
                   size_t nImdb) {
  const char *ii, *ifim;
  imdb[0] = 0;
  js_texto_raiz_em(oi, of, "title", d->titulo, sizeof d->titulo);
  if (objetoRaiz(oi, of, "ids", &ii, &ifim))
    js_texto(ii, ifim, "imdb", imdb, nImdb);
  // Um id que nao e "tt..." nao serve a ninguem aqui: metahub, Cinemeta e os
  // addons indexam pelo IMDb. Recusar na leitura evita um card sem arte.
  if (imdb[0] != 't' || imdb[1] != 't') { imdb[0] = 0; return 0; }
  { int ano = (int)js_num(oi, of, "year", 0.0);
    if (ano > 1800) snprintf(d->meta, sizeof d->meta, "%d", ano); }
  return 1;
}

// ------------------------------------------------------------ leitores puros

// `simklIds` (may be NULL) gets each kept row's title Simkl ID, 0 when absent.
static int lerPlayback(const char *json, CatItem *dst, long long *ids, long *simklIds,
                       int max) {
  const char *p;
  int n = 0;
  if (!json || !dst) return 0;
  for (p = js_raiz_array(json); p && *p == '{' && n < max; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *oi, *of, *ei = NULL, *ef = NULL, *vi, *vf;
    CatItem *d = &dst[n];
    char imdb[24], quando[40] = "";
    int serie = 0, anime = 0;
    memset(d, 0, sizeof *d);
    if (objetoRaiz(p, f, "movie", &oi, &of)) serie = 0;
    else if (objetoRaiz(p, f, "show", &oi, &of)) serie = 1;
    else if (objetoRaiz(p, f, "anime", &oi, &of)) serie = anime = 1;
    else continue;
    if (!lerObra(oi, of, d, imdb, sizeof imdb)) continue;
    if (serie) {
      int t = 0, e = 0;
      if (!objetoRaiz(p, f, "episode", &ei, &ef)) continue;
      // ANIME SO COM A NUMERACAO DO TVDB. A doc: "Anime episodes return both
      // AniDB episode numbers (season, number) and TVDB numbers (tvdb_season,
      // tvdb_number)". O resto do app fala TVDB; o AniDB poria o card no
      // episodio errado — pior que nao mostrar.
      t = (int)js_num(ei, ef, "tvdb_season", -1.0);
      e = (int)js_num(ei, ef, "tvdb_number", 0.0);
      if (!anime && (t < 0 || e <= 0)) {
        t = (int)js_num(ei, ef, "season", -1.0);
        e = (int)js_num(ei, ef, "episode", 0.0);
        if (e <= 0) e = (int)js_num(ei, ef, "number", 0.0);
      }
      if (t < 0 || e <= 0) continue;
      d->temporada = t;
      d->episodio = e;
      js_texto_raiz_em(ei, ef, "title", d->nomeEpisodio, sizeof d->nomeEpisodio);
      snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", imdb, t, e);
      snprintf(d->tipo, sizeof d->tipo, "series");
    } else {
      snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
      snprintf(d->tipo, sizeof d->tipo, "movie");
    }
    // `progress` e porcentagem em float (0-100, "45.5"). Os limites de 1% a
    // 90% sao aplicados em montarContinuar, igual para as tres fontes.
    if (valorRaiz(p, f, "progress", &vi, &vf)) d->progresso = (int)atof(vi);
    if (d->progresso < ajustes_cw_minimo() || d->progresso >= ajustes_cw_concluido()) continue;
    if (js_texto_raiz_em(p, f, "paused_at", quando, sizeof quando))
      d->retomadoMs = js_ms_iso(quando);
    if (ids) {
      // O "id" da RAIZ e o do registro de playback — o que DELETE
      // /sync/playback/{id} pede. "64bit integer" na doc: atof guarda inteiro
      // exato ate 2^53, e o id do Simkl esta longe disso.
      ids[n] = valorRaiz(p, f, "id", &vi, &vf) ? (long long)atof(vi) : 0;
    }
    if (simklIds) {
      const char *ii, *ifim;
      simklIds[n] = objetoRaiz(oi, of, "ids", &ii, &ifim)
                      ? (long)js_num(ii, ifim, "simkl", 0) : 0;
    }
    n++;
  }
  return n;
}

int simkl_ler_playback(const char *json, CatItem *dst, long long *ids, int max) {
  return lerPlayback(json, dst, ids, NULL, max);
}

static int animeAbsoluto_global(const char *corpo, int absoluto, MetaEpInfo *ep) {
  const char *p;
  int temporada = 0, correspondencias = 0;
  int primeira = INT_MAX, anteriorMax = 0;
  if (!corpo || absoluto <= 0 || js_videos_temporadas_falsas(corpo)) return 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    int t = metaep_temporada(p, f);
    int e = (int)js_num(p, f, "episode", 0);
    if (t > 0 && e == absoluto) {
      if (temporada && temporada != t) return 0;
      temporada = t;
      correspondencias++;
    }
  }
  if (correspondencias != 1 || temporada <= 1) return 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    int t = metaep_temporada(p, f);
    int e = (int)js_num(p, f, "episode", 0);
    if (t == temporada && e > 0) {
      if (e < primeira) primeira = e;
    } else if (t == temporada - 1 && e > anteriorMax) {
      anteriorMax = e;
    }
  }
  if (primeira <= 1 || anteriorMax != primeira - 1) return 0;
  // The catalog already identifies this episode by its absolute number.
  ep->temporada = temporada;
  ep->episodio = absoluto;
  return 1;
}

static int animeThumb_global(const char *corpo, int absoluto, char *thumb, size_t n) {
  const char *p;
  int encontrados = 0;
  if (thumb && n) thumb[0] = 0;
  if (!corpo || absoluto <= 0 || !thumb || n == 0) return 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if ((int)js_num(p, f, "episode", 0) != absoluto) continue;
    if (++encontrados != 1) return 0;
    if (!js_texto(p, f, "thumbnail", thumb, n) ||
        strncmp(thumb, "https://", 8) != 0) {
      thumb[0] = 0;
      return 0;
    }
  }
  return encontrados == 1 && thumb[0] != 0;
}

// The provider's episode for a Simkl absolute number; a catalog that already
// numbers it globally also gives its still.
static int animeAbsoluto_corpo(const char *corpo, int absoluto, MetaEpInfo *ep,
                               char *thumb, size_t thumbTam) {
  if (animeAbsoluto_global(corpo, absoluto, ep)) {
    animeThumb_global(corpo, absoluto, thumb, thumbTam);
    return 1;
  }
  int ok = metaep_buscar(corpo, 1, 1, ep);
  // ponytail: repeated scans reuse provider ordering; index if long shows cost time.
  for (int i = 1; ok && i < absoluto; i++) {
    MetaEpInfo prox;
    ok = metaep_proximo(corpo, ep->temporada, ep->episodio, &prox) &&
         ((prox.temporada == ep->temporada && prox.episodio == ep->episodio + 1) ||
          (prox.temporada == ep->temporada + 1 && prox.episodio == 1));
    if (ok) *ep = prox;
  }
  return ok;
}

// JSON string body of `s` into `dst` (quotes and backslashes escaped,
// control characters dropped). 0 when it does not fit.
static int animeJsonTexto(char *dst, size_t n, const char *s) {
  size_t u = 0;
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 32) continue;
    if (c == '"' || c == '\\') { if (u + 2 >= n) return 0; dst[u++] = '\\'; }
    if (u + 1 >= n) return 0;
    dst[u++] = (char)c;
  }
  dst[u] = 0;
  return 1;
}

// Only what the absolute mapping reads from each video: season, episode,
// still and release date (js_videos_temporadas_falsas). One Piece's full
// metadata is 738 KB, over the disk cache's limit, so without this copy every
// Continue watching rebuild downloaded it again, and one failed download
// left the card with no episode. Its trimmed copy is about 125 KB.
static char *animeVideosEnxutos(const char *corpo) {
  size_t cap = 65536, u = 0;
  char *s = malloc(cap);
  const char *p;
  int primeiro = 1;
  if (!s) return NULL;
  u = (size_t)snprintf(s, cap, "{\"meta\":{\"videos\":[");
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char thumb[512] = "", quando[40] = "", tEsc[1100], qEsc[100], item[1400];
    size_t k = 0;
    int w;
    js_texto(p, f, "thumbnail", thumb, sizeof thumb);
    js_texto(p, f, "released", quando, sizeof quando);
    if (!animeJsonTexto(tEsc, sizeof tEsc, thumb)) tEsc[0] = 0;
    if (!animeJsonTexto(qEsc, sizeof qEsc, quando)) qEsc[0] = 0;
    item[k++] = '{';
    item[k] = 0;
    if (js_tem(p, f, "season"))
      k += (size_t)snprintf(item + k, sizeof item - k, "\"season\":%d,",
                            (int)js_num(p, f, "season", 0));
    if (js_tem(p, f, "episode"))
      k += (size_t)snprintf(item + k, sizeof item - k, "\"episode\":%d,",
                            (int)js_num(p, f, "episode", 0));
    if (tEsc[0])
      k += (size_t)snprintf(item + k, sizeof item - k, "\"thumbnail\":\"%s\",", tEsc);
    if (qEsc[0])
      k += (size_t)snprintf(item + k, sizeof item - k, "\"released\":\"%s\",", qEsc);
    if (k > 1) k--;   // trailing comma
    item[k++] = '}';
    item[k] = 0;
    if (u + k + 8 > cap) {
      char *mais = realloc(s, cap * 2);
      if (!mais) { free(s); return NULL; }
      s = mais;
      cap *= 2;
    }
    w = snprintf(s + u, cap - u, "%s%s", primeiro ? "" : ",", item);
    u += (size_t)w;
    primeiro = 0;
  }
  snprintf(s + u, cap - u, "]}}");
  return s;
}

// The last mapping that worked for each title this session, for the same
// Simkl number: a failed download keeps the card on that episode instead of
// showing the title alone.
#define SMK_ANIME_ABS_MEMO 16
typedef struct {
  char id[24];
  int absoluto, t, e;
  char thumb[512];
} AnimeAbsMemo;
static AnimeAbsMemo animeAbsMemo[SMK_ANIME_ABS_MEMO];
static int animeAbsMemoProx;
static pthread_mutex_t animeAbsTrava = PTHREAD_MUTEX_INITIALIZER;

static void animeAbsMemoGuardar(const char *id, int absoluto, int t, int e,
                                const char *thumb) {
  int i;
  pthread_mutex_lock(&animeAbsTrava);
  for (i = 0; i < SMK_ANIME_ABS_MEMO && strcmp(animeAbsMemo[i].id, id); i++) {}
  if (i == SMK_ANIME_ABS_MEMO) {
    i = animeAbsMemoProx;
    animeAbsMemoProx = (animeAbsMemoProx + 1) % SMK_ANIME_ABS_MEMO;
  }
  snprintf(animeAbsMemo[i].id, sizeof animeAbsMemo[i].id, "%s", id);
  animeAbsMemo[i].absoluto = absoluto;
  animeAbsMemo[i].t = t;
  animeAbsMemo[i].e = e;
  snprintf(animeAbsMemo[i].thumb, sizeof animeAbsMemo[i].thumb, "%s", thumb ? thumb : "");
  pthread_mutex_unlock(&animeAbsTrava);
}

static int animeAbsMemoLer(const char *id, int absoluto, int *t, int *e,
                           char *thumb, size_t thumbTam) {
  int ok = 0;
  pthread_mutex_lock(&animeAbsTrava);
  for (int i = 0; i < SMK_ANIME_ABS_MEMO; i++) {
    const AnimeAbsMemo *m = &animeAbsMemo[i];
    if (strcmp(m->id, id) || m->absoluto != absoluto) continue;
    *t = m->t;
    *e = m->e;
    if (thumb && thumbTam) snprintf(thumb, thumbTam, "%s", m->thumb);
    ok = 1;
    break;
  }
  pthread_mutex_unlock(&animeAbsTrava);
  return ok;
}

static void animeAbsMemoEsquecer(void) {
  pthread_mutex_lock(&animeAbsTrava);
  memset(animeAbsMemo, 0, sizeof animeAbsMemo);
  animeAbsMemoProx = 0;
  pthread_mutex_unlock(&animeAbsTrava);
}

// Reuse the provider metadata cache; an unknown mapping opens the title only.
static int animeAbsoluto(const char *id, int absoluto, int *t, int *e,
                         char *thumb, size_t thumbTam) {
  char chave[64], chaveEnxuta[72];
  MetaEpInfo ep;
  int ok;
  snprintf(chave, sizeof chave, "series/%s/%s", id, metaprov_idioma());
  snprintf(chaveEnxuta, sizeof chaveEnxuta, "anime-videos/%s/%s", id, metaprov_idioma());
  char *corpo = fichameta_ler(chave, 6 * 3600);
  if (!corpo) corpo = fichameta_ler(chaveEnxuta, 6 * 3600);
  int teveLista = corpo != NULL;
  if (thumb && thumbTam) thumb[0] = 0;
  ok = animeAbsoluto_corpo(corpo, absoluto, &ep, thumb, thumbTam);
  if (!ok && (!strncmp(id, "tt", 2) || !strncmp(id, "tmdb:", 5))) {
    free(corpo);
    corpo = metaprov_meta("series", id, 6, NULL);
    teveLista = teveLista || corpo != NULL;
    if (corpo) {
      fichameta_gravar(chave, corpo);
      ok = animeAbsoluto_corpo(corpo, absoluto, &ep, thumb, thumbTam);
      // Kept only when it maps: a list that does not is no better next time.
      if (ok) {
        char *enxuto = animeVideosEnxutos(corpo);
        if (enxuto) fichameta_gravar(chaveEnxuta, enxuto);
        free(enxuto);
      }
    }
  }
  free(corpo);
  // Only when no list could be read at all: a list that answered and does
  // not place this number is an answer, not an outage.
  if (ok) {
    animeAbsMemoGuardar(id, absoluto, ep.temporada, ep.episodio, thumb);
  } else if (!teveLista && animeAbsMemoLer(id, absoluto, t, e, thumb, thumbTam)) {
    printf("[simkl] %s E%d: episode list unavailable; keeping S%dE%d from the last mapping\n",
           id, absoluto, *t, *e);
    return 1;
  }
  *t = ok ? ep.temporada : 0;
  *e = ok ? ep.episodio : 0;
  if (!ok && thumb && thumbTam) thumb[0] = 0;
  return ok;
}

static int animeTvdbGlobal(const char *corpo, int temporada, int episodio,
                           MetaEpInfo *mapeado) {
  const char *p;
  unsigned char encontrados[SHRT_MAX + 1] = {0};
  int primeiro = INT_MAX, ultimo = 0, anteriorMax = 0;
  if (!corpo || temporada <= 1 || episodio <= 0 ||
      episodio > SHRT_MAX || js_videos_temporadas_falsas(corpo)) return 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *fim = js_fim(p);
    int t = metaep_temporada(p, fim);
    int e = (int)js_num(p, fim, "episode", 0);
    if (t == temporada - 1 && e > anteriorMax) anteriorMax = e;
    if (t != temporada || e <= 0) continue;
    if (e < primeiro) primeiro = e;
    if (e > ultimo) ultimo = e;
  }
  if (primeiro <= 1 || anteriorMax != primeiro - 1 ||
      episodio > ultimo - primeiro + 1) return 0;
  int alvo = primeiro + episodio - 1;
  int quantidade = 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *fim = js_fim(p);
    int e = (int)js_num(p, fim, "episode", 0);
    if (metaep_temporada(p, fim) != temporada || e < primeiro || e > alvo) continue;
    if (encontrados[e - primeiro]) return 0;
    encontrados[e - primeiro] = 1;
    quantidade++;
  }
  if (quantidade != episodio) return 0;
  if (mapeado) {
    memset(mapeado, 0, sizeof *mapeado);
    mapeado->temporada = temporada;
    mapeado->episodio = alvo;
  }
  return 1;
}

typedef struct {
  int temporada, episodio, tvdb;  // tvdb: episode TVDB ID, 0 = unknown
} AnimeEpPar;

static int animeEpParComparar(const void *a, const void *b) {
  const AnimeEpPar *x = a, *y = b;
  if (x->temporada != y->temporada) return x->temporada < y->temporada ? -1 : 1;
  return (x->episodio > y->episodio) - (x->episodio < y->episodio);
}

// Watch order (season, then episode) without repeats.
static int animeEpParOrdenar(AnimeEpPar *pares, int n) {
  int unicos = 0;
  qsort(pares, (size_t)n, sizeof *pares, animeEpParComparar);
  for (int i = 0; i < n; i++)
    if (!unicos || animeEpParComparar(&pares[unicos - 1], &pares[i]))
      pares[unicos++] = pares[i];
  return unicos;
}

// The episode's place in TVDB's numbering, as Simkl sends it: flat
// (tvdb_season/tvdb_number) or, with extended=full_anime_seasons, nested
// ("tvdb": {"season": 2, "episode": 9}; specials are season 0). 0 when absent.
static int simklEpTvdb(const char *e, const char *fe, int *t, int *ep) {
  const char *tv, *ftv;
  int tt = (int)js_num(e, fe, "tvdb_season", -1.0), ee = (int)js_num(e, fe, "tvdb_number", 0.0);
  if (objetoRaiz(e, fe, "tvdb", &tv, &ftv)) {
    int nt = (int)js_num(tv, ftv, "season", -1.0), ne = (int)js_num(tv, ftv, "episode", 0.0);
    if (nt >= 0 && nt <= SHRT_MAX && ne > 0 && ne <= SHRT_MAX) { tt = nt; ee = ne; }
  }
  if (tt < 0 || tt > SHRT_MAX || ee <= 0 || ee > SHRT_MAX) return 0;
  *t = tt;
  *ep = ee;
  return 1;
}

// Whether this season/episode is in the list (sorted, as animeEpParOrdenar
// leaves it).
static int animeParesTem(const AnimeEpPar *v, int n, int t, int e) {
  AnimeEpPar alvo = { t, e, 0 };
  return v && n > 0 && bsearch(&alvo, v, (size_t)n, sizeof *v, animeEpParComparar);
}

static int animeCatalogoMapa(int indice, const char *id, AnimeEpPar **mapa,
                             int *quantidade, int *maxTemp1) {
  AnimeEpPar *pares;
  int total, n = 0;
  if (mapa) *mapa = NULL;
  if (quantidade) *quantidade = 0;
  if (maxTemp1) *maxTemp1 = 0;
  if (indice < 0 || !cat_episodios_finais(indice, id)) return 0;
  total = cat_n_episodios(indice);
  if (total <= 0) return 0;
  pares = calloc((size_t)total, sizeof *pares);
  if (!pares) return 0;
  for (int i = 0; i < total; i++) {
    const CatEp *ep = cat_episodio(indice, i);
    if (!ep || ep->temporada <= 0 || ep->episodio <= 0) continue;
    pares[n].temporada = ep->temporada;
    pares[n].episodio = ep->episodio;
    pares[n].tvdb = ep->tvdbEp > 0 ? ep->tvdbEp : 0;
    if (maxTemp1 && ep->temporada == 1 && ep->episodio > *maxTemp1)
      *maxTemp1 = ep->episodio;
    n++;
  }
  if (!n) {
    free(pares);
    return 0;
  }
  // The catalog keeps earlier pairs in place and appends new ones, so its
  // order is not watch order.
  *mapa = pares;
  *quantidade = animeEpParOrdenar(pares, n);
  return 1;
}

// The one TVDB season a per-season anime entry covers: "mapped_tvdb_seasons"
// is [2] for Kaiju No. 8 Season 2, [1..23] for One Piece (one entry for the
// whole show). 0 unless it is exactly one season after the first: an entry
// for season 1 numbers its episodes like the whole show would.
static int animeTemporadaUnica(const char *p, const char *f) {
  const char *v, *vf, *q;
  int n = 0, s = 0;
  if (!valorRaiz(p, f, "mapped_tvdb_seasons", &v, &vf) || *v != '[') return 0;
  for (q = v + 1; q < vf; ) {
    while (q < vf && ((unsigned char)*q <= ' ' || *q == ',' || *q == ']')) q++;
    if (q >= vf) break;
    if (*q < '0' || *q > '9') return 0;
    s = (int)strtol(q, (char **)&q, 10);
    n++;
  }
  return n == 1 && s > 1 && s <= SHRT_MAX ? s : 0;
}

static int simkl_ler_assistindo_com(const char *json, CatItem *dst, int max,
                                    int *absolutos, long *simklIds, int *temporadasUnicas) {
  // O Simkl guarda anime num terceiro tipo: /sync/all-items/anime/<estado>
  // devolve a raiz "anime" e o objeto interno tambem se chama "anime".
  static const char *vetores[] = { "shows", "anime" };
  static const char *chaves[]  = { "show", "anime" };
  const char *p;
  int n = 0, k, animeEntries = 0, animeKept = 0;
  if (!json || !dst) return 0;
  // Sem nada em "watching" o Simkl responde `null` ou `{}` — js_array devolve
  // NULL e o laco nem comeca.
  for (k = 0; k < 2; k++)
  for (p = js_array(json, NULL, vetores[k]); p && *p == '{';
       p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *oi, *of;
    CatItem *d;
    char imdb[24] = "", prox[16] = "", quando[40] = "";
    char thumb[512] = "";
    const char *reason = "missing next_to_watch";
    int t = 0, e = 0, absoluto = 0;
    if (k == 1) animeEntries++;
    if (n >= max) { reason = "candidate limit"; goto watchingDropped; }
    d = &dst[n];
    memset(d, 0, sizeof *d);
    // Anime may return a bare absolute number, as a JSON number or string.
    if (!js_texto_raiz_em(p, f, "next_to_watch", prox, sizeof prox)) {
      const char *vi, *vf;
      if (k != 1 || !valorRaiz(p, f, "next_to_watch", &vi, &vf) ||
          *vi < '0' || *vi > '9') goto watchingDropped;
      while (vf > vi && (unsigned char)vf[-1] <= ' ') vf--;
      if (vf - vi >= (int)sizeof prox) goto watchingDropped;
      snprintf(prox, sizeof prox, "%.*s", (int)(vf - vi), vi);
    }
    reason = "invalid next_to_watch";
    if (sscanf(prox, "S%dE%d", &t, &e) != 2) {
      char *end;
      long number;
      if (k != 1) goto watchingDropped;
      const char *start = prox + (prox[0] == 'E');
      number = strtol(start, &end, 10);
      if (end == start || *end || number <= 0 || number > INT_MAX) goto watchingDropped;
      absoluto = e = (int)number;
    }
    if (t < 0 || e <= 0) goto watchingDropped;
    reason = "missing title object";
    if (!objetoRaiz(p, f, chaves[k], &oi, &of) &&
        (k != 1 || !objetoRaiz(p, f, "show", &oi, &of))) goto watchingDropped;
    if (!lerObra(oi, of, d, imdb, sizeof imdb)) {
      const char *ii, *ifim;
      long id;
      reason = "missing supported id";
      if (k != 1 || !objetoRaiz(oi, of, "ids", &ii, &ifim)) goto watchingDropped;
      id = (long)js_num(ii, ifim, "tmdb", 0);
      if (id > 0) snprintf(imdb, sizeof imdb, "tmdb:%ld", id);
      else {
        id = (long)js_num(ii, ifim, "simkl", 0);
        if (id <= 0) goto watchingDropped;
        snprintf(imdb, sizeof imdb, "simkl:%ld", id);
      }
    }
    { const char *ii, *ifim;
      if (objetoRaiz(oi, of, "ids", &ii, &ifim)) {
        d->tmdb = (long)js_num(ii, ifim, "tmdb", 0);
        if (simklIds) simklIds[n] = (long)js_num(ii, ifim, "simkl", 0);
      }
    }
    // A per-season entry (Kaiju No. 8 Season 2) carries the show's IMDb ID
    // with full_anime_seasons, but its E9 is that season's ninth episode,
    // not the show's: placed later from its own episode list (simkl_continuar).
    { int unica = k == 1 ? animeTemporadaUnica(p, f) : 0;
      if (temporadasUnicas) temporadasUnicas[n] = unica;
      if (absoluto) {
        if (!strncmp(imdb, "simkl:", 6) || unica) t = e = 0;
        else animeAbsoluto(imdb, absoluto, &t, &e, thumb, sizeof thumb);
      } }
    d->temporada = t;
    d->episodio = e;
    snprintf(d->thumbEpisodio, sizeof d->thumbEpisodio, "%s", thumb);
    d->progresso = 0;
    if (e > 0) snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", imdb, t, e);
    else snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
    snprintf(d->tipo, sizeof d->tipo, "series");
    // O instante do "a seguir" e o do ULTIMO EPISODIO VISTO, como no Trakt
    // (ult[].quandoMs): e ele que diz quando a pessoa mexeu nesta serie.
    if (js_texto_raiz_em(p, f, "last_watched_at", quando, sizeof quando))
      d->retomadoMs = js_ms_iso(quando);
    if (absolutos) absolutos[n] = absoluto;
    n++;
    if (k == 1) {
      animeKept++;
      printf("[simkl] anime watching #%d id=%s next=%s: candidate (%s)\n",
             animeEntries, imdb, prox, e > 0 ? "episode mapped" :
             (absoluto && (!strncmp(imdb, "simkl:", 6) || animeTemporadaUnica(p, f)))
               ? "episode mapping pending" : "title fallback");
    }
    continue;
watchingDropped:
    if (k == 1)
      printf("[simkl] anime watching #%d: dropped (%s)\n", animeEntries, reason);
  }
  // Only for a list with an anime part: the shows list is read here too, and
  // printed "anime watching: 0 entries" on every Continue watching build.
  { const char *v, *vf;
    if (valorRaiz(json, NULL, "anime", &v, &vf))
      printf("[simkl] anime watching: %d entries, %d candidates, %d dropped\n",
             animeEntries, animeKept, animeEntries - animeKept); }
  return n;
}

int simkl_ler_assistindo(const char *json, CatItem *dst, int max) {
  return simkl_ler_assistindo_com(json, dst, max, NULL, NULL, NULL);
}

int simkl_ler_plantowatch(const char *json, int serie, CatItem *dst, int max) {
  static const char *vetores[] = { "movies", "shows", "anime" };
  static const char *chaves[]  = { "movie", "show", "anime" };
  const char *p;
  int n = 0, k;
  if (!json || !dst) return 0;
  for (k = serie ? 1 : 0; k < (serie ? 3 : 1) && n < max; k++)
  for (p = js_array(json, NULL, vetores[k]); p && *p == '{' && n < max;
       p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *oi, *of;
    CatItem *d = &dst[n];
    char imdb[24];
    memset(d, 0, sizeof *d);
    // Simkl's anime lists put each item under "show"; "anime" is kept too.
    if (!objetoRaiz(p, f, chaves[k], &oi, &of) &&
        (k != 2 || !objetoRaiz(p, f, "show", &oi, &of))) continue;
    if (!lerObra(oi, of, d, imdb, sizeof imdb)) continue;
    snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
    snprintf(d->tipo, sizeof d->tipo, "%s", serie ? "series" : "movie");
    d->naLista = 1;
    d->listaFontes = 1u << AJ_SALVOS_SIMKL;
    // O mesmo acabamento de trakt_lista: arte deterministica pelo IMDb, sem
    // um GET por item, e o genero de sempre para a legenda do cartaz.
    arte_metahub_preencher(d);
    snprintf(d->genero, sizeof d->genero, "%s",
             i18n(serie ? "Programa de TV" : "Filme"));
    n++;
  }
  return n;
}

static int imdbValido(const char *s) {
  int i;
  if (!s || s[0] != 't' || s[1] != 't' || !s[2]) return 0;
  for (i = 2; s[i] && s[i] != ':'; i++) if (s[i] < '0' || s[i] > '9') return 0;
  return i < 20;
}

int simkl_corpo_lista(char *dst, size_t tam, const char *imdb,
                      const char *tipo, int adicionar) {
  char id[24];
  const char *dp;
  size_t k;
  int w;
  if (!dst || !tam || !imdbValido(imdb)) return 0;
  dp = strchr(imdb, ':');
  k = dp ? (size_t)(dp - imdb) : strlen(imdb);
  if (k >= sizeof id) return 0;
  memcpy(id, imdb, k); id[k] = 0;
  // Filme e serie em vetores separados, como no Trakt: o id do IMDb resolvido
  // no escopo errado casa outro titulo, ou nenhum. Anime vai em "shows" — a
  // doc do add-to-list e do history/remove aceita os dois ali.
  w = snprintf(dst, tam,
               adicionar ? "{\"%s\":[{\"to\":\"plantowatch\",\"ids\":{\"imdb\":\"%s\"}}]}"
                         : "{\"%s\":[{\"ids\":{\"imdb\":\"%s\"}}]}",
               (tipo && !strcmp(tipo, "series")) ? "shows" : "movies", id);
  return w > 0 && (size_t)w < tam;
}

const char *simkl_rota_lista(int adicionar) {
  return adicionar ? "/sync/add-to-list" : "/sync/history/remove";
}

// ------------------------------------------------------------ historico (visto)
//
// Corpos de /sync/history e /sync/history/remove. Ver simkl.h para as rotas e a
// fonte; aqui so a montagem, pura, para o teste conferir texto contra texto.

// Anexa com snprintf e diz se coube. `*u` avanca; estourou, o corpo inteiro e
// recusado (0) em vez de sair cortado — JSON pela metade o servidor le como
// 400, ou pior, como outro titulo.
static int anexar(char *dst, size_t tam, size_t *u, const char *fmt, int a, int b) {
  int w;
  if (*u >= tam) return 0;
  w = snprintf(dst + *u, tam - *u, fmt, a, b);
  if (w < 0 || (size_t)w >= tam - *u) return 0;
  *u += (size_t)w;
  return 1;
}

static int baseImdb(const char *imdb, char *id, size_t tam) {
  size_t k;
  if (!imdbValido(imdb)) return 0;
  k = strcspn(imdb, ":");
  if (k >= tam) return 0;
  memcpy(id, imdb, k); id[k] = 0;
  return 1;
}

int simkl_corpo_historico_eps(char *dst, size_t tam, const char *imdb,
                              const VistoPar *pares, int qtd) {
  char id[24], feita[SMK_LOTE_MAX];
  size_t u = 0;
  int i, j, prim = 1;
  if (!dst || !tam || !pares || qtd < 1 || !baseImdb(imdb, id, sizeof id)) return 0;
  if (qtd > SMK_LOTE_MAX) qtd = SMK_LOTE_MAX;
  memset(feita, 0, sizeof feita);
  { int w = snprintf(dst, tam, "{\"shows\":[{\"ids\":{\"imdb\":\"%s\"},\"seasons\":[", id);
    if (w < 0 || (size_t)w >= tam) return 0;
    u = (size_t)w; }
  // AGRUPA POR TEMPORADA sem assumir ordem, como trakt_episodios_marcar: o
  // lote "ate aqui" atravessa temporadas e cada uma tem de aparecer UMA vez.
  // Sem watched_at: a doc diz que o servidor usa a hora do pedido.
  for (i = 0; i < qtd; i++) {
    int primEp = 1;
    if (feita[i]) continue;
    if (!anexar(dst, tam, &u, prim ? "{\"number\":%d,\"episodes\":[" : ",{\"number\":%d,\"episodes\":[",
                pares[i].temporada, 0)) return 0;
    prim = 0;
    for (j = i; j < qtd; j++) {
      if (feita[j] || pares[j].temporada != pares[i].temporada) continue;
      feita[j] = 1;
      if (!anexar(dst, tam, &u, primEp ? "{\"number\":%d}" : ",{\"number\":%d}",
                  pares[j].episodio, 0)) return 0;
      primEp = 0;
    }
    if (!anexar(dst, tam, &u, "]}", 0, 0)) return 0;
  }
  return anexar(dst, tam, &u, "]}]}", 0, 0);
}

int simkl_corpo_historico_titulo(char *dst, size_t tam, const char *imdb,
                                 const char *tipo, const int *temporadas,
                                 int nt, int visto) {
  char id[24];
  size_t u = 0;
  int i, serie = tipo && !strcmp(tipo, "series");
  if (!dst || !tam || !baseImdb(imdb, id, sizeof id)) return 0;
  if (!serie || visto) {
    // Filme (os dois sentidos) e serie marcada: o objeto so com ids. Para
    // serie, a guia mark-as-watched: "Drop both seasons and episodes" marca
    // todo episodio.
    int w = snprintf(dst, tam, "{\"%s\":[{\"ids\":{\"imdb\":\"%s\"}}]}",
                     serie ? "shows" : "movies", id);
    return w > 0 && (size_t)w < tam;
  }
  // DESMARCAR SERIE SEM TEMPORADAS APAGA A SERIE DA BIBLIOTECA do Simkl (aviso
  // da pagina remove-from-history). Com a lista de temporadas, cada entrada sem
  // `episodes` e expandida pelo servidor e a serie fica na biblioteca. Sem
  // temporada conhecida nao ha corpo seguro: 0, e nada e mandado.
  if (!temporadas || nt < 1) return 0;
  { int w = snprintf(dst, tam, "{\"shows\":[{\"ids\":{\"imdb\":\"%s\"},\"seasons\":[", id);
    if (w < 0 || (size_t)w >= tam) return 0;
    u = (size_t)w; }
  for (i = 0; i < nt; i++)
    if (!anexar(dst, tam, &u, i ? ",{\"number\":%d}" : "{\"number\":%d}", temporadas[i], 0))
      return 0;
  return anexar(dst, tam, &u, "]}]}", 0, 0);
}

const char *simkl_rota_historico(int visto) {
  return visto ? "/sync/history" : "/sync/history/remove";
}

// ------------------------------------------------------------ transporte

// Um pedido a api. `consulta` vai DEPOIS dos tres parametros obrigatorios.
// `metodo`: 'G' GET, 'P' POST, 'D' DELETE. O token entra so no cabecalho, e
// nada aqui imprime a URL — o client_id nao e segredo, mas o costume do app e
// logar so o caminho (ver rede_url_publica).
// A write reached Simkl: its activities moved, so the next list reader asks
// again instead of trusting the last check (see SMK_BIB_FRESCA). Atomic:
// writers may hold `trava`.
// playSujo: the same for Continue watching's playback copy, which otherwise
// waits for Simkl's playback timestamps to move (assinarPlayback).
static int bibSujo, playSujo;
static void bibSujar(char metodo, int st) {
  if ((metodo == 'P' || metodo == 'D') && st >= 200 && st < 300) {
    __atomic_store_n(&bibSujo, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&playSujo, 1, __ATOMIC_RELEASE);
  }
}

static char *pedirComToken(char metodo, const char *caminho, const char *consulta,
                           const char *corpo, int *st, const char *tk) {
  char url[640], cid[200], nome[120], aut[400];
  const char *cab[3];
  char *r;
  *st = 0;
  if (!tk[0] || !nuvem_simkl_cliente()[0]) return NULL;
  nuvem_url_escapar(nuvem_simkl_cliente(), cid, sizeof cid);
  nuvem_url_escapar(nuvem_simkl_app()[0] ? nuvem_simkl_app() : "nuvio", nome, sizeof nome);
  snprintf(url, sizeof url, "%s%s?client_id=%s&app-name=%s&app-version=1.0.1%s%s",
           SMK_API, caminho, cid, nome, consulta ? "&" : "", consulta ? consulta : "");
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", tk);
  cab[0] = aut;
  cab[1] = "Accept: application/json";
  cab[2] = NULL;
  if (metodo == 'P')      r = rede_postar_st(url, 20, cab, corpo ? corpo : "", st);
  else if (metodo == 'D') r = rede_apagar(url, 20, cab, st);
  else                    r = provedorBaixarSt(url, 25, cab, st);
  bibSujar(metodo, *st);
  return r;
}

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static uint64_t cacheGeracao = 1;
typedef SimklDestino SmkPedido;
static SmkPedido pedidoCapturar(void) {
  SmkPedido p = {0}; p.sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(p.sessao)) return p;
  const char *token = simklauth_token();
  if (token && *token && nuvem_simkl_cliente()[0]) {
    snprintf(p.aut, sizeof p.aut, "Authorization: Bearer %s", token);
    nuvem_url_escapar(nuvem_simkl_cliente(), p.cid, sizeof p.cid);
    nuvem_url_escapar(nuvem_simkl_app()[0] ? nuvem_simkl_app() : "nuvio", p.nome, sizeof p.nome);
  }
  pthread_mutex_lock(&trava); p.geracao = cacheGeracao; pthread_mutex_unlock(&trava);
  nv_perfil_sessao_destravar();
  return p;
}
// Profile opening first, private state second; neither lock crosses HTTP.
static int pedidoTravar(SmkPedido p) {
  if (!nv_perfil_sessao_travar(p.sessao)) return 0;
  pthread_mutex_lock(&trava);
  if (p.geracao != cacheGeracao) {
    pthread_mutex_unlock(&trava); nv_perfil_sessao_destravar(); return 0;
  }
  return 1;
}
static void pedidoDestravar(void) { pthread_mutex_unlock(&trava); nv_perfil_sessao_destravar(); }
static int pedidoAtual(SmkPedido p) {
  if (!pedidoTravar(p)) return 0;
  pedidoDestravar(); return 1;
}

static void simklAnimeIdGuardar(SmkPedido pedido, long simkl, const char *imdb) {
  char base[24];
  int i, livre = -1;
  if (simkl <= 0 || !baseImdb(imdb, base, sizeof base) || !pedidoTravar(pedido))
    return;
  for (i = 0; i < SMK_PLAY_MAX; i++) {
    if (simklAnimeIds[i].simkl == simkl) { livre = i; break; }
    if (!simklAnimeIds[i].simkl && livre < 0) livre = i;
  }
  if (livre < 0) {
    livre = simklAnimeIdProximo++ % SMK_PLAY_MAX;
  }
  simklAnimeIds[livre].simkl = simkl;
  strcpy(simklAnimeIds[livre].imdb, base);
  pedidoDestravar();
}

static int simklAnimeIdLer(long simkl, char *imdb, size_t tam) {
  int i, ok = 0;
  if (simkl <= 0 || !imdb || tam == 0) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < SMK_PLAY_MAX; i++) {
    if (simklAnimeIds[i].simkl == simkl && imdbValido(simklAnimeIds[i].imdb)) {
      snprintf(imdb, tam, "%s", simklAnimeIds[i].imdb);
      ok = 1;
      break;
    }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

static char *pedir(SmkPedido pedido, char metodo, const char *caminho, const char *consulta,
                   const char *corpo, int *st) {
  char url[640];
  const char *cab[3];
  char *r;
  *st = 0;
  if (!pedido.aut[0] || (metodo == 'G' && !pedidoAtual(pedido))) return NULL;
  snprintf(url, sizeof url, "%s%s?client_id=%s&app-name=%s&app-version=1.0.1%s%s",
           SMK_API, caminho, pedido.cid, pedido.nome, consulta ? "&" : "", consulta ? consulta : "");
  cab[0] = pedido.aut;
  cab[1] = "Accept: application/json";
  cab[2] = NULL;
  if (metodo == 'P')      r = rede_postar_st(url, 20, cab, corpo ? corpo : "", st);
  else if (metodo == 'D') r = rede_apagar(url, 20, cab, st);
  else {
    int next = -1;
    r = provedorGet(url, 25, cab, st, &next, 1);
    // Simkl normally returns all items. Follow explicit pagination if present.
    if (r && strstr(caminho, "/plantowatch")) {
      char *all = r;
      int page = 1;
      while (next > page && page < 64 && pedidoAtual(pedido)) {
        char paged[720]; page = next;
        snprintf(paged, sizeof paged, "%s&page=%d&limit=500", url, page);
        char *part = provedorGet(paged, 25, cab, st, &next, page);
        char *a = strchr(all, '['), *b = part ? strchr(part, '[') : NULL;
        if (!a || !b || *st < 200 || *st >= 300) { free(all); free(part); return NULL; }
        const char *ae = js_fim(a), *be = js_fim(b);
        if (ae <= a + 1 || ae[-1] != ']' || be <= b + 1 || be[-1] != ']') {
          free(all); free(part); return NULL;
        }
        size_t prefix = (size_t)(ae - all - 1), items = (size_t)(be - b - 2);
        size_t suffix = strlen(ae - 1);
        int comma = prefix > (size_t)(a - all + 1) && items > 0;
        if (prefix + items + suffix + 2 > REDE_CORPO_MAXIMO) { free(all); free(part); return NULL; }
        char *joined = malloc(prefix + items + suffix + 2);
        if (!joined) { free(all); free(part); return NULL; }
        memcpy(joined, all, prefix);
        if (comma) joined[prefix++] = ',';
        memcpy(joined + prefix, b + 1, items);
        strcpy(joined + prefix + items, ae - 1);
        free(all); free(part); all = joined;
      }
      if (next > page) { free(all); return NULL; }
      r = all;
    }
  }
  bibSujar(metodo, *st);
  return r;
}

static int slugPosterSimkl(const char *s) {
  int segmento = 0;
  if (!s || !*s) return 0;
  for (; *s; s++) {
    if (*s == '/') {
      if (!segmento) return 0;
      segmento = 0;
    } else if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
               (*s >= '0' && *s <= '9') || *s == '_' || *s == '-') {
      segmento++;
    } else return 0;
  }
  return segmento > 0;
}

// One Simkl anime's details (/anime/{id}?extended=full), cached for 30 days.
// *obra..*fim is the anime object, wrapped in "anime" or not.
static char *animeDetalheCorpo(SmkPedido pedido, long id, const char **obra,
                               const char **fim) {
  char chave[64], caminho[80];
  char *corpo;
  int st = 0;
  snprintf(chave, sizeof chave, "simkl-anime/%ld", id);
  corpo = fichameta_ler(chave, 30 * 24 * 3600);
  if (!corpo) {
    if (!pedidoAtual(pedido)) return NULL;
    snprintf(caminho, sizeof caminho, "/anime/%ld", id);
    corpo = pedir(pedido, 'G', caminho, "extended=full", NULL, &st);
    if (!corpo || st < 200 || st >= 300 || !pedidoAtual(pedido)) {
      printf("[simkl] anime details id=%ld unavailable (HTTP %d)\n", id, st);
      if (st == 404) fichameta_gravar(chave, "{}");
      free(corpo);
      return NULL;
    }
    fichameta_gravar(chave, corpo);
  }
  if (!objetoRaiz(corpo, NULL, "anime", obra, fim)) {
    *obra = corpo;
    *fim = js_fim(corpo);
    if (!*fim || **obra != '{') { free(corpo); return NULL; }
  }
  return corpo;
}

static int simkl_anime_detalhe(SmkPedido pedido, CatItem *d, int *temporadaSimkl,
                               long simklId) {
  char poster[128] = "", imdb[24] = "", titulo[160] = "";
  char base[32], imdbAtual[24] = "", imdbBase[24] = "";
  const char *obra, *fim, *ii, *ifim;
  char *corpo;
  char *end;
  long id = simklId, tmdb = 0;
  int ok = 0, temporada = 0;
  if (temporadaSimkl) *temporadaSimkl = 0;
  if (!d) return 0;
  snprintf(imdbAtual, sizeof imdbAtual, "%s", d->imdb);
  if (id <= 0) {
    if (strncmp(imdbAtual, "simkl:", 6)) return 0;
    id = strtol(imdbAtual + 6, &end, 10);
    if (end == imdbAtual + 6 || (*end && (*end != ':' || d->episodio <= 0)) || id <= 0)
      return 0;
  }
  if (!imdbValido(imdbAtual) && strncmp(imdbAtual, "tmdb:", 5))
    snprintf(imdbAtual, sizeof imdbAtual, "simkl:%ld", id);
  corpo = animeDetalheCorpo(pedido, id, &obra, &fim);
  if (!corpo) return 0;
  js_texto_raiz_em(obra, fim, "title", titulo, sizeof titulo);
  js_texto_raiz_em(obra, fim, "poster", poster, sizeof poster);
  temporada = (int)js_num(obra, fim, "season", 0);
  if (temporadaSimkl && temporada > 0) *temporadaSimkl = temporada;
  if (objetoRaiz(obra, fim, "ids", &ii, &ifim)) {
    js_texto(ii, ifim, "imdb", imdb, sizeof imdb);
    tmdb = (long)js_num(ii, ifim, "tmdb", 0);
  }
  if (titulo[0]) snprintf(d->titulo, sizeof d->titulo, "%s", titulo);
  if (imdbValido(imdbAtual) && baseImdb(imdbAtual, imdbBase, sizeof imdbBase))
    snprintf(base, sizeof base, "%s", imdbBase);
  else if (imdbValido(imdb)) snprintf(base, sizeof base, "%s", imdb);
  else if (tmdb > 0) snprintf(base, sizeof base, "tmdb:%ld", tmdb);
  else snprintf(base, sizeof base, "%s", imdbAtual);
  simklAnimeIdGuardar(pedido, id, base);
  if (tmdb > 0) d->tmdb = tmdb;
  if (slugPosterSimkl(poster))
    snprintf(d->poster, sizeof d->poster, "https://simkl.in/posters/%s_m.jpg", poster);
  if (d->episodio > 0) {
    if (temporada > 0) d->temporada = temporada;
    snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", base,
             d->temporada, d->episodio);
  } else {
    snprintf(d->imdb, sizeof d->imdb, "%s", base);
  }
  ok = imdbValido(imdb) || tmdb > 0 || slugPosterSimkl(poster);
  free(corpo);
  return ok;
}

static char *pegar(SmkPedido pedido, const char *caminho, const char *consulta, int *st) {
  char *r = pedir(pedido, 'G', caminho, consulta, NULL, st);
  if (r && (*st < 200 || *st >= 300 || !pedidoAtual(pedido))) {
    printf("[simkl] %s -> HTTP %d\n", caminho, *st);
    fflush(stdout);
    free(r);
    return NULL;
  }
  return r;
}

// ------------------------------------------------------------ perfil e stats

static int perfilNoMes(const char *iso, const struct tm *mes, int *dia) {
  long long ms;
  time_t s;
  struct tm local;
  if (!iso || !(ms = js_ms_iso(iso))) return 0;
  s = (time_t)(ms / 1000);
  localtime_r(&s, &local);
  if (local.tm_year != mes->tm_year || local.tm_mon != mes->tm_mon) return 0;
  if (dia) *dia = local.tm_mday;
  return 1;
}

// Cada obra e agregada antes de disputar as quatro vagas, como no Trakt.
static void perfilDestaque(PerfilDados *d, const PerfilDestaque *r) {
  int i = 0, n;
  if (!r->plays || !imdbValido(r->id)) return;
  while (i < d->nDestaques && d->destaques[i].plays >= r->plays) i++;
  if (i >= PERFIL_MAX_DESTAQUES) return;
  n = d->nDestaques;
  if (n < PERFIL_MAX_DESTAQUES) d->nDestaques++;
  else n--;
  for (; n > i; n--) d->destaques[n] = d->destaques[n - 1];
  d->destaques[i] = *r;
}

static void perfilSomar(PerfilDados *d, const struct tm *mes, const char *quando,
                        PerfilDestaque *r, int runtime,
                        int filme, int temporada, int episodio) {
  int dia = 0;
  if (!perfilNoMes(quando, mes, &dia)) return;
  d->plays++;
  if (filme) d->filmes++; else d->episodios++;
  if (runtime > 0) d->minutos += runtime;
  if (dia >= 1 && dia <= PERFIL_MAX_DIAS) d->atividade[dia - 1]++;
  r->plays++;
  if (runtime > 0) r->minutos += runtime;
  if (filme) snprintf(r->detalhe, sizeof r->detalhe, "Filme");
  else if (temporada > 0 && episodio > 0)
    snprintf(r->detalhe, sizeof r->detalhe, i18n("T%dE%d"), temporada, episodio);
  else if (temporada < 0 && episodio > 0)   // an anime numbered across the show
    snprintf(r->detalhe, sizeof r->detalhe, "E%d", episodio);
}

static void perfilLerTipo(const char *json, const char *lista, int filme,
                          PerfilDados *d, const struct tm *mes) {
  const char *p;
  for (p = js_array(json, NULL, lista); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *obra, *obraFim, *ids, *idsFim;
    char titulo[128] = "", imdb[24] = "", quando[48] = "";
    int runtime;
    PerfilDestaque r = {0};
    // Simkl's anime lists put each item under "show"; "anime" is kept too.
    if (!objetoRaiz(p, f, filme ? "movie" : "show", &obra, &obraFim) &&
        (filme || strcmp(lista, "anime") || !objetoRaiz(p, f, "anime", &obra, &obraFim)))
      continue;
    js_texto_raiz_em(obra, obraFim, "title", titulo, sizeof titulo);
    runtime = (int)js_num(obra, obraFim, "runtime", 0);
    if (objetoRaiz(obra, obraFim, "ids", &ids, &idsFim))
      js_texto(ids, idsFim, "imdb", imdb, sizeof imdb);
    if (imdbValido(imdb)) {
      snprintf(r.id, sizeof r.id, "%s", imdb);
      snprintf(r.titulo, sizeof r.titulo, "%s", titulo[0] ? titulo : imdb);
      snprintf(r.poster, sizeof r.poster, "https://images.metahub.space/poster/medium/%s/img", imdb);
      snprintf(r.backdrop, sizeof r.backdrop, "https://images.metahub.space/background/medium/%s/img", imdb);
    }
    if (filme) {
      js_texto_raiz_em(p, f, "last_watched_at", quando, sizeof quando);
      perfilSomar(d, mes, quando, &r, runtime, 1, 0, 0);
    } else {
      const char *s;
      // Simkl files every anime episode under season 1 with its own number
      // (One Piece E1122, Kaiju No. 8 Season 2's E8). A per-season entry is
      // labelled by its TVDB placement (S2E8, as Continue watching numbers
      // it); one numbered across the show by that number alone (E1122).
      int anime = !strcmp(lista, "anime"), unica = anime ? animeTemporadaUnica(p, f) : 0;
      for (s = js_array(p, f, "seasons"); s && *s == '{'; s = js_prox(js_fim(s))) {
        const char *sf = js_fim(s), *e;
        int temporada = (int)js_num(s, sf, "number", 0);
        for (e = js_array(s, sf, "episodes"); e && *e == '{'; e = js_prox(js_fim(e))) {
          const char *ef = js_fim(e), *tv, *tvf;
          int episodio = (int)js_num(e, ef, "number", 0);
          int duracao = (int)js_num(e, ef, "runtime", runtime);
          int t = temporada, ep = episodio;
          if (anime && temporada <= 1) {
            t = -1;
            if (unica && objetoRaiz(e, ef, "tvdb", &tv, &tvf) &&
                (int)js_num(tv, tvf, "season", 0) > 0 && (int)js_num(tv, tvf, "episode", 0) > 0) {
              t = (int)js_num(tv, tvf, "season", 0);
              ep = (int)js_num(tv, tvf, "episode", 0);
            }
          }
          quando[0] = 0;
          js_texto_raiz_em(e, ef, "watched_at", quando, sizeof quando);
          perfilSomar(d, mes, quando, &r, duracao, 0, t, ep);
        }
      }
    }
    perfilDestaque(d, &r);
  }
}

// {} e a resposta vazia. Erro HTTP/HTML/JSON cortado nao e ausencia de plays.
static int perfilHistoricoValido(const char *json) {
  const char *fim, *p, *v, *vf;
  int listas = 0;
  static const char *tipos[] = { "movies", "shows", "anime" };
  if (!json) return 0;
  while (*json && (unsigned char)*json <= ' ') json++;
  if (*json != '{') return 0;
  fim = js_fim(json);
  if (fim <= json + 1 || fim[-1] != '}') return 0;
  for (p = fim; *p && (unsigned char)*p <= ' '; p++) ;
  if (*p || valorRaiz(json, fim, "error", &v, &vf)) return 0;
  p = json + 1;
  while (p < fim && (unsigned char)*p <= ' ') p++;
  if (*p == '}') return 1;
  for (int i = 0; i < 3; i++) if (valorRaiz(json, fim, tipos[i], &v, &vf)) {
    if (*v != '[' || vf > fim || vf <= v + 1 || vf[-1] != ']') return 0;
    listas++;
  }
  return listas != 0;
}

// Provider episodes in watch order (season, then episode; specials left out).
static int animeMetaMapa(const char *corpo, AnimeEpPar **mapa, int *quantidade) {
  const char *p;
  AnimeEpPar *pares = NULL;
  int n = 0, cap = 0;
  *mapa = NULL;
  *quantidade = 0;
  if (!corpo || js_videos_temporadas_falsas(corpo)) return 0;
  for (p = js_array(corpo, NULL, "videos"); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    int t = metaep_temporada(p, f);
    int e = (int)js_num(p, f, "episode", 0);
    if (t <= 0 || t > SHRT_MAX || e <= 0 || e > SHRT_MAX) continue;
    if (n == cap) {
      int novo = cap ? cap * 2 : 256;
      AnimeEpPar *mais = realloc(pares, (size_t)novo * sizeof *pares);
      if (!mais) { free(pares); return 0; }
      pares = mais;
      cap = novo;
    }
    double tvdb = js_num(p, f, "tvdb_id", 0);
    pares[n].temporada = t;
    pares[n].episodio = e;
    pares[n].tvdb = tvdb > 0 && tvdb <= INT_MAX && tvdb == (int)tvdb ? (int)tvdb : 0;
    n++;
  }
  if (!n) { free(pares); return 0; }
  *mapa = pares;
  *quantidade = animeEpParOrdenar(pares, n);
  return 1;
}

// An addon list can follow TVDB's numbering without carrying TVDB IDs
// (Xperience with "TheTVDB" as the anime source). Cinemeta follows the same
// numbering and publishes the IDs: when the two lists hold the same episodes,
// borrow them, so Simkl's TVDB IDs still decide instead of positions (One
// Piece has no 628 there, which shifts every later position by one).
static int animeCatalogoTvdbCinemeta(const char *id, AnimeEpPar *v, int n) {
  AnimeEpPar *cine = NULL;
  int qtd = 0, comuns = 0, copiados = 0, i, j;
  char *corpo;
  if (!v || n <= 0 || strncmp(id, "tt", 2)) return 0;
  for (i = 0; i < n; i++) if (v[i].tvdb > 0) return 0;
  corpo = metaprov_meta_cinemeta_com("series", id, 8, NULL, NULL, NULL);
  animeMetaMapa(corpo, &cine, &qtd);
  free(corpo);
  for (i = j = 0; i < n && j < qtd;) {
    int c = animeEpParComparar(&v[i], &cine[j]);
    if (c < 0) i++;
    else if (c > 0) j++;
    else { comuns++; i++; j++; }
  }
  // Same numbering only when nearly every episode lines up both ways.
  if (qtd > 0 && (long)comuns * 20 >= (long)n * 19 && (long)comuns * 20 >= (long)qtd * 19) {
    for (i = j = 0; i < n && j < qtd;) {
      int c = animeEpParComparar(&v[i], &cine[j]);
      if (c < 0) i++;
      else if (c > 0) j++;
      else { if (cine[j].tvdb > 0) { v[i].tvdb = cine[j].tvdb; copiados++; } i++; j++; }
    }
  }
  printf("[simkl] %s: episode list without TVDB IDs; Cinemeta %d episodes, %d in common, %d IDs borrowed\n",
         id, qtd, comuns, copiados);
  fflush(stdout);
  free(cine);
  return copiados;
}

typedef struct {
  char *corpo;
  AnimeEpPar *mapa;
  int quantidade, lido, buscado;
} AnimeMetaCache;

static void animeMetaCarregar(AnimeMetaCache *c, const char *id, int forcarRede) {
  char chave[64];
  snprintf(chave, sizeof chave, "series/%s/%s", id, metaprov_idioma());
  if (!c->lido) {
    c->lido = 1;
    c->corpo = fichameta_ler(chave, 6 * 3600);
  }
  if ((!c->corpo || forcarRede) && !c->buscado &&
      (!strncmp(id, "tt", 2) || !strncmp(id, "tmdb:", 5))) {
    char *novo;
    c->buscado = 1;
    novo = metaprov_meta("series", id, 6, NULL);
    if (novo) {
      fichameta_gravar(chave, novo);
      free(c->corpo);
      c->corpo = novo;
      free(c->mapa);
      c->mapa = NULL;
      c->quantidade = 0;
    }
  }
  if (!c->mapa) animeMetaMapa(c->corpo, &c->mapa, &c->quantidade);
}

// Absolute numbering: episode numbers keep counting across seasons (Nuvio's
// catalog has S22E1122), so Simkl's number names the episode directly.
static int animeParesAbsolutos(const AnimeEpPar *v, int n) {
  if (!v || n < 2 || v[0].temporada == v[n - 1].temporada) return 0;
  for (int i = 1; i < n; i++)
    if (v[i].episodio <= v[i - 1].episodio) return 0;
  return 1;
}

// Simkl anime history counts episodes 1..N across the whole show. Providers
// do not count the same way: Cinemeta's One Piece has no 628 and adds an
// extra before 1123, so counting positions lands one episode off. The
// episode's TVDB ID decides first; a provider that restarts numbering each
// season falls back to position, and a candidate whose own TVDB ID names a
// different episode is rejected. `rigido`: Simkl identifies this show by TVDB
// ID, so an episode it sends without one (One Piece 628) cannot take a
// position that already has its own TVDB ID.
static int animeParesAchar(const AnimeEpPar *v, int n, int absoluto, int tvdbId,
                           int rigido, int *t, int *e) {
  const AnimeEpPar *c = NULL;
  if (!v || n <= 0 || absoluto <= 0) return 0;
  if (tvdbId > 0) {
    for (int i = 0; i < n; i++)
      if (v[i].tvdb == tvdbId) { *t = v[i].temporada; *e = v[i].episodio; return 1; }
  }
  if (animeParesAbsolutos(v, n)) {
    int lo = 0, hi = n - 1;
    while (lo <= hi && !c) {
      int m = lo + (hi - lo) / 2;
      if (v[m].episodio == absoluto) c = &v[m];
      else if (v[m].episodio < absoluto) lo = m + 1;
      else hi = m - 1;
    }
  } else if (absoluto <= n) {
    c = &v[absoluto - 1];
  }
  if (!c || (c->tvdb > 0 && (tvdbId > 0 ? c->tvdb != tvdbId : rigido))) return 0;
  *t = c->temporada;
  *e = c->episodio;
  return 1;
}

// Whether Simkl's number falls inside this list at all. Past its end, another
// source may know the episode; inside it, the list's own answer stands.
static int animeParesCobre(const AnimeEpPar *v, int n, int absoluto) {
  if (!v || n <= 0) return 0;
  return animeParesAbsolutos(v, n) ? absoluto <= v[n - 1].episodio : absoluto <= n;
}

static int animeMetaAbsoluto(AnimeMetaCache *c, const char *id, int absoluto,
                             int tvdbId, int rigido, int *t, int *e) {
  if (absoluto <= 0) return 0;
  animeMetaCarregar(c, id, 0);
  if (animeParesAchar(c->mapa, c->quantidade, absoluto, tvdbId, rigido, t, e))
    return 1;
  // Cached metadata can predate new episodes: refresh it once per show.
  animeMetaCarregar(c, id, 1);
  return animeParesAchar(c->mapa, c->quantidade, absoluto, tvdbId, rigido, t, e);
}

// The episode list on screen can switch numbering after the import: One
// Piece opens with Nuvio's absolute list (S2E62) and then an addon such as
// Xperience replaces it with per-season numbers (S2E1). Each mapped anime
// entry remembers which list it was mapped onto, so a new list maps the show
// again from the library in memory. ONE SLOT PER SIMKL ENTRY: a show can have
// several (Kaiju No. 8's S1 and S2 share one IMDb ID; Bleach has two), and
// they are mapped together. Keyed by show only, the last entry replaced the
// others and a new list mapped just that one (Bleach 366 -> 0).
#define SMK_ANIME_HIST_MAX 64
typedef struct {
  char id[24];
  long simkl;           // the entry's Simkl ID; 0 when it has none
  unsigned assinatura;  // episode list it was mapped onto; 0 = none loaded
  uint64_t sessao;
  int proxT, proxE;     // in that list, the episode after the last one watched
} AnimeHistorico;
static AnimeHistorico animeHist[SMK_ANIME_HIST_MAX];
static int animeHistProximo;
static pthread_mutex_t animeHistTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned animeParesAssinatura(const AnimeEpPar *v, int n) {
  unsigned h = 2166136261u;
  if (!v || n <= 0) return 0;
  for (int i = 0; i < n; i++) {
    h = (h ^ (unsigned)v[i].temporada) * 16777619u;
    h = (h ^ (unsigned)v[i].episodio) * 16777619u;
    h = (h ^ (unsigned)v[i].tvdb) * 16777619u;
  }
  return h ? h : 1;
}

static void animeHistGuardar(const char *id, long simkl, unsigned assinatura,
                             uint64_t sessao, int proxT, int proxE) {
  int i, vaga = -1;
  if (!id || !id[0]) return;
  pthread_mutex_lock(&animeHistTrava);
  for (i = 0; i < SMK_ANIME_HIST_MAX; i++) {
    if (!strcmp(animeHist[i].id, id) && animeHist[i].simkl == simkl) { vaga = i; break; }
    if (vaga < 0 && !animeHist[i].id[0]) vaga = i;
  }
  if (vaga < 0) vaga = animeHistProximo++ % SMK_ANIME_HIST_MAX;
  snprintf(animeHist[vaga].id, sizeof animeHist[vaga].id, "%s", id);
  animeHist[vaga].simkl = simkl;
  animeHist[vaga].assinatura = assinatura;
  animeHist[vaga].sessao = sessao;
  animeHist[vaga].proxT = proxT;
  animeHist[vaga].proxE = proxE;
  pthread_mutex_unlock(&animeHistTrava);
}

static void animeHistEsquecer(void) {
  pthread_mutex_lock(&animeHistTrava);
  memset(animeHist, 0, sizeof animeHist);
  animeHistProximo = 0;
  pthread_mutex_unlock(&animeHistTrava);
}

static int simklHistoricoTipo(const char *json, const char *lista,
                              const char *chaveObra, const char *filtro,
                              const char *fallbackImdb, long fallbackTmdb,
                              uint64_t sessao) {
  const char *p;
  int importados = 0;
  for (p = js_array(json, NULL, lista); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *obra, *fimObra, *ids, *fimIds, *s;
    const char *chave = chaveObra;
    char id[24] = "";
    long simklEntrada;
    if (!objetoRaiz(p, f, chave, &obra, &fimObra)) {
      if (strcmp(lista, "anime") || !objetoRaiz(p, f, "show", &obra, &fimObra))
        continue;
    }
    if (!objetoRaiz(obra, fimObra, "ids", &ids, &fimIds)) continue;
    js_texto(ids, fimIds, "imdb", id, sizeof id);
    simklEntrada = (long)js_num(ids, fimIds, "simkl", 0.0);
    if (!imdbValido(id)) {
      if (simklAnimeIdLer(simklEntrada, id, sizeof id)) {
        // The anime details already tied this exact Simkl ID to a catalog ID.
      } else {
        double tmdb = js_num(ids, fimIds, "tmdb", 0.0);
        if (!fallbackImdb || fallbackTmdb <= 0 ||
            tmdb != (double)fallbackTmdb) continue;
        snprintf(id, sizeof id, "%s", fallbackImdb);
      }
    }
    if (filtro && strcmp(id, filtro)) continue;
    VistoFonte fonte;
    AnimeMetaCache animeMeta = {0};
    int importadosAntes = importados;
    int catalogoIndice = cat_indice_por_imdb_tipo(id, "series");
    AnimeEpPar *animeCatalogo = NULL;
    int animeCatalogoQtd = 0, animeCatalogoTemp1Max = 0;
    if (!strcmp(lista, "anime"))
      animeCatalogoMapa(catalogoIndice, id, &animeCatalogo,
                        &animeCatalogoQtd, &animeCatalogoTemp1Max);
    // Taken before any borrowed IDs, as simkl_anime_lista_mudou computes it.
    unsigned animeCatalogoAssin = animeParesAssinatura(animeCatalogo, animeCatalogoQtd);
    memset(&fonte, 0, sizeof fonte);
    int simklRigido = 0;
    int ultimoN = 0, ultimoT = 0, ultimoE = 0;
    // BY TVDB NUMBER. Simkl names each episode's place in TVDB's numbering
    // (Kaiju No. 8 Season 2's episode 1 is S2E1). A list on screen numbered
    // the same way (Xperience with TheTVDB, Cinemeta) takes those numbers as
    // they are, with no IDs and no positions. Whether it is numbered that way
    // is decided for the whole entry: nearly all (95%) of its regular episodes
    // must be in the list. Nuvio's absolute One Piece (S22E1089...) shares
    // only season 1 with TVDB, and a single-season Kaiju list has no S2E1:
    // both fail and keep the chain below.
    int porNumero = 0, numerosQtd = 0, numerosNaLista = 0;
    if (!strcmp(lista, "anime")) {
      int total = 0, comTvdb = 0;
      for (s = js_array(p, f, "seasons"); s && *s == '{'; s = js_prox(js_fim(s))) {
        const char *fs = js_fim(s), *e, *ie, *fie;
        for (e = js_array(s, fs, "episodes"); e && *e == '{'; e = js_prox(js_fim(e))) {
          const char *fe = js_fim(e);
          int nt, ne;
          total++;
          if (objetoRaiz(e, fe, "ids", &ie, &fie) && js_num(ie, fie, "tvdb_id", 0) > 0)
            comTvdb++;
          // Specials (season 0) are left out: the list keeps no season 0.
          if (simklEpTvdb(e, fe, &nt, &ne) && nt > 0) {
            numerosQtd++;
            numerosNaLista += animeParesTem(animeCatalogo, animeCatalogoQtd, nt, ne);
          }
        }
      }
      simklRigido = comTvdb * 2 > total;
      porNumero = numerosQtd > 0 && (long)numerosNaLista * 20 >= (long)numerosQtd * 19;
      // Numbers already decide: no Cinemeta download for its IDs.
      if (simklRigido && !porNumero)
        animeCatalogoTvdbCinemeta(id, animeCatalogo, animeCatalogoQtd);
    }
    for (s = js_array(p, f, "seasons"); s && *s == '{'; s = js_prox(js_fim(s))) {
      const char *fs = js_fim(s), *e;
      int temporada = (int)js_num(s, fs, "number", -1.0);
      if (temporada < 0 || temporada > SHRT_MAX) continue;
      for (e = js_array(s, fs, "episodes"); e && *e == '{'; e = js_prox(js_fim(e))) {
        const char *fe = js_fim(e);
        char assistido[40] = "";
        int t = temporada, episodio = (int)js_num(e, fe, "number", 0.0);
        int tvdbT = -1, tvdbE = 0;
        int tvdbMapeado = 0;
        const char *idsEp, *fimIdsEp;
        simklEpTvdb(e, fe, &tvdbT, &tvdbE);
        int tvdbId = 0;
        if (objetoRaiz(e, fe, "ids", &idsEp, &fimIdsEp)) {
          double v = js_num(idsEp, fimIdsEp, "tvdb_id", 0.0);
          if (isfinite(v) && v > 0 && v <= INT_MAX && v == (int)v) tvdbId = (int)v;
        }
        if (catalogoIndice >= 0 && tvdbId > 0)
          tvdbMapeado = cat_episodio_por_tvdb(catalogoIndice, id,
                                              tvdbId, &t, &episodio);
        if (!js_texto_raiz_em(e, fe, "watched_at", assistido, sizeof assistido) ||
            !assistido[0])
          js_texto_raiz_em(e, fe, "last_watched_at", assistido, sizeof assistido);
        if (!assistido[0]) continue;
        // A special is S0E<n> in any list: a position would land on S1.
        if (!tvdbMapeado && !strcmp(lista, "anime") && tvdbE > 0 &&
            (tvdbT == 0 || (porNumero && tvdbT > 0))) {
          t = tvdbT;
          episodio = tvdbE;
          tvdbMapeado = 1;
        }
        if (!tvdbMapeado && !strcmp(lista, "anime")) {
          MetaEpInfo mapeado;
          int mappedT = 0, mappedE = 0, achou = 0;
          int temTvdb = tvdbT >= 0 && tvdbE > 0;
          if (temTvdb && tvdbT > 1) {
            animeMetaCarregar(&animeMeta, id, 0);
            if (animeTvdbGlobal(animeMeta.corpo, tvdbT, tvdbE, &mapeado)) {
              mappedT = mapeado.temporada;
              mappedE = mapeado.episodio;
              achou = 1;
            }
          }
          if (episodio <= 0 && temTvdb) episodio = tvdbE;
          if (!achou && temporada <= 1 && episodio >= 1) {
            // The loaded episode list is what the screen shows; the provider
            // metadata covers what it lacks.
            achou = animeParesAchar(animeCatalogo, animeCatalogoQtd, episodio,
                                    tvdbId, simklRigido, &mappedT, &mappedE) ||
                    (!animeParesCobre(animeCatalogo, animeCatalogoQtd, episodio) &&
                     animeMetaAbsoluto(&animeMeta, id, episodio, tvdbId,
                                       simklRigido, &mappedT, &mappedE));
          }
          if (achou) {
            t = mappedT;
            episodio = mappedE;
          } else if (temTvdb) {
            t = tvdbT;
            episodio = tvdbE;
          } else if (temporada <= 1 &&
                     (animeCatalogoQtd > 0 || animeMeta.quantidade > 0 ||
                      episodio > 100)) {
            // Not in the provider's list: marking S1E<n> would hit the wrong
            // episode.
            continue;
          }
        }
        if (episodio <= 0 || episodio > SHRT_MAX) continue;
        if (!strcmp(lista, "anime") && temporada <= 1 &&
            (int)js_num(e, fe, "number", 0.0) > ultimoN) {
          ultimoN = (int)js_num(e, fe, "number", 0.0);
          ultimoT = t;
          ultimoE = episodio;
        }
        if (vistoep_fonte_se_sessao(id, t, episodio, 1,
                                    js_ms_iso(assistido), &fonte, sessao))
          importados++;
      }
    }
    vistoep_fonte_log(id, "simkl", &fonte);
    if (!strcmp(lista, "anime")) {
      int proxT = 0, proxE = 0;
      for (int i = 0; ultimoN > 0 && i + 1 < animeCatalogoQtd; i++)
        if (animeCatalogo[i].temporada == ultimoT && animeCatalogo[i].episodio == ultimoE) {
          proxT = animeCatalogo[i + 1].temporada;
          proxE = animeCatalogo[i + 1].episodio;
          break;
        }
      animeHistGuardar(id, simklEntrada, animeCatalogoAssin, sessao, proxT, proxE);
      int tvC = 0, tvM = 0;
      for (int i = 0; i < animeCatalogoQtd; i++) tvC += animeCatalogo[i].tvdb > 0;
      for (int i = 0; i < animeMeta.quantidade; i++) tvM += animeMeta.mapa[i].tvdb > 0;
      // Per Simkl entry: a show can have several (Kaiju No. 8's S1 and S2).
      // marked = episodes this entry set as watched, marked already or not.
      printf("[simkl] %s simkl=%ld: anime map marked=%d tvdb_numbers=%d/%d%s "
             "catalog=%d(abs=%d tvdb=%d) "
             "meta=%d(abs=%d tvdb=%d fetched=%d) simkl_tvdb=%d\n", id, simklEntrada,
             importados - importadosAntes, numerosNaLista, numerosQtd,
             porNumero ? "(used)" : "",
             animeCatalogoQtd, animeParesAbsolutos(animeCatalogo, animeCatalogoQtd), tvC,
             animeMeta.quantidade, animeParesAbsolutos(animeMeta.mapa, animeMeta.quantidade),
             tvM, animeMeta.buscado, simklRigido);
      fflush(stdout);
    }
    free(animeMeta.corpo);
    free(animeMeta.mapa);
    free(animeCatalogo);
  }
  return importados;
}

static int simklHistoricoLerCom(const char *json, const char *imdb,
                                const char *fallbackImdb, long fallbackTmdb,
                                uint64_t sessao) {
  char id[24], *filtro = NULL;
  int importados;
  if ((imdb && (!baseImdb(imdb, id, sizeof id) || !id[0])) ||
      !perfilHistoricoValido(json)) return -1;
  if (imdb) filtro = id;
  importados = simklHistoricoTipo(json, "shows", "show", filtro,
                                  fallbackImdb, fallbackTmdb, sessao);
  importados += simklHistoricoTipo(json, "anime", "anime", filtro,
                                   fallbackImdb, fallbackTmdb, sessao);
  printf("[vistoep] Simkl: importou %d episódios%s%s\n", importados,
         filtro ? " de " : "", filtro ? filtro : "");
  fflush(stdout);
  return importados;
}

int simkl_historico_ler(const char *json, const char *imdb, uint64_t sessao) {
  return simklHistoricoLerCom(json, imdb, NULL, 0, sessao);
}

static char *bibCopiaSessao(uint64_t sessao);

typedef struct {
  char id[24];
  uint64_t sessao;
} AnimeReaplicar;

// Maps every entry of the show again, from the library in memory, with the
// same TMDB fallback as the show page (Cyberpunk: Edgerunners' S2 has only a
// TMDB ID).
static void *animeReaplicarFio(void *arg) {
  AnimeReaplicar *r = arg;
  CatItem item;
  long tmdb = 0;
  int n = -1;
  char *corpo = bibCopiaSessao(r->sessao);
  if (cat_copiar_por_id(r->id, "series", &item)) tmdb = item.tmdb;
  if (corpo) n = simklHistoricoTipo(corpo, "anime", "anime", r->id, r->id, tmdb, r->sessao);
  printf("[vistoep] %s: Simkl anime history mapped again onto the new episode list (%d)\n",
         r->id, n);
  fflush(stdout);
  free(corpo);
  vistoep_segurar(r->id, 0);
  // Another list may have been published while this ran.
  if (n > 0) simkl_anime_lista_mudou(r->id);
  free(r);
  return NULL;
}

int simkl_anime_proximo(const char *imdb, int *t, int *e) {
  char id[24];
  AnimeEpPar *mapa = NULL;
  int qtd = 0, ok = 0, bT = 0, bE = 0;
  unsigned assinatura;
  uint64_t sessao = nv_perfil_sessao_ler();
  if (!imdb || !t || !e || !baseImdb(imdb, id, sizeof id) || !id[0]) return 0;
  if (!animeCatalogoMapa(cat_indice_por_imdb_tipo(id, "series"), id, &mapa, &qtd, NULL))
    return 0;
  assinatura = animeParesAssinatura(mapa, qtd);
  free(mapa);
  pthread_mutex_lock(&animeHistTrava);
  // Every entry of the show must be mapped onto this very list; the furthest
  // next-up wins (Kaiju No. 8: S1's is S2E1, S2's is S2E9).
  for (int i = 0; i < SMK_ANIME_HIST_MAX; i++) {
    const AnimeHistorico *h = &animeHist[i];
    if (strcmp(h->id, id) || h->sessao != sessao) continue;
    if (h->assinatura != assinatura) { ok = 0; break; }
    if (h->proxE > 0 && (h->proxT > bT || (h->proxT == bT && h->proxE > bE))) {
      bT = h->proxT;
      bE = h->proxE;
      ok = 1;
    }
  }
  pthread_mutex_unlock(&animeHistTrava);
  if (ok) { *t = bT; *e = bE; }
  return ok;
}

void simkl_anime_lista_mudou(const char *imdb) {
  char id[24];
  AnimeEpPar *mapa = NULL;
  AnimeReaplicar *r;
  int qtd = 0, indice, mudou = 0;
  unsigned assinatura;
  uint64_t sessao = nv_perfil_sessao_ler();
  pthread_t fio;
  if (!imdb || !baseImdb(imdb, id, sizeof id) || !id[0]) return;
  indice = cat_indice_por_imdb_tipo(id, "series");
  if (!animeCatalogoMapa(indice, id, &mapa, &qtd, NULL)) return;
  assinatura = animeParesAssinatura(mapa, qtd);
  free(mapa);
  pthread_mutex_lock(&animeHistTrava);
  for (int i = 0; i < SMK_ANIME_HIST_MAX; i++) {
    const AnimeHistorico *h = &animeHist[i];
    if (!strcmp(h->id, id) && h->sessao == sessao && h->assinatura != assinatura) mudou = 1;
  }
  // Claimed now, every entry of the show, so a second publish of the same list
  // does not repeat it. The next-up pairs belong to the old list until the
  // thread rewrites them.
  for (int i = 0; mudou && i < SMK_ANIME_HIST_MAX; i++) {
    AnimeHistorico *h = &animeHist[i];
    if (strcmp(h->id, id) || h->sessao != sessao) continue;
    h->assinatura = assinatura;
    h->proxT = h->proxE = 0;
  }
  pthread_mutex_unlock(&animeHistTrava);
  if (!mudou || !(r = calloc(1, sizeof *r))) return;
  snprintf(r->id, sizeof r->id, "%s", id);
  r->sessao = sessao;
  // From the moment the new list is out: its numbering may not match the
  // marks yet (One Piece fell to 9 watched until this thread finished).
  vistoep_segurar(id, 1);
  if (pthread_create(&fio, NULL, animeReaplicarFio, r) == 0) pthread_detach(fio);
  else { vistoep_segurar(id, 0); free(r); }
}

// THE SAVED LIBRARY (simklbib.h). The whole pull below also keeps the pages,
// joined into one body of the same shape, and saves it with the activities
// asked just before: a later delta from that "all" may repeat a change, never
// miss one.
typedef struct { char *p; size_t n, cap; } SmkTexto;

static void trocar(char **dst, char *novo) { free(*dst); *dst = novo; }

static int textoPor(SmkTexto *t, const char *s, size_t n) {
  if (t->n + n + 1 > t->cap) {
    size_t cap = t->cap ? t->cap : 4096;
    char *novo;
    while (cap < t->n + n + 1) cap *= 2;
    if (cap > REDE_CORPO_MAXIMO || !(novo = realloc(t->p, cap))) return 0;
    t->p = novo; t->cap = cap;
  }
  memcpy(t->p + t->n, s, n);
  t->n += n;
  t->p[t->n] = 0;
  return 1;
}

static const char *const bibTipos[3] = { "shows", "anime", "movies" };

// Appends one page's items, list by list. 0 when the page is not all-items.
static int bibliotecaJuntar(SmkTexto tipos[3], const char *pagina) {
  const char *ini = pagina, *fim, *v, *vf;
  if (!perfilHistoricoValido(pagina)) return 0;
  while ((unsigned char)*ini <= ' ') ini++;
  fim = js_fim(ini);
  for (int i = 0; i < 3; i++) {
    if (!valorRaiz(ini, fim, bibTipos[i], &v, &vf)) continue;
    const char *a = v + 1, *b = vf - 1;
    while (a < b && (unsigned char)*a <= ' ') a++;
    while (b > a && (unsigned char)b[-1] <= ' ') b--;
    if (a == b) continue;
    if ((tipos[i].n && !textoPor(&tipos[i], ",", 1)) ||
        !textoPor(&tipos[i], a, (size_t)(b - a))) return 0;
  }
  return 1;
}

static char *bibliotecaMontar(const SmkTexto tipos[3]) {
  SmkTexto t = {0};
  int ok = textoPor(&t, "{", 1);
  for (int i = 0; ok && i < 3; i++) {
    char cab[24];
    int n = snprintf(cab, sizeof cab, "%s\"%s\":[", i ? "," : "", bibTipos[i]);
    ok = textoPor(&t, cab, (size_t)n) &&
         (!tipos[i].n || textoPor(&t, tipos[i].p, tipos[i].n)) && textoPor(&t, "]", 1);
  }
  if (ok) ok = textoPor(&t, "}", 1);
  if (!ok) { free(t.p); return NULL; }
  return t.p;
}

// /sync/activities as the saved library keeps it: an object with "all".
static char *bibliotecaAtividades(SmkPedido pedido) {
  char quando[40] = "";
  int st = 0;
  char *r = pedir(pedido, 'G', "/sync/activities", NULL, NULL, &st);
  const char *p = r;
  while (p && *p && (unsigned char)*p <= ' ') p++;
  if (!r || st < 200 || st >= 300 || !p || *p != '{' ||
      !js_texto_raiz(p, "all", quando, sizeof quando) || !quando[0]) {
    free(r);
    return NULL;
  }
  return r;
}

// The link the request went out with, as simklbib signs it.
static unsigned long long pedidoConta(SmkPedido pedido) {
  static const char pre[] = "Authorization: Bearer ";
  if (strncmp(pedido.aut, pre, sizeof pre - 1)) return 0;
  return simklbib_conta(pedido.aut + sizeof pre - 1);
}

// `desde`: a delta, only the items changed strictly after that activities
// timestamp (sent exactly as /sync/activities wrote it). NULL: the whole
// library.
static char *simklHistoricoPagina(SmkPedido pedido, int pagina, const char *desde,
                                  int *st, int *proxima) {
  char url[720], consulta[300];
  const char *cab[3];
  // full_anime_seasons is full plus, for anime, each episode's TVDB season and
  // episode and the entry's mapped_tvdb_seasons. It also gives a per-season
  // entry the show's IDs: with full, 21 of 86 anime entries on a real account
  // came with no IMDb or TMDB ID (My Hero Academia S2-7, Spy x Family...).
  // language=en: an anime's title is otherwise its romaji name; language=pt
  // was tried and gave romaji too.
  snprintf(consulta, sizeof consulta,
           "extended=full_anime_seasons&include_all_episodes=yes&episode_watched_at=yes"
           "&episode_tvdb_id=yes&language=en&page=%d&limit=500%s%s",
           pagina, desde ? "&date_from=" : "", desde ? desde : "");
  snprintf(url, sizeof url, "%s/sync/all-items/all/all?client_id=%s&app-name=%s"
           "&app-version=1.0.1&%s", SMK_API, pedido.cid, pedido.nome, consulta);
  cab[0] = pedido.aut;
  cab[1] = "Accept: application/json";
  cab[2] = NULL;
  return provedorGet(url, 25, cab, st, proxima, pagina);
}

// An answer with nothing in it: Simkl may send null or [] instead of lists.
static int corpoVazio(const char *r) {
  while (r && (unsigned char)*r <= ' ' && *r) r++;
  if (!r) return 0;
  if (!strncmp(r, "null", 4)) r += 4;
  else if (r[0] == '[' || r[0] == '{') {
    const char *q = r + 1;
    while ((unsigned char)*q <= ' ' && *q) q++;
    if ((r[0] == '[' && *q != ']') || (r[0] == '{' && *q != '}')) return 0;
    r = q + 1;
  } else return 0;
  while ((unsigned char)*r <= ' ' && *r) r++;
  return !*r;
}

// The library from the network, pages joined: whole, or (`desde`) the items
// changed since. 1 = *corpo set.
static int bibliotecaPuxar(SmkPedido pedido, const char *desde, char **corpo, int *st) {
  SmkTexto tipos[3] = {{0}};
  int pagina = 1, proxima = -1, ok = 0;
  *corpo = NULL;
  while (pagina <= 64 && pedidoAtual(pedido)) {
    char *json = simklHistoricoPagina(pedido, pagina, desde, st, &proxima);
    if (!json || *st < 200 || *st >= 300 ||
        (!corpoVazio(json) && !bibliotecaJuntar(tipos, json))) {
      free(json);
      break;
    }
    free(json);
    if (proxima <= pagina) { ok = 1; break; }
    pagina = proxima;
  }
  if (ok) *corpo = bibliotecaMontar(tipos);
  for (int i = 0; i < 3; i++) free(tipos[i].p);
  return *corpo != NULL;
}

// THE LIBRARY IN MEMORY, for the session. Loaded in the background when the
// profile opens (simkl_biblioteca_iniciar): the saved copy first, then the
// network only when /sync/activities moved since it. Regular shows' marks are
// imported as it arrives. An anime is mapped only when its show opens
// (simkl_historico_aplicar): mapping needs that show's episode list, and
// mapping every anime at once downloaded a provider list for each (19 to 27
// lists, 4 to 20 s, measured on the old branch).
// Under `trava`. The body is replaced whole, never edited in place.
static char *bibCorpo;      // all-items body
static char *bibAtiv;       // the activities it reflects; NULL = unknown
static uint64_t bibSessao, bibGeracao;
static time_t bibConferida; // last activities check that reached Simkl
static int bibFio;          // a loader is running for bibFioSessao/Geracao
static int bibVivos;        // loader threads alive, of any session
static uint64_t bibFioSessao, bibFioGeracao;
static pthread_cond_t bibMudou = PTHREAD_COND_INITIALIZER;
// A show page asks Simkl again (activities first) once the check is older.
#define SMK_BIB_IDADE 600

static int bibDoPedido(SmkPedido p) {   // under trava
  return bibCorpo && bibSessao == p.sessao && bibGeracao == p.geracao;
}

// The item without its "seasons" (the episode history): a status list
// (/sync/all-items/<type>/<status>) has none, and its readers were written
// for that shape.
static int textoItem(SmkTexto *t, const char *ini, const char *fim) {
  const char *v, *vf, *k, *corteIni, *corteFim;
  if (!valorRaiz(ini, fim, "seasons", &v, &vf)) return textoPor(t, ini, (size_t)(fim - ini));
  // Back from the value to the key's opening quote: "seasons" :
  k = v - 1;
  while (k > ini && (unsigned char)*k <= ' ') k--;
  if (*k != ':') return textoPor(t, ini, (size_t)(fim - ini));
  k--;
  while (k > ini && (unsigned char)*k <= ' ') k--;
  k -= 8;   // the 7 letters and the opening quote
  if (k <= ini || *k != '"') return textoPor(t, ini, (size_t)(fim - ini));
  corteIni = k;
  corteFim = vf;
  k--;
  while (k > ini && (unsigned char)*k <= ' ') k--;
  if (*k == ',') corteIni = k;   // not the first key: drop the comma before
  else {                         // the first key: drop the comma after
    while (corteFim < fim && (unsigned char)*corteFim <= ' ') corteFim++;
    if (corteFim < fim && *corteFim == ',') corteFim++;
  }
  return textoPor(t, ini, (size_t)(corteIni - ini)) &&
         textoPor(t, corteFim, (size_t)(fim - corteFim));
}

// One status list of the library, as /sync/all-items/<lista>/<estado> answers
// it: {"<lista>":[the items with that status]}. NULL without a library.
static char *bibLista(const char *corpo, const char *lista, const char *estado) {
  SmkTexto t = {0};
  const char *ini = corpo, *fim, *p;
  int itens = 0, ok;
  char cab[32], st[24];
  if (!corpo) return NULL;
  while ((unsigned char)*ini <= ' ') ini++;
  fim = js_fim(ini);
  snprintf(cab, sizeof cab, "{\"%s\":[", lista);
  ok = textoPor(&t, cab, strlen(cab));
  const char *v, *vf;
  if (ok && valorRaiz(ini, fim, lista, &v, &vf) && *v == '[') {
    for (p = v + 1; ok && p < vf; ) {
      while (p < vf && ((unsigned char)*p <= ' ' || *p == ',')) p++;
      if (p >= vf || *p != '{') break;
      const char *f = js_fim(p);
      if (!f || f > vf) break;
      st[0] = 0;
      js_texto_raiz_em(p, f, "status", st, sizeof st);
      if (!strcmp(st, estado))
        ok = (!itens++ || textoPor(&t, ",", 1)) && textoItem(&t, p, f);
      p = f;
    }
  }
  if (ok) ok = textoPor(&t, "]}", 2);
  if (!ok) { free(t.p); return NULL; }
  return t.p;
}

// A copy of the library of this session, or NULL.
static char *bibCopiaSessao(uint64_t sessao) {
  char *r = NULL;
  pthread_mutex_lock(&trava);
  if (bibCorpo && bibSessao == sessao && bibGeracao == cacheGeracao) r = strdup(bibCorpo);
  pthread_mutex_unlock(&trava);
  return r;
}

// Takes corpo/ativ. `conferida`: the activities were just checked with Simkl.
static void bibPublicar(SmkPedido p, char *corpo, char *ativ, int conferida) {
  if (pedidoTravar(p)) {
    if (corpo) {
      trocar(&bibCorpo, corpo); corpo = NULL;
      trocar(&bibAtiv, ativ); ativ = NULL;
      bibSessao = p.sessao;
      bibGeracao = p.geracao;
    } else if (ativ && bibDoPedido(p)) {
      trocar(&bibAtiv, ativ); ativ = NULL;   // only the activities moved on
    }
    if (conferida && bibDoPedido(p)) bibConferida = time(NULL);
    pthread_cond_broadcast(&bibMudou);
    pedidoDestravar();
  }
  free(corpo);
  free(ativ);
}

// Copies of the library and the activities it reflects; 0 when either is
// missing (a library loaded without its activities gets the whole pull).
static int bibAtivCopiar(SmkPedido p, char **corpo, char **ativ) {
  int ok = 0;
  *corpo = *ativ = NULL;
  pthread_mutex_lock(&trava);
  if (bibDoPedido(p) && bibAtiv) {
    *corpo = strdup(bibCorpo);
    *ativ = strdup(bibAtiv);
    ok = *corpo && *ativ;
  }
  pthread_mutex_unlock(&trava);
  if (!ok) { free(*corpo); free(*ativ); *corpo = *ativ = NULL; }
  return ok;
}

static int bibMesmoAll(const char *a, const char *b) {
  char x[40] = "", y[40] = "";
  return a && b && js_texto_raiz(a, "all", x, sizeof x) &&
         js_texto_raiz(b, "all", y, sizeof y) && x[0] && !strcmp(x, y);
}

// Regular shows only; see the note above for anime.
static void bibImportarSeries(const char *corpo, uint64_t sessao, const char *origem) {
  int n = simklHistoricoTipo(corpo, "shows", "show", NULL, NULL, 0, sessao);
  printf("[vistoep] Simkl: %d show episodes from the %s library\n", n, origem);
  fflush(stdout);
}

// The profile the request belongs to, read under its session; -1 if it moved.
static int pedidoPerfil(SmkPedido p) {
  int perfil = -1;
  if (pedidoTravar(p)) {
    perfil = simklauth_perfil();
    pedidoDestravar();
  }
  return perfil;
}

// WHAT MOVED between the activities the library reflects and new ones, per
// type: a status bucket (an item added or changed: a date_from delta) or
// removed_from_list (an item gone: the IDs-only list). Playback, ratings,
// settings and custom lists move nothing in the library. A block missing on
// either side counts as moved.
static void ativComparar(const char *velho, const char *novo, int *mudou, int *removeu) {
  static const char *const blocos[] = { "tv_shows", "anime", "movies" };
  static const char *const estados[] = { "plantowatch", "watching", "completed", "hold", "dropped" };
  const char *vf = js_fim(velho), *nf = js_fim(novo);
  *mudou = *removeu = 0;
  for (int b = 0; b < 3; b++) {
    const char *vi, *vfim, *ni, *nfim;
    if (!vf || !nf || !objetoRaiz(velho, vf, blocos[b], &vi, &vfim) ||
        !objetoRaiz(novo, nf, blocos[b], &ni, &nfim)) {
      *mudou = 1;
      continue;
    }
    for (int k = 0; k <= 5; k++) {
      const char *chave = k < 5 ? estados[k] : "removed_from_list";
      char x[48] = "", y[48] = "";
      js_texto_raiz_em(vi, vfim, chave, x, sizeof x);
      js_texto_raiz_em(ni, nfim, chave, y, sizeof y);
      if (strcmp(x, y)) *(k < 5 ? mudou : removeu) = 1;
    }
  }
}

// The "ids" object of an all-items item: in its show/anime/movie, or its own
// "ids" (the IDs-only list).
static int itemIds(const char *p, const char *f, const char **ids, const char **idsf) {
  static const char *const env[] = { "show", "anime", "movie" };
  const char *o, *of;
  for (int i = 0; i < 3; i++)
    if (objetoRaiz(p, f, env[i], &o, &of) && objetoRaiz(o, of, "ids", ids, idsf)) return 1;
  return objetoRaiz(p, f, "ids", ids, idsf);
}
// An item's Simkl ID. 0 when it has none.
static long itemSimkl(const char *p, const char *f) {
  const char *ids, *idsf;
  return itemIds(p, f, &ids, &idsf) ? (long)js_num(ids, idsf, "simkl", 0.0) : 0;
}

// One of an item's other IDs as text ("tt123", "456"), quotes dropped. 0 when
// missing, null or empty.
static int idOutro(const char *ids, const char *idsf, const char *chave, char *dst, size_t n) {
  const char *v, *vf;
  if (!valorRaiz(ids, idsf, chave, &v, &vf)) return 0;
  if (*v == '"') { v++; if (vf > v && vf[-1] == '"') vf--; }
  if (vf <= v || (size_t)(vf - v) >= n || (vf - v == 4 && !strncmp(v, "null", 4))) return 0;
  memcpy(dst, v, (size_t)(vf - v));
  dst[vf - v] = 0;
  return 1;
}

// Two items without a common Simkl ID are the same title when they share at
// least one other ID and no ID they both carry differs. Kaiju No. 8's
// seasons share an IMDb ID but not their MAL ID, so they never match.
static int itensMesmos(const char *a, const char *af, const char *b, const char *bf) {
  static const char *const chaves[] = { "imdb", "tmdb", "tvdb", "mal", "anidb", "anilist", "kitsu" };
  const char *ia, *iaf, *ib, *ibf;
  long sa, sb;
  int iguais = 0;
  if (!itemIds(a, af, &ia, &iaf) || !itemIds(b, bf, &ib, &ibf)) return 0;
  sa = (long)js_num(ia, iaf, "simkl", 0.0);
  sb = (long)js_num(ib, ibf, "simkl", 0.0);
  if (sa > 0 && sb > 0) return sa == sb;
  for (size_t k = 0; k < sizeof chaves / sizeof *chaves; k++) {
    char x[48], y[48];
    if (!idOutro(ia, iaf, chaves[k], x, sizeof x) || !idOutro(ib, ibf, chaves[k], y, sizeof y))
      continue;
    if (strcmp(x, y)) return 0;
    iguais++;
  }
  return iguais > 0;
}

typedef struct { long *v; int n, cap; } SmkIds;
static int idsComparar(const void *a, const void *b) {
  long x = *(const long *)a, y = *(const long *)b;
  return (x > y) - (x < y);
}
// Every Simkl ID in an all-items body, sorted. 0 if out of memory.
static int idsLer(SmkIds *s, const char *corpo) {
  const char *ini = corpo, *fim, *v, *vf, *p;
  while ((unsigned char)*ini <= ' ') ini++;
  fim = js_fim(ini);
  for (int t = 0; t < 3; t++) {
    if (!fim || !valorRaiz(ini, fim, bibTipos[t], &v, &vf) || *v != '[') continue;
    for (p = v + 1; p < vf; ) {
      while (p < vf && ((unsigned char)*p <= ' ' || *p == ',')) p++;
      if (p >= vf || *p != '{') break;
      const char *f = js_fim(p);
      if (!f || f > vf) break;
      long id = itemSimkl(p, f);
      if (id > 0) {
        if (s->n == s->cap) {
          long *novo = realloc(s->v, sizeof *novo * (size_t)(s->cap ? s->cap * 2 : 256));
          if (!novo) return 0;
          s->v = novo;
          s->cap = s->cap ? s->cap * 2 : 256;
        }
        s->v[s->n++] = id;
      }
      p = f;
    }
  }
  if (s->n) qsort(s->v, (size_t)s->n, sizeof *s->v, idsComparar);
  return 1;
}
static int idsTem(const SmkIds *s, long id) {
  return s->n && bsearch(&id, s->v, (size_t)s->n, sizeof *s->v, idsComparar);
}

// Every item of an all-items body, pointing into it. -1 out of memory.
typedef struct { const char *p, *f; long id; int filme; } SmkItem;
static int itensLer(const char *corpo, SmkItem **v) {
  const char *ini = corpo, *fim, *a, *af, *p;
  int n = 0, cap = 0;
  *v = NULL;
  while ((unsigned char)*ini <= ' ') ini++;
  fim = js_fim(ini);
  for (int t = 0; t < 3; t++) {
    if (!fim || !valorRaiz(ini, fim, bibTipos[t], &a, &af) || *a != '[') continue;
    for (p = a + 1; p < af; ) {
      while (p < af && ((unsigned char)*p <= ' ' || *p == ',')) p++;
      if (p >= af || *p != '{') break;
      const char *f = js_fim(p);
      if (!f || f > af) break;
      if (n == cap) {
        SmkItem *novo = realloc(*v, sizeof *novo * (size_t)(cap ? cap * 2 : 64));
        if (!novo) { free(*v); *v = NULL; return -1; }
        *v = novo;
        cap = cap ? cap * 2 : 64;
      }
      (*v)[n++] = (SmkItem){ p, f, itemSimkl(p, f), t == 2 };
      p = f;
    }
  }
  return n;
}

// The delta item an item of the library is replaced by, if any. By Simkl ID;
// when either side has none, by its other IDs (itensMesmos), shows and anime
// together, movies apart (TMDB numbers TV and movies separately).
static int itemTrocado(const SmkItem *o, const SmkIds *novos, const SmkItem *d, int nd) {
  if (o->id > 0 && idsTem(novos, o->id)) return 1;
  for (int i = 0; i < nd; i++)
    if ((o->id <= 0 || d[i].id <= 0) && o->filme == d[i].filme &&
        itensMesmos(o->p, o->f, d[i].p, d[i].f))
      return 1;
  return 0;
}

// THE MERGE. Each item of the delta replaces the library's item with the same
// Simkl ID, in whichever list it was (keyed by Simkl ID, not IMDb: Kaiju No.
// 8's two seasons share one IMDb ID); new ones are added. Simkl gives every
// item an ID; one without is matched by its other IDs (itemTrocado), and if
// it matches several library items, *ambiguo is set and NULL returned: the
// caller downloads the whole library rather than replace the wrong one.
// With `atuais` (the IDs-only list) an item no longer there is dropped; one
// without a Simkl ID cannot be checked and stays. NULL on failure.
static char *bibliotecaFundir(const char *velho, const char *delta, const SmkIds *atuais,
                              int *trocados, int *removidos, int *ambiguo) {
  SmkIds novos = {0};
  SmkTexto t = {0};
  SmkItem *vs = NULL, *ds = NULL;
  const char *vi = velho, *di = delta, *vfim, *dfim;
  int ok, nv, nd;
  *trocados = *removidos = *ambiguo = 0;
  while ((unsigned char)*vi <= ' ') vi++;
  while ((unsigned char)*di <= ' ') di++;
  vfim = js_fim(vi);
  dfim = js_fim(di);
  if (!vfim || !dfim || !idsLer(&novos, delta)) { free(novos.v); return NULL; }
  nv = itensLer(velho, &vs);
  nd = itensLer(delta, &ds);
  if (nv < 0 || nd < 0) { free(vs); free(ds); free(novos.v); return NULL; }
  // A delta item without a Simkl ID that fits more than one library item.
  for (int i = 0; i < nd && !*ambiguo; i++) {
    int achados = 0;
    if (ds[i].id > 0) continue;
    for (int j = 0; j < nv && achados < 2; j++)
      if (!(vs[j].id > 0 && idsTem(&novos, vs[j].id)) && vs[j].filme == ds[i].filme &&
          itensMesmos(vs[j].p, vs[j].f, ds[i].p, ds[i].f))
        achados++;
    *ambiguo = achados > 1;
  }
  free(vs);
  if (*ambiguo) { free(ds); free(novos.v); return NULL; }
  ok = textoPor(&t, "{", 1);
  for (int k = 0; ok && k < 3; k++) {
    const char *v, *vf, *p;
    int itens = 0;
    char cab[24];
    int n = snprintf(cab, sizeof cab, "%s\"%s\":[", k ? "," : "", bibTipos[k]);
    ok = textoPor(&t, cab, (size_t)n);
    for (int lado = 0; ok && lado < 2; lado++) {
      const char *ini = lado ? di : vi, *fim = lado ? dfim : vfim;
      if (!valorRaiz(ini, fim, bibTipos[k], &v, &vf) || *v != '[') continue;
      for (p = v + 1; ok && p < vf; ) {
        while (p < vf && ((unsigned char)*p <= ' ' || *p == ',')) p++;
        if (p >= vf || *p != '{') break;
        const char *f = js_fim(p);
        if (!f || f > vf) break;
        if (!lado) {
          SmkItem o = { p, f, itemSimkl(p, f), k == 2 };
          if (itemTrocado(&o, &novos, ds, nd)) { (*trocados)++; p = f; continue; }
          if (o.id > 0 && atuais && !idsTem(atuais, o.id)) { (*removidos)++; p = f; continue; }
        }
        ok = (!itens++ || textoPor(&t, ",", 1)) && textoPor(&t, p, (size_t)(f - p));
        p = f;
      }
    }
    if (ok) ok = textoPor(&t, "]", 1);
  }
  if (ok) ok = textoPor(&t, "}", 1);
  free(ds);
  free(novos.v);
  if (!ok) { free(t.p); return NULL; }
  return t.p;
}

// The IDs-only list (/sync/all-items?extended=simkl_ids_only): what is in the
// library now. 0 if it could not be had.
static int bibliotecaIds(SmkPedido pedido, SmkIds *ids, int *st) {
  char *r = pedir(pedido, 'G', "/sync/all-items", "extended=simkl_ids_only", NULL, st);
  int ok = r && *st >= 200 && *st < 300 &&
           (corpoVazio(r) || (perfilHistoricoValido(r) && idsLer(ids, r)));
  free(r);
  return ok;
}

// Brings `velho` (the library at `ativVelho`) up to `ativ`. 0 failed (nothing
// changed), 1 the library is unchanged (only playback and the like moved),
// 2 *novo and *delta set, 3 a merge cannot be trusted: download it whole.
static int bibliotecaAtualizar(SmkPedido pedido, const char *velho, const char *ativVelho,
                               const char *ativ, char **novo, char **delta, int *st) {
  char desde[48] = "";
  int mudou, removeu, trocados = 0, removidos = 0, ambiguo = 0;
  SmkIds atuais = {0};
  *novo = *delta = NULL;
  ativComparar(ativVelho, ativ, &mudou, &removeu);
  if (!mudou && !removeu) return 1;
  if (!js_texto_raiz(ativVelho, "all", desde, sizeof desde) || !desde[0]) {
    printf("[simkl] library: no date to ask changes from, downloading it whole\n");
    return 3;
  }
  if (mudou) {
    if (!bibliotecaPuxar(pedido, desde, delta, st)) return 0;
  } else if (!(*delta = strdup("{\"shows\":[],\"anime\":[],\"movies\":[]}"))) {
    return 0;
  }
  if (removeu && !bibliotecaIds(pedido, &atuais, st)) {
    free(*delta); *delta = NULL;
    free(atuais.v);
    return 0;
  }
  *novo = bibliotecaFundir(velho, *delta, removeu ? &atuais : NULL, &trocados, &removidos,
                           &ambiguo);
  free(atuais.v);
  if (!*novo) {
    free(*delta); *delta = NULL;
    if (!ambiguo) return 0;
    printf("[simkl] library: an item without a Simkl ID fits several, downloading it whole\n");
    return 3;
  }
  printf("[simkl] library: delta since %s: %d changed, %d removed%s\n", desde, trocados,
         removidos, removeu ? " (removals checked)" : "");
  return 2;
}

typedef struct { SmkPedido pedido; int perfil; } BibCarga;

static void *bibCarregarFio(void *arg) {
  BibCarga c = *(BibCarga *)arg;
  SmkPedido pedido = c.pedido;
  unsigned long long conta = pedidoConta(pedido);
  char *ativ, *corpo = NULL, *ativVelho = NULL, *novo = NULL, *delta = NULL;
  int tem, igual, st = 0, r = 3;
  free(arg);
  pthread_mutex_lock(&trava);
  tem = bibDoPedido(pedido);
  pthread_mutex_unlock(&trava);
  // 1. The saved copy, once per session.
  if (!tem) {
    SimklBib salvo;
    if (simklbib_ler(c.perfil, conta, &salvo)) {
      bibImportarSeries(salvo.biblioteca, pedido.sessao, "saved");
      bibPublicar(pedido, salvo.biblioteca, salvo.atividades, 0);
    }
  }
  // 2. Activities: unchanged since the copy = nothing to download. A write
  // after this point flags the library again.
  __atomic_store_n(&bibSujo, 0, __ATOMIC_RELEASE);
  ativ = bibliotecaAtividades(pedido);
  pthread_mutex_lock(&trava);
  tem = bibDoPedido(pedido);
  igual = tem && bibMesmoAll(bibAtiv, ativ);
  pthread_mutex_unlock(&trava);
  if (igual) {
    printf("[simkl] library: activities unchanged, saved copy is current\n");
    free(ativ);
    bibPublicar(pedido, NULL, NULL, 1);
    goto fim;
  }
  if (!ativ && tem) {
    // Simkl unreachable: keep the saved copy, ask again on a later show page.
    printf("[simkl] library: activities unavailable, keeping the saved copy\n");
    bibPublicar(pedido, NULL, NULL, 0);
    goto fim;
  }
  // 3. A library and the activities it reflects: only what changed (r 0-2).
  // r 3: no library yet, one without its activities, or one a merge cannot
  // be trusted with: the whole library below.
  if (tem && ativ && bibAtivCopiar(pedido, &corpo, &ativVelho))
    r = bibliotecaAtualizar(pedido, corpo, ativVelho, ativ, &novo, &delta, &st);
  free(corpo);
  free(ativVelho);
  corpo = NULL;
  if (r == 2) {
    bibImportarSeries(delta, pedido.sessao, "changed");
    free(delta);
    if (pedidoTravar(pedido)) {
      simklbib_gravar(c.perfil, conta, &(SimklBib){ ativ, novo });
      pedidoDestravar();
    }
    bibPublicar(pedido, novo, ativ, 1);
  } else if (r == 1) {
    printf("[simkl] library: only playback/ratings/settings moved, nothing to download\n");
    bibPublicar(pedido, NULL, ativ, 1);
  } else if (r == 0) {
    // Nothing saved or published: the next reader asks again.
    printf("[simkl] library: delta failed (HTTP %d), keeping the library\n", st);
    free(ativ);
    bibPublicar(pedido, NULL, NULL, 0);
  } else if (bibliotecaPuxar(pedido, NULL, &corpo, &st)) {
    bibImportarSeries(corpo, pedido.sessao, "downloaded");
    // Without activities the copy is used but not saved: a later delta would
    // have no date to start from.
    if (ativ && pedidoTravar(pedido)) {
      simklbib_gravar(c.perfil, conta, &(SimklBib){ ativ, corpo });
      pedidoDestravar();
    }
    bibPublicar(pedido, corpo, ativ, ativ != NULL);
  } else {
    printf("[simkl] library: download failed (HTTP %d)\n", st);
    free(ativ);
    bibPublicar(pedido, NULL, NULL, 0);
  }
fim:
  fflush(stdout);
  pthread_mutex_lock(&trava);
  if (bibFio && bibFioSessao == pedido.sessao && bibFioGeracao == pedido.geracao)
    bibFio = 0;
  bibVivos--;
  pthread_cond_broadcast(&bibMudou);
  pthread_mutex_unlock(&trava);
  return NULL;
}

// Starts the loader unless one runs for this generation. `velha`: only when
// the library is missing or its last check is SMK_BIB_IDADE old.
static void bibIniciar(SmkPedido pedido, int perfil, int velha) {
  BibCarga *c;
  pthread_t fio;
  time_t agora = time(NULL);
  pthread_mutex_lock(&trava);
  if (pedido.geracao != cacheGeracao || bibFio ||
      (velha && bibDoPedido(pedido) && agora >= bibConferida &&
       agora - bibConferida < SMK_BIB_IDADE)) {
    pthread_mutex_unlock(&trava);
    return;
  }
  bibFio = 1;
  bibFioSessao = pedido.sessao;
  bibFioGeracao = pedido.geracao;
  bibVivos++;
  pthread_mutex_unlock(&trava);
  if ((c = malloc(sizeof *c))) {
    c->pedido = pedido;
    c->perfil = perfil;
    if (pthread_create(&fio, NULL, bibCarregarFio, c) == 0) {
      pthread_detach(fio);
      return;
    }
    free(c);
  }
  pthread_mutex_lock(&trava);
  if (bibFioSessao == pedido.sessao && bibFioGeracao == pedido.geracao) bibFio = 0;
  bibVivos--;
  pthread_cond_broadcast(&bibMudou);
  pthread_mutex_unlock(&trava);
}

int simkl_biblioteca_carregando(void) {
  int r;
  pthread_mutex_lock(&trava);
  r = bibVivos > 0;
  pthread_mutex_unlock(&trava);
  return r;
}

void simkl_biblioteca_iniciar(void) {
  SmkPedido pedido = pedidoCapturar();
  int perfil;
  if (!pedido.aut[0] || (perfil = pedidoPerfil(pedido)) < 0) return;
  bibIniciar(pedido, perfil, 0);
}

// LIST READERS (Continue watching, Saved, the Lists tab) want what Simkl has
// now, as when each of them asked activities on every build: the library
// checked within SMK_BIB_FRESCA, and again after a write from this app. They
// wait for that check; at launch it is the load already on its way.
#ifndef SMK_BIB_FRESCA   // tests set 0: every read asks activities, as before
#define SMK_BIB_FRESCA 60
#endif

static int bibFresca(SmkPedido p) {   // under trava
  time_t agora = time(NULL);
  return bibDoPedido(p) && bibConferida && agora >= bibConferida &&
         agora - bibConferida < SMK_BIB_FRESCA &&
         !__atomic_load_n(&bibSujo, __ATOMIC_ACQUIRE);
}

// The library and the activities it reflects (copies; either may be NULL).
static char *bibAtualizada(SmkPedido pedido, int perfil, char **ativ) {
  struct timespec ate;
  char *copia = NULL;
  int fresca;
  if (ativ) *ativ = NULL;
  pthread_mutex_lock(&trava);
  fresca = bibFresca(pedido);
  pthread_mutex_unlock(&trava);
  if (!fresca) bibIniciar(pedido, perfil, 0);
  clock_gettime(CLOCK_REALTIME, &ate);
  ate.tv_sec += 60;
  pthread_mutex_lock(&trava);
  while (bibFio && bibFioSessao == pedido.sessao && bibFioGeracao == pedido.geracao &&
         pedido.geracao == cacheGeracao)
    if (pthread_cond_timedwait(&bibMudou, &trava, &ate)) break;
  if (bibDoPedido(pedido)) {
    copia = strdup(bibCorpo);
    if (ativ && bibAtiv) *ativ = strdup(bibAtiv);
  }
  pthread_mutex_unlock(&trava);
  return copia;
}

char *simkl_lista_corpo(const char *tipo, const char *estado) {
  SmkPedido pedido = pedidoCapturar();
  char *lib, *r;
  int perfil;
  if (!tipo || !estado || !pedido.aut[0] || (perfil = pedidoPerfil(pedido)) < 0) return NULL;
  lib = bibAtualizada(pedido, perfil, NULL);
  r = bibLista(lib, tipo, estado);
  free(lib);
  return r;
}

// A copy of the session's library, waiting for a loader that is bringing it
// (started here when none is). NULL = none could be had.
static char *bibGarantir(SmkPedido pedido, int perfil) {
  struct timespec ate;
  char *copia = NULL;
  bibIniciar(pedido, perfil, 1);
  clock_gettime(CLOCK_REALTIME, &ate);
  ate.tv_sec += 60;
  pthread_mutex_lock(&trava);
  while (!bibDoPedido(pedido) && bibFio && pedido.geracao == cacheGeracao)
    if (pthread_cond_timedwait(&bibMudou, &trava, &ate)) break;
  if (bibDoPedido(pedido)) copia = strdup(bibCorpo);
  pthread_mutex_unlock(&trava);
  return copia;
}

// 1 when the library has an anime entry with this IMDb ID.
static int bibTemAnime(const char *corpo, const char *imdb) {
  const char *ini = corpo, *fim, *a, *af, *p;
  while (ini && (unsigned char)*ini <= ' ' && *ini) ini++;
  if (!ini || !(fim = js_fim(ini)) || !valorRaiz(ini, fim, "anime", &a, &af) || *a != '[')
    return 0;
  for (p = a + 1; p < af; ) {
    const char *f, *ids, *idsf;
    char x[24] = "";
    while (p < af && ((unsigned char)*p <= ' ' || *p == ',')) p++;
    if (p >= af || *p != '{' || !(f = js_fim(p)) || f > af) break;
    if (itemIds(p, f, &ids, &idsf) && idOutro(ids, idsf, "imdb", x, sizeof x) &&
        !strcmp(x, imdb))
      return 1;
    p = f;
  }
  return 0;
}

// The show page: this show's marks from the library in memory, with no
// request of its own. The anime mapping and the TMDB-only fallback both need
// the opened show, so they happen here.
int simkl_historico_aplicar(const char *imdb) {
  SmkPedido pedido = pedidoCapturar();
  CatItem item;
  char id[24], *corpo;
  long fallbackTmdb = 0;
  int perfil, n;
  if (!pedido.aut[0] || !imdb || !baseImdb(imdb, id, sizeof id) || !id[0]) {
    printf("[vistoep] %s: Simkl history sync skipped (no authenticated session or invalid series ID)\n",
           imdb ? imdb : "");
    fflush(stdout);
    return 0;
  }
  if (cat_copiar_por_id(id, "series", &item)) fallbackTmdb = item.tmdb;
  if ((perfil = pedidoPerfil(pedido)) < 0) {
    printf("[vistoep] %s: Simkl history sync skipped (session changed)\n", id);
    fflush(stdout);
    return 0;
  }
  corpo = bibGarantir(pedido, perfil);
  if (!corpo) {
    printf("[vistoep] %s: Simkl watched history unavailable\n", id);
    fflush(stdout);
    return 0;
  }
  // An anime's marks are mapped here, one by one onto the episode list:
  // until done the page shows no count rather than a climbing one.
  { int anime = bibTemAnime(corpo, id);
    if (anime) vistoep_segurar(id, 1);
    n = simklHistoricoLerCom(corpo, id, id, fallbackTmdb, pedido.sessao);
    if (anime) vistoep_segurar(id, 0); }
  free(corpo);
  // The final list may have been published while this import ran (the page
  // scrolled to episodes early): its hook found nothing mapped yet. After all
  // the show's entries; the same list as used here is a no-op.
  if (n > 0) simkl_anime_lista_mudou(id);
  return n >= 0;
}

int simkl_perfil_ler(const char *settings, const char *itens,
                     PerfilDados *d, time_t agora) {
  const char *u, *uf;
  struct tm mes, primeiro;
  int dias;
  static const char *meses[] = { "Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho",
                                 "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro" };
  if (!d || !perfilHistoricoValido(itens)) return 0;
  memset(d, 0, sizeof *d);
  localtime_r(&agora, &mes);
  snprintf(d->periodo, sizeof d->periodo, "%s %d", i18n(meses[mes.tm_mon]), mes.tm_year + 1900);
  if (settings && objetoRaiz(settings, NULL, "user", &u, &uf)) {
    js_texto_raiz_em(u, uf, "name", d->nome, sizeof d->nome);
    js_texto_raiz_em(u, uf, "avatar", d->avatar, sizeof d->avatar);
  }
  if (itens) {
    perfilLerTipo(itens, "movies", 1, d, &mes);
    perfilLerTipo(itens, "shows", 0, d, &mes);
    perfilLerTipo(itens, "anime", 0, d, &mes);
  }
  dias = 31;
  if (mes.tm_mon == 1) dias = ((mes.tm_year + 1900) % 4 == 0) ? 29 : 28;
  else if (mes.tm_mon == 3 || mes.tm_mon == 5 || mes.tm_mon == 8 || mes.tm_mon == 10) dias = 30;
  d->nDias = dias;
  primeiro = mes; primeiro.tm_mday = 1; primeiro.tm_hour = primeiro.tm_min = primeiro.tm_sec = 0;
  primeiro.tm_isdst = -1; mktime(&primeiro); d->primeiroDiaSemana = primeiro.tm_wday;
  for (int i = 0; i < d->nDias; i++) if (d->atividade[i]) d->diasAtivosMes++;
  for (int i = mes.tm_mday - 1; i >= 0 && i < d->nDias; i--) {
    if (!d->atividade[i]) break;
    d->streakAtual++;
  }
  d->parcial = 1;
  snprintf(d->aviso, sizeof d->aviso,
           "Resumo mensal do histórico disponível no Simkl.");
  return 1;
}

// Profile & Stats' name and avatar (/users/settings), kept while Simkl's
// "settings" activity stays the same. Under `trava`; for one session and
// generation (perfilSettingsSessao/Geracao), dropped by simkl_esquecer.
static char *perfilSettings;
static char perfilSettingsQuando[48];
static uint64_t perfilSettingsSessao, perfilSettingsGeracao;

// The month's stats from the library in memory: it holds every watched
// episode with its date, and perfilLerTipo keeps only this month's. One
// check with Simkl at most (shared with the list readers), and the settings
// only when they moved: an open with nothing changed sends 0 or 1 request
// instead of 2. -1 when there is no library (the caller asks Simkl).
static int perfilDaBiblioteca(PerfilDados *d, time_t agora) {
  SmkPedido pedido = pedidoCapturar();
  char *lib, *ativ = NULL, *settings = NULL, quando[48] = "";
  const char *af, *s, *sf;
  int perfil, st = 0, guardado = 0, ok;
  if (!pedido.aut[0] || (perfil = pedidoPerfil(pedido)) < 0) return -1;
  if (!(lib = bibAtualizada(pedido, perfil, &ativ))) { free(ativ); return -1; }
  if (ativ && (af = js_fim(ativ)) && objetoRaiz(ativ, af, "settings", &s, &sf))
    js_texto_raiz_em(s, sf, "all", quando, sizeof quando);
  if (quando[0] && pedidoTravar(pedido)) {
    if (perfilSettings && perfilSettingsSessao == pedido.sessao &&
        perfilSettingsGeracao == pedido.geracao && !strcmp(perfilSettingsQuando, quando))
      settings = strdup(perfilSettings), guardado = 1;
    pedidoDestravar();
  }
  if (!settings) {
    settings = pegar(pedido, "/users/settings", NULL, &st);
    if (settings && quando[0] && pedidoTravar(pedido)) {
      trocar(&perfilSettings, strdup(settings));
      snprintf(perfilSettingsQuando, sizeof perfilSettingsQuando, "%s", quando);
      perfilSettingsSessao = pedido.sessao;
      perfilSettingsGeracao = pedido.geracao;
      pedidoDestravar();
    }
  }
  ok = pedidoAtual(pedido) && simkl_perfil_ler(settings, lib, d, agora);
  printf("[simkl] profile: month from the library, settings %s\n",
         guardado ? "unchanged" : settings ? "asked" : "unavailable");
  fflush(stdout);
  free(settings); free(lib); free(ativ);
  return ok;
}

int simkl_perfil(PerfilDados *d) {
  char inicio[48], consulta[256], tk[300];
  unsigned long long credencial;
  char *settings = NULL, *itens = NULL;
  time_t agora = time(NULL), limite;
  struct tm primeiro, utc;
  int st = 0, ok;
  if (!d) return 0;
  if ((ok = perfilDaBiblioteca(d, agora)) >= 0) return ok;
  // No library yet (first download failed): the month asked from Simkl.
  credencial = simklauth_credencial_copiar(tk, sizeof tk);
  if (!tk[0]) return 0;
  settings = pedirComToken('G', "/users/settings", NULL, NULL, &st, tk);
  if (st < 200 || st >= 300) { free(settings); settings = NULL; }
  localtime_r(&agora, &primeiro);
  primeiro.tm_mday = 1; primeiro.tm_hour = primeiro.tm_min = primeiro.tm_sec = 0; primeiro.tm_isdst = -1;
  limite = mktime(&primeiro); gmtime_r(&limite, &utc);
  strftime(inicio, sizeof inicio, "%Y-%m-%dT%H:%M:%SZ", &utc);
  snprintf(consulta, sizeof consulta,
           "date_from=%s&extended=full&include_all_episodes=original&episode_watched_at=yes", inicio);
  itens = pedirComToken('G', "/sync/all-items/all/all", consulta, NULL, &st, tk);
  memset(tk, 0, sizeof tk);
  ok = st >= 200 && st < 300 && credencial == simklauth_credencial_copiar(NULL, 0) &&
       simkl_perfil_ler(settings, itens, d, agora);
  free(settings); free(itens);
  return ok;
}

// Assinatura barata de um corpo (FNV-1a). 0 = sem corpo.
static unsigned long assinar(const char *s) {
  unsigned long h = 1469598103UL;
  if (!s || !*s) return 0;
  for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619UL;
  return h ? h : 1;
}

// ------------------------------------------------------------ estado

static volatile int listaEstado;
static int listaFioVivo;
static uint64_t listaSessao;

// Registros de playback da ultima leitura (chave composta -> id), para o
// DELETE, e os ids "tt:S:E" que sao "a seguir". Tabelas laterais pela mesma
// razao de trakt.c: sizeof(CatItem) e o cabecalho do cache em disco.
static struct { char chave[28]; long long id; } play[SMK_PLAY_MAX];
static int nPlay;
static char proxIds[SMK_PLAY_MAX][28];
static int nProx;
// Plan to Watch conhecido: o que a ultima leitura trouxe e o que o "+" daqui
// pos depois dela. E so a guarda do "-" (ver simkl.h).
static char ptw[SMK_PTW_MAX][24];
static int nPtw;

// The lists (watching, plan to watch, completed) come from the library in
// memory, split by status. Playback is not in it: its body is kept while
// Simkl's playback timestamps stay the same (assCw = their signature).
static unsigned long assCw;
static char *corpoPlay;

// The playback timestamps of the activities (tv_shows, anime, movies), as
// Simkl's sync guide asks: marking an episode watched or changing a list
// moves "all" but not these, and the paused items are the same. An answer
// without them (older data, the test fakes) falls back to the whole
// activities, as before.
static unsigned long assinarPlayback(const char *ativ) {
  static const char *const blocos[] = { "tv_shows", "anime", "movies" };
  const char *f, *o, *of, *v, *vf;
  char junto[200] = "";
  size_t usado = 0;
  int achou = 0;
  if (!ativ || !(f = js_fim(ativ))) return assinar(ativ);
  for (int b = 0; b < 3; b++) {
    int n;
    if (!objetoRaiz(ativ, f, blocos[b], &o, &of) || !valorRaiz(o, of, "playback", &v, &vf)) {
      n = snprintf(junto + usado, sizeof junto - usado, "-|");
    } else {
      achou = 1;
      n = snprintf(junto + usado, sizeof junto - usado, "%.*s|", (int)(vf - v), v);
    }
    if (n < 0 || (size_t)n >= sizeof junto - usado) return assinar(ativ);
    usado += (size_t)n;
  }
  return achou ? assinar(junto) : assinar(ativ);
}


static void scrobbleZerar(void);

void simkl_esquecer(void) {
  scrobbleZerar();
  animeHistEsquecer();
  animeAbsMemoEsquecer();
  pthread_mutex_lock(&trava);
  if (!++cacheGeracao) ++cacheGeracao;
  listaFioVivo = 0;
  __atomic_store_n(&listaEstado, SMK_OP_NENHUMA, __ATOMIC_RELEASE);
  nPlay = nProx = nPtw = 0;
  assCw = 0;
  trocar(&corpoPlay, NULL);
  trocar(&perfilSettings, NULL);
  perfilSettingsQuando[0] = 0;
  trocar(&bibCorpo, NULL); trocar(&bibAtiv, NULL);
  bibSessao = bibGeracao = 0;
  bibConferida = 0;
  bibFio = 0;
  pthread_cond_broadcast(&bibMudou);
  memset(simklAnimeIds, 0, sizeof simklAnimeIds);
  simklAnimeIdProximo = 0;
  pthread_mutex_unlock(&trava);
}

void simkl_abrir_sessao(void) { simkl_esquecer(); }

int simkl_e_a_seguir(const char *id) {
  int i, sim = 0;
  if (!id) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nProx && !sim; i++) sim = !strcmp(proxIds[i], id);
  pthread_mutex_unlock(&trava);
  return sim;
}

static int mesmaSerie(const char *a, const char *b) {
  size_t la = idbase_len(a), lb = idbase_len(b);
  return la == lb && !strncmp(a, b, la);
}

// NEXT SEASON OF AN ANIME ALREADY SEEN. Simkl keeps each anime season as its
// own entry: "Cyberpunk: Edgerunners" is completed and "Cyberpunk: Edgerunners
// II" sits in plan to watch with nothing watched and no IMDb ID in the list.
// Its details tie it back: the same IMDb ID and mapped_tvdb_seasons [2]. Once
// that season has aired, its first episode is a next-up like any other.
#define SMK_SEASON_IDS_MAX 400

static int animeTemTitulo(const char *json, const char *imdb) {
  const char *p;
  if (!json || !imdb[0]) return 0;
  for (p = js_array(json, NULL, "anime"); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *oi, *of, *ii, *ifim;
    char id[24] = "";
    if (js_num(p, f, "watched_episodes_count", 0) <= 0) continue;
    if (!objetoRaiz(p, f, "show", &oi, &of) && !objetoRaiz(p, f, "anime", &oi, &of))
      continue;
    if (objetoRaiz(oi, of, "ids", &ii, &ifim)) js_texto(ii, ifim, "imdb", id, sizeof id);
    if (!strcmp(id, imdb)) return 1;
  }
  return 0;
}

// The first season number in "mapped_tvdb_seasons": [2]. js_array only walks
// objects and strings, so the number is read by hand inside the object.
static int primeiraTemporadaMapeada(const char *ini, const char *fim) {
  static const char chave[] = "\"mapped_tvdb_seasons\"";
  const char *p = ini;
  size_t n = sizeof chave - 1;
  while (p && p + n < fim && (p = strstr(p, chave)) && p < fim) {
    char *e;
    long t;
    p += n;
    while (p < fim && (*p == ' ' || *p == ':' || *p == '\n' || *p == '\t')) p++;
    if (p >= fim || *p != '[') return 0;
    t = strtol(p + 1, &e, 10);
    return e != p + 1 && t > 0 && t <= SHRT_MAX ? (int)t : 0;
  }
  return 0;
}

static int animeTemporadaNova(SmkPedido pedido, long simkl, CatItem *d, int *temporada,
                              long long *estreiaMs) {
  char imdb[24] = "", poster[128] = "", quando[40] = "";
  const char *obra, *fim, *ii, *ifim;
  char *corpo;
  *temporada = 0;
  *estreiaMs = 0;
  corpo = animeDetalheCorpo(pedido, simkl, &obra, &fim);
  if (!corpo) return 0;
  if (objetoRaiz(obra, fim, "ids", &ii, &ifim)) {
    js_texto(ii, ifim, "imdb", imdb, sizeof imdb);
    d->tmdb = (long)js_num(ii, ifim, "tmdb", 0);
  }
  *temporada = primeiraTemporadaMapeada(obra, fim);
  if (js_texto_raiz_em(obra, fim, "first_aired", quando, sizeof quando) && quando[0])
    *estreiaMs = js_ms_iso(quando);
  js_texto_raiz_em(obra, fim, "title", d->titulo, sizeof d->titulo);
  js_texto_raiz_em(obra, fim, "poster", poster, sizeof poster);
  if (slugPosterSimkl(poster))
    snprintf(d->poster, sizeof d->poster, "https://simkl.in/posters/%s_m.jpg", poster);
  free(corpo);
  if (!imdbValido(imdb)) return 0;
  snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
  return 1;
}

// Slug of a Simkl episode image ("17/17570079032e93ce31"): same rule as posters.
// The card's episode name and still come from Simkl's own episode list, for
// episode `numero` of that Simkl entry. The card numbers the season as TVDB
// does (Bleach's Thousand-Year Blood War is S17, Kaiju No. 8 Season 2 is S2),
// which Cinemeta, where the card decoration looks, may not have at all: it
// lists Kaiju No. 8 as one 23-episode season. `*temporadaTvdb` and
// `*episodioTvdb` get the episode's own TVDB placement, 0 when absent.
static void animeEpisodio(SmkPedido pedido, long simkl, int numero, CatItem *d,
                          int *temporadaTvdb, int *episodioTvdb) {
  char chave[64], caminho[80], img[128] = "";
  const char *p;
  char *corpo;
  int st = 0;
  *temporadaTvdb = *episodioTvdb = 0;
  snprintf(chave, sizeof chave, "simkl-anime-eps/%ld", simkl);
  corpo = fichameta_ler(chave, 7 * 24 * 3600);
  if (!corpo) {
    if (!pedidoAtual(pedido)) return;
    snprintf(caminho, sizeof caminho, "/anime/episodes/%ld", simkl);
    corpo = pedir(pedido, 'G', caminho, NULL, NULL, &st);
    if (!corpo || st < 200 || st >= 300 || !pedidoAtual(pedido)) {
      if (st == 404) fichameta_gravar(chave, "[]");
      free(corpo);
      return;
    }
    fichameta_gravar(chave, corpo);
  }
  for (p = js_raiz_array(corpo); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *tv, *tvf;
    char tipo[16] = "";
    js_texto_raiz_em(p, f, "type", tipo, sizeof tipo);
    if ((tipo[0] && strcmp(tipo, "episode")) || (int)js_num(p, f, "episode", 0) != numero)
      continue;
    js_texto_raiz_em(p, f, "title", d->nomeEpisodio, sizeof d->nomeEpisodio);
    js_texto_raiz_em(p, f, "img", img, sizeof img);
    if (slugPosterSimkl(img))
      snprintf(d->thumbEpisodio, sizeof d->thumbEpisodio,
               "https://simkl.in/episodes/%s_w.jpg", img);
    if (objetoRaiz(p, f, "tvdb", &tv, &tvf)) {
      int t = (int)js_num(tv, tvf, "season", 0), e = (int)js_num(tv, tvf, "episode", 0);
      if (t > 0 && t <= SHRT_MAX && e > 0 && e <= SHRT_MAX) {
        *temporadaTvdb = t;
        *episodioTvdb = e;
      }
    }
    break;
  }
  free(corpo);
}

static int animeTemporadasNovas(SmkPedido pedido, const char *ptw, const char *feito,
                                const char *assistindo, CatItem *dst, int max) {
  const char *p;
  int n = 0, vistos = 0;
  long long agora = (long long)time(NULL) * 1000LL;
  if (!ptw || !dst || max <= 0) return 0;
  for (p = js_array(ptw, NULL, "anime"); p && *p == '{' && n < max;
       p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *oi, *of, *ii, *ifim;
    CatItem *d = &dst[n];
    char imdbLista[24] = "";
    long simkl = 0;
    int temporada = 0;
    long long estreia = 0;
    if (js_num(p, f, "watched_episodes_count", 0) > 0) continue;
    if (!objetoRaiz(p, f, "show", &oi, &of) && !objetoRaiz(p, f, "anime", &oi, &of))
      continue;
    if (objetoRaiz(oi, of, "ids", &ii, &ifim)) {
      js_texto(ii, ifim, "imdb", imdbLista, sizeof imdbLista);
      simkl = (long)js_num(ii, ifim, "simkl", 0);
    }
    // With an IMDb ID already in the list, a show never watched is not a
    // next season: no details request for it.
    if (imdbLista[0] && !animeTemTitulo(feito, imdbLista) &&
        !animeTemTitulo(assistindo, imdbLista)) continue;
    if (simkl <= 0 || ++vistos > SMK_SEASON_IDS_MAX) continue;
    memset(d, 0, sizeof *d);
    if (!animeTemporadaNova(pedido, simkl, d, &temporada, &estreia)) continue;
    if (temporada <= 1 || estreia <= 0 || estreia > agora) continue;
    if (!animeTemTitulo(feito, d->imdb) && !animeTemTitulo(assistindo, d->imdb)) continue;
    { int temporadaEp = 0;
      int episodioEp = 0;
      animeEpisodio(pedido, simkl, 1, d, &temporadaEp, &episodioEp);
      if (episodioEp != 1) temporadaEp = 0;
      // The episode's own TVDB placement is more precise than the show's.
      if (temporadaEp > 1) temporada = temporadaEp; }
    snprintf(d->tipo, sizeof d->tipo, "%s", "series");
    arte_metahub_preencher(d);
    d->temporada = temporada;
    d->episodio = 1;
    d->progresso = 0;
    d->retomadoMs = estreia;
    { char base[24];
      snprintf(base, sizeof base, "%s", d->imdb);
      snprintf(d->imdb, sizeof d->imdb, "%s:%d:1", base, temporada); }
    printf("[simkl] anime next season id=%s simkl=%ld: candidate\n", d->imdb, simkl);
    n++;
  }
  return n;
}

// STARTED BUT FILED AS PLAN TO WATCH. A show keeps all its seasons in one
// Simkl entry, and that entry can sit in plan to watch after seasons were
// watched (Monster: S01-S02 seen, status plantowatch, next_to_watch S03E01).
// Such entries join the watching list; untouched plan-to-watch titles do not.
// Returns {"<raiz>":[watching items, started plan-to-watch items]}.
static char *juntarComecados(const char *assistindo, const char *ptw, const char *raiz) {
  const char *p;
  size_t cap = 64 + (assistindo ? strlen(assistindo) : 0) + (ptw ? strlen(ptw) : 0), n;
  char *r = malloc(cap);
  int itens = 0, extras = 0;
  if (!r) return assistindo ? strdup(assistindo) : NULL;
  n = (size_t)snprintf(r, cap, "{\"%s\":[", raiz);
  for (int lado = 0; lado < 2; lado++) {
    const char *json = lado ? ptw : assistindo;
    for (p = json ? js_array(json, NULL, raiz) : NULL; p && *p == '{'; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      char prox[16] = "";
      if (!f) break;
      if (lado && (js_num(p, f, "watched_episodes_count", 0) <= 0 ||
                   !js_texto_raiz_em(p, f, "next_to_watch", prox, sizeof prox) || !prox[0]))
        continue;
      if (itens) r[n++] = ',';
      memcpy(r + n, p, (size_t)(f - p));
      n += (size_t)(f - p);
      itens++;
      extras += lado;
    }
  }
  memcpy(r + n, "]}", 3);
  if (extras)
    printf("[simkl] %s: %d started title(s) from plan to watch join Continue watching\n",
           raiz, extras);
  return r;
}

// The library item with this Simkl ID; *lista 0 shows, 1 anime, 2 movies.
static int bibAchar(const char *lib, long simkl, const char **ini, const char **fim,
                    int *lista) {
  const char *raiz = lib, *rfim, *v, *vf, *p;
  if (!lib || simkl <= 0) return 0;
  while ((unsigned char)*raiz <= ' ') raiz++;
  if (!(rfim = js_fim(raiz))) return 0;
  for (int k = 0; k < 3; k++) {
    if (!valorRaiz(raiz, rfim, bibTipos[k], &v, &vf) || *v != '[') continue;
    for (p = v + 1; p < vf; ) {
      while (p < vf && ((unsigned char)*p <= ' ' || *p == ',')) p++;
      if (p >= vf || *p != '{') break;
      const char *f = js_fim(p);
      if (!f || f > vf) break;
      if (itemSimkl(p, f) == simkl) { *ini = p; *fim = f; *lista = k; return 1; }
      p = f;
    }
  }
  return 0;
}

// Why a paused row no longer belongs in Continue watching, or NULL, from the
// title's library item (Simkl keeps the playback row; Nuvio Mobile drops it the
// same way). A title not in the library hides nothing. A row marked watched
// needs no check: Simkl hides it by itself.
//  - the title is on hold or dropped;
//  - it was completed after the pause;
//  - a show was watched after the pause and Simkl's next episode is past the
//    paused one: the next-episode card takes its place.
// Anime keep only the first rule: their next_to_watch may use another
// numbering than the paused episode.
static const char *playbackVencido(const CatItem *d, long simkl, const char *lib) {
  const char *p, *f;
  char estado[24] = "", quando[40] = "", prox[16] = "";
  int lista, t = 0, e = 0;
  long long visto;
  if (!bibAchar(lib, simkl, &p, &f, &lista)) return NULL;
  js_texto_raiz_em(p, f, "status", estado, sizeof estado);
  if (!strcmp(estado, "hold")) return "on hold";
  if (!strcmp(estado, "dropped")) return "dropped";
  if (lista == 1 || d->retomadoMs <= 0) return NULL;
  visto = js_texto_raiz_em(p, f, "last_watched_at", quando, sizeof quando) && quando[0]
            ? js_ms_iso(quando) : 0;
  if (visto < d->retomadoMs) return NULL;
  if (!strcmp(estado, "completed")) return "completed after the pause";
  if (d->episodio > 0 && js_texto_raiz_em(p, f, "next_to_watch", prox, sizeof prox) &&
      sscanf(prox, "S%dE%d", &t, &e) == 2 &&
      (t > d->temporada || (t == d->temporada && e > d->episodio)))
    return "watched past it after the pause";
  return NULL;
}

int simkl_continuar(CatItem *saida, int max) {
  CatItem *prox;
  long long ids[SMK_PLAY_MAX];
  long simklIds[SMK_PLAY_MAX] = {0};
  int animeAbsolutos[SMK_PLAY_MAX] = {0};
  int animeTemporadas[SMK_PLAY_MAX] = {0};   // per-season entries: their season
  unsigned long h;
  char *cPlay = NULL, *cAssist = NULL, *cAnime = NULL;
  char *cAnimePtw = NULL, *cAnimeFeito = NULL, *cShowsPtw = NULL;
  char proxLocal[SMK_PLAY_MAX][28]; int nProxLocal = 0;
  int n, nP, nA, i, k;
  SmkPedido pedido = pedidoCapturar();
  char *lib, *ativ = NULL;
  int perfil;
  if (!saida || max <= 0 || !pedido.aut[0]) return 0;
  if (max > SMK_PLAY_MAX) max = SMK_PLAY_MAX;
  if ((perfil = pedidoPerfil(pedido)) < 0) return 0;
  // The lists come from the library (status watching / plantowatch /
  // completed); only playback is not in it. Playback is asked again when its
  // own timestamps move (assinarPlayback) or after a write from this app.
  lib = bibAtualizada(pedido, perfil, &ativ);
  h = assinarPlayback(ativ);
  // A write from this app asks again whatever the timestamps say.
  if (!__atomic_exchange_n(&playSujo, 0, __ATOMIC_ACQ_REL) && pedidoTravar(pedido)) {
    if (h && h == assCw && corpoPlay) {
      cPlay = strdup(corpoPlay);
      printf("[simkl] resume: playback unchanged, cached playback\n");
    }
    pedidoDestravar();
  }
  if (!cPlay) {
    int st = 0;
    cPlay = pegar(pedido, "/sync/playback", NULL, &st);
    if (pedidoTravar(pedido)) {
      if (h && cPlay) { trocar(&corpoPlay, strdup(cPlay)); assCw = h; }
      else assCw = 0;
      pedidoDestravar();
    }
  }
  cAssist = bibLista(lib, "shows", "watching");
  // Anime e um TERCEIRO tipo no Simkl. Sem esta lista, nenhum anime aparece
  // no Continuar assistindo.
  cAnime = bibLista(lib, "anime", "watching");
  if (!cAnime) printf("[simkl] anime watching: no library yet, no anime in Continue watching\n");
  if (ajustes_cw_proximo()) {
    cAnimePtw = bibLista(lib, "anime", "plantowatch");
    cAnimeFeito = bibLista(lib, "anime", "completed");
    cShowsPtw = bibLista(lib, "shows", "plantowatch");
  }
  free(ativ);
  if (!pedidoAtual(pedido)) {
    free(cPlay); free(cAssist); free(cAnime); free(cAnimePtw); free(cAnimeFeito);
    free(cShowsPtw); free(lib);
    return 0;
  }
  if (cShowsPtw && cAssist) {
    char *junto = juntarComecados(cAssist, cShowsPtw, "shows");
    if (junto) { free(cAssist); cAssist = junto; }
  }
  if (cAnimePtw && cAnime) {
    char *junto = juntarComecados(cAnime, cAnimePtw, "anime");
    if (junto) { free(cAnime); cAnime = junto; }
  }
  {
    long playSimkl[SMK_PLAY_MAX];
    n = lerPlayback(cPlay, saida, ids, playSimkl, max);
    for (i = k = 0; i < n; i++) {
      const char *motivo = playbackVencido(&saida[i], playSimkl[i], lib);
      if (motivo) {
        printf("[simkl] paused id=%s: dropped (%s)\n", saida[i].imdb, motivo);
        continue;
      }
      if (k != i) { saida[k] = saida[i]; ids[k] = ids[i]; }
      k++;
    }
    n = k;
  }
  free(lib);
  int nPausados = n;
  prox = malloc(sizeof(CatItem) * SMK_PLAY_MAX);
  nP = prox && ajustes_cw_proximo() ? simkl_ler_assistindo(cAssist, prox, SMK_PLAY_MAX) : 0;
  // O mesmo leitor aceita a raiz "anime"; funde aqui, dedup por serie, para
  // nao repetir um titulo que o endpoint de shows ja tenha trazido.
  if (!prox || !ajustes_cw_proximo())
    printf("[simkl] anime watching: skipped (%s)\n", prox ? "next-up disabled" : "allocation failed");
  if (prox && ajustes_cw_proximo()) {
    nA = simkl_ler_assistindo_com(cAnime, prox + nP, SMK_PLAY_MAX - nP,
                                  animeAbsolutos, simklIds, animeTemporadas);
    for (k = nP; k < nP + nA; k++) {
      CatItem *anime = &prox[k];
      int absoluto = animeAbsolutos[k - nP], temporada, episodio, temporadaSimkl = 0;
      long simklId = simklIds[k - nP];
      int somenteSimkl = !strncmp(anime->imdb, "simkl:", 6);
      int temporadaUnica = animeTemporadas[k - nP];
      // Placed from its own episode list: no IMDb ID (simkl:), or one Simkl
      // entry for one later season that carries the show's IMDb ID.
      int porTemporada = somenteSimkl || temporadaUnica > 0;
      if (simklId > 0) simklAnimeIdGuardar(pedido, simklId, anime->imdb);
      if (somenteSimkl || simklId > 0) {
        simkl_anime_detalhe(pedido, anime, &temporadaSimkl, simklId);
      }
      if (absoluto > 0) {
        int animeMapeado = 0;
        if (!temporadaUnica &&
            (!strncmp(anime->imdb, "tt", 2) || !strncmp(anime->imdb, "tmdb:", 5))) {
          char idBase[24];
          if (baseImdb(anime->imdb, idBase, sizeof idBase))
            animeMapeado = animeAbsoluto(idBase, absoluto, &temporada, &episodio,
                                         anime->thumbEpisodio,
                                         sizeof anime->thumbEpisodio);
        }
        if (porTemporada && (temporadaUnica > 0 || temporadaSimkl > 0)) {
          // One Simkl entry per season (Kaiju No. 8 Season 2): name, still and
          // the episode's TVDB placement from that entry's episode list.
          int tEp = 0, eEp = 0;
          temporada = temporadaUnica > 0 ? temporadaUnica : temporadaSimkl;
          episodio = absoluto;
          if (simklId > 0) {
            animeEpisodio(pedido, simklId, absoluto, anime, &tEp, &eEp);
            if (tEp > 0 && eEp > 0) { temporada = tEp; episodio = eEp; }
          }
        } else if (animeMapeado) {
          // Map a series-wide absolute number through the provider metadata.
        } else {
          temporada = anime->temporada;
          episodio = anime->episodio;
        }
      } else {
        temporada = anime->temporada;
        episodio = anime->episodio;
        if (temporadaSimkl > 0 && temporada <= 1 && episodio > 0)
          temporada = temporadaSimkl;
      }
      if (episodio > 0) {
        char base[32];
        if (!baseImdb(anime->imdb, base, sizeof base))
          snprintf(base, sizeof base, "%.*s", (int)sizeof base - 1, anime->imdb);
        anime->temporada = temporada;
        anime->episodio = episodio;
        snprintf(anime->imdb, sizeof anime->imdb, "%s:%d:%d",
                 base, temporada, episodio);
      }
      if (somenteSimkl || simklId > 0)
        printf("[simkl] anime details id=%ld title=%s poster=%s\n",
               simklId, anime->titulo[0] ? "resolved" : "unchanged",
               anime->poster[0] ? "yes" : "no");
      int j, ja = 0;
      for (j = 0; j < nP && !ja; j++) ja = mesmaSerie(prox[j].imdb, anime->imdb);
      if (ja) {
        printf("[simkl] anime watching id=%s: dropped (duplicate show)\n", anime->imdb);
        memmove(prox + k, prox + k + 1, sizeof prox[0] * (size_t)(nP + nA - k - 1));
        // The side arrays follow, or every later anime reads the dropped
        // one's absolute number and Simkl ID.
        memmove(animeAbsolutos + (k - nP), animeAbsolutos + (k - nP) + 1,
                sizeof animeAbsolutos[0] * (size_t)(nP + nA - k - 1));
        memmove(simklIds + (k - nP), simklIds + (k - nP) + 1,
                sizeof simklIds[0] * (size_t)(nP + nA - k - 1));
        memmove(animeTemporadas + (k - nP), animeTemporadas + (k - nP) + 1,
                sizeof animeTemporadas[0] * (size_t)(nP + nA - k - 1));
        nA--; k--;
      }
    }
    nP += nA;
    // Appended after the watching entries: a show already there wins.
    if (cAnimePtw && nP < SMK_PLAY_MAX) {
      int nT = animeTemporadasNovas(pedido, cAnimePtw, cAnimeFeito, cAnime,
                                    prox + nP, SMK_PLAY_MAX - nP);
      for (k = nP; k < nP + nT; k++) {
        int j, ja = 0;
        for (j = 0; j < k && !ja; j++) ja = mesmaSerie(prox[j].imdb, prox[k].imdb);
        if (ja) {
          printf("[simkl] anime next season id=%s: dropped (duplicate show)\n", prox[k].imdb);
          memmove(prox + k, prox + k + 1, sizeof prox[0] * (size_t)(nP + nT - k - 1));
          nT--; k--;
        }
      }
      nP += nT;
    }
  }
  for (k = 0; k < nP; k++) {
    int ja = 0, alvo;
    for (i = 0; i < n && !ja; i++) ja = mesmaSerie(saida[i].imdb, prox[k].imdb);
    if (ja) {
      printf("[simkl] watching id=%s: dropped (already in resume row)\n", prox[k].imdb);
      continue;
    }
    if (n < max) alvo = n++;
    else {
      int vel = -1;
      for (i = 0; i < n; i++)
        if (saida[i].progresso == 0 &&
            (vel < 0 || saida[i].retomadoMs < saida[vel].retomadoMs)) vel = i;
      if (vel < 0 || saida[vel].retomadoMs >= prox[k].retomadoMs) {
        printf("[simkl] watching id=%s: dropped (resume row limit)\n", prox[k].imdb);
        continue;
      }
      alvo = vel;
    }
    saida[alvo] = prox[k];
    if (nProxLocal < SMK_PLAY_MAX)
      snprintf(proxLocal[nProxLocal++], sizeof proxLocal[0], "%s", prox[k].imdb);
  }
  free(prox); free(cPlay); free(cAssist); free(cAnime);
  free(cAnimePtw); free(cAnimeFeito); free(cShowsPtw);
  if (!pedidoTravar(pedido)) return 0;
  nPlay = 0;
  for (i = 0; i < nPausados; i++)
    if (ids[i] > 0 && nPlay < SMK_PLAY_MAX) {
      snprintf(play[nPlay].chave, sizeof play[nPlay].chave, "%s", saida[i].imdb);
      play[nPlay++].id = ids[i];
    }
  nProx = nProxLocal;
  memcpy(proxIds, proxLocal, sizeof proxIds[0] * (size_t)nProx);
  pedidoDestravar();
  { int a, b;
    for (a = 1; a < n; a++) {
      CatItem t = saida[a];
      for (b = a - 1; b >= 0 && saida[b].retomadoMs < t.retomadoMs; b--)
        saida[b + 1] = saida[b];
      saida[b + 1] = t;
    }
  }
  printf("[simkl] retomada: %d pausado(s) + %d a seguir candidato(s) -> %d\n", nPausados, nP, n);
  fflush(stdout);
  if (n) n = trakt_enfeitar_lote_sessao ? trakt_enfeitar_lote_sessao(saida, n, pedido.sessao)
                                       : trakt_enfeitar_lote(saida, n);
  return pedidoAtual(pedido) ? n : 0;
}

int simkl_plantowatch(CatItem *saida, int max) {
  char *cF, *cS, *cA, *lib;
  int n = 0, nA, i, j, perfil;
  SmkPedido pedido = pedidoCapturar();
  if (!saida || max <= 0 || !pedido.aut[0]) return 0;
  if ((perfil = pedidoPerfil(pedido)) < 0) return 0;
  lib = bibAtualizada(pedido, perfil, NULL);
  cF = bibLista(lib, "movies", "plantowatch");
  cS = bibLista(lib, "shows", "plantowatch");
  // Anime e o terceiro tipo do Simkl: vai para o lado das series.
  cA = bibLista(lib, "anime", "plantowatch");
  free(lib);
  if (!cF || !cS || !cA) {
    free(cF); free(cS); free(cA);
    return -1;
  }
  n = simkl_ler_plantowatch(cF, 0, saida, max);
  n += simkl_ler_plantowatch(cS, 1, saida + n, max - n);
  nA = simkl_ler_plantowatch(cA, 1, saida + n, max - n);
  // Dedup por imdb: um titulo em shows e anime ao mesmo tempo nao entra 2x.
  for (i = n; i < n + nA; i++) {
    for (j = 0; j < n; j++) if (!strcmp(saida[j].imdb, saida[i].imdb)) break;
    if (j < n) memmove(saida + i, saida + i + 1, sizeof saida[0] * (size_t)(n + nA - i - 1)), nA--, i--;
  }
  n += nA;
  if (cF && cS && cA && pedidoTravar(pedido)) {
    nPtw = 0;
    for (i = 0; i < n && nPtw < SMK_PTW_MAX; i++)
      snprintf(ptw[nPtw++], sizeof ptw[0], "%s", saida[i].imdb);
    pedidoDestravar();
  }
  free(cF); free(cS); free(cA);
  printf("[simkl] plantowatch: %d\n", n); fflush(stdout);
  return pedidoAtual(pedido) ? n : 0;
}

int simkl_na_plantowatch(const char *imdb) {
  int i, sim = 0;
  size_t L;
  if (!imdb || !imdb[0]) return 0;
  L = strcspn(imdb, ":");
  pthread_mutex_lock(&trava);
  for (i = 0; i < nPtw && !sim; i++)
    sim = strlen(ptw[i]) == L && !strncmp(ptw[i], imdb, L);
  pthread_mutex_unlock(&trava);
  return sim;
}

// ------------------------------------------------------------ escritas

typedef struct { char id[24], tipo[8]; int adicionar; SmkPedido pedido; } SmkLista;
static void ptwMarcar(const char *id, int dentro) { // caller owns pedidoTravar
  int i;
  for (i = 0; i < nPtw; i++) if (!strcmp(ptw[i], id)) break;
  if (dentro && i == nPtw && nPtw < SMK_PTW_MAX) snprintf(ptw[nPtw++], sizeof ptw[0], "%s", id);
  if (!dentro && i < nPtw) { memmove(ptw[i], ptw[i + 1], sizeof ptw[0] * (size_t)(nPtw - i - 1)); nPtw--; }
}
static void *fioLista(void *u) {
  SmkLista *a = u; char corpo[200], *r; int st = 0, ok;
  ok = simkl_corpo_lista(corpo, sizeof corpo, a->id, a->tipo, a->adicionar);
  r = ok ? pedir(a->pedido, 'P', simkl_rota_lista(a->adicionar), NULL, corpo, &st) : NULL;
  ok = st >= 200 && st < 300;
  if (pedidoTravar(a->pedido)) {
    if (ok) ptwMarcar(a->id, a->adicionar);
    __atomic_store_n(&listaEstado, ok ? SMK_OP_CONFIRMADA : SMK_OP_FALHA, __ATOMIC_RELEASE);
    listaFioVivo = 0;
    pedidoDestravar();
  }
  printf("[simkl] plantowatch %s %s (%s) -> HTTP %d\n", a->adicionar ? "add" : "del", a->id, a->tipo, st);
  fflush(stdout); free(r); memset(a, 0, sizeof *a); free(a);
  return NULL;
}
int simkl_lista_tipo(const char *imdb, const char *tipo, int adicionar) {
  pthread_t t;
  if (!imdbValido(imdb)) { __atomic_store_n(&listaEstado, SMK_OP_FALHA, __ATOMIC_RELEASE); return 0; }
  SmkLista *a = calloc(1, sizeof *a); if (!a) return 0;
  a->pedido = pedidoCapturar();
  if (!a->pedido.aut[0]) {
    if (pedidoTravar(a->pedido)) { __atomic_store_n(&listaEstado, SMK_OP_FALHA, __ATOMIC_RELEASE); pedidoDestravar(); }
    free(a); return 0;
  }
  if (!pedidoTravar(a->pedido)) { free(a); return 0; }
  int dentro = 0; size_t k = strcspn(imdb, ":");
  for (int i = 0; i < nPtw && !dentro; i++) dentro = strlen(ptw[i]) == k && !strncmp(ptw[i], imdb, k);
  if ((!adicionar && !dentro) || (listaFioVivo && listaSessao == a->pedido.sessao)) {
    if (!adicionar && !dentro) __atomic_store_n(&listaEstado, SMK_OP_FALHA, __ATOMIC_RELEASE);
    pedidoDestravar(); free(a); return 0;
  }
  if (k >= sizeof a->id) k = sizeof a->id - 1;
  memcpy(a->id, imdb, k); a->id[k] = 0;
  snprintf(a->tipo, sizeof a->tipo, "%s",
           (tipo && !strcmp(tipo, "series")) || strchr(imdb, ':') ? "series" : "movie");
  a->adicionar = adicionar;
  listaFioVivo = 1; listaSessao = a->pedido.sessao;
  __atomic_store_n(&listaEstado, SMK_OP_PENDENTE, __ATOMIC_RELEASE);
  int ok = pthread_create(&t, NULL, fioLista, a) == 0;
  if (ok) pthread_detach(t);
  else { listaFioVivo = 0; __atomic_store_n(&listaEstado, SMK_OP_FALHA, __ATOMIC_RELEASE); free(a); }
  pedidoDestravar(); return ok;
}
// NOTA (/sync/ratings): mesmo corpo do Trakt, um fio solto por resposta.
typedef struct { char id[24], tipo[8]; int nota; SmkPedido pedido; } SmkNota;
static void *fioNota(void *u) {
  SmkNota *a = u; char corpo[200], *r; int st = 0;
  snprintf(corpo, sizeof corpo, "{\"%s\":[{\"rating\":%d,\"ids\":{\"imdb\":\"%s\"}}]}",
           !strcmp(a->tipo, "series") ? "shows" : "movies", a->nota, a->id);
  r = pedir(a->pedido, 'P', a->nota ? "/sync/ratings" : "/sync/ratings/remove", NULL, corpo, &st);
  printf("[simkl] nota %d %s (%s) -> HTTP %d\n", a->nota, a->id, a->tipo, st);
  fflush(stdout); free(r); memset(a, 0, sizeof *a); free(a);
  return NULL;
}
int simkl_avaliar(const char *imdb, const char *tipo, int nota) {
  pthread_t t;
  if (!imdbValido(imdb) || strchr(imdb, ':') || nota < 0 || nota > 10) return 0;
  SmkNota *a = calloc(1, sizeof *a); if (!a) return 0;
  a->pedido = pedidoCapturar();
  if (!a->pedido.aut[0]) { free(a); return 0; }
  { size_t k = strcspn(imdb, ":"); if (k >= sizeof a->id) k = sizeof a->id - 1;
    memcpy(a->id, imdb, k); a->id[k] = 0; }
  snprintf(a->tipo, sizeof a->tipo, "%s", tipo && !strcmp(tipo, "series") ? "series" : "movie");
  a->nota = nota;
  if (pthread_create(&t, NULL, fioNota, a) != 0) { free(a); return 0; }
  pthread_detach(t); return 1;
}
int simkl_lista_estado(void) { return __atomic_load_n(&listaEstado, __ATOMIC_ACQUIRE); }

// POST do historico, SINCRONO (quem chama ja esta num fio: visto.c). 2xx
// sozinho nao prova nada: a doc manda olhar `not_found`, que volta com os
// objetos que o servidor nao resolveu. Um `"ids"` dentro dele e titulo que o
// Simkl nao achou pelo imdb — conta como falha, e o log diz.
// `qtd` > 0: lote de episodios; 0: titulo inteiro (`serie` diz qual).
static int historicoOk(char *r, int st, int visto, const char *id, int qtd, int serie) {
  int ok = st >= 200 && st < 300;
  const char *nf = r ? strstr(r, "\"not_found\"") : NULL;
  if (ok && nf && strstr(nf, "\"ids\"")) ok = 0;
  if (qtd > 0) printf("[simkl] historico %s %d eps de %s", visto ? "add" : "del", qtd, id);
  else printf("[simkl] historico %s %s %s", visto ? "add" : "del", serie ? "serie" : "filme", id);
  printf(" -> %s (HTTP %d)%s\n", ok ? "ok" : "falhou", st,
         nf && strstr(nf, "\"ids\"") ? " [nao encontrado no Simkl]" : "");
  fflush(stdout);
  free(r);
  return ok;
}
static int postarHistoricoComToken(const char *corpo, int visto, const char *id, int qtd,
                                   int serie, const char *tk) {
  int st = 0;
  char *r = pedirComToken('P', simkl_rota_historico(visto), NULL, corpo, &st, tk);
  return historicoOk(r, st, visto, id, qtd, serie);
}
static int postarHistorico(SmkPedido pedido, const char *corpo, int visto, const char *id, int qtd, int serie) {
  int st = 0;
  char *r = pedir(pedido, 'P', simkl_rota_historico(visto), NULL, corpo, &st);
  return historicoOk(r, st, visto, id, qtd, serie);
}

// One history POST at a time. Watched-state workers and a scrobble fallback
// share this; the frozen SimklDestino still names the account.
static pthread_mutex_t travaCorpo = PTHREAD_MUTEX_INITIALIZER;
int simkl_episodios_marcar_destino(const SimklDestino *destino, const char *imdb, const VistoPar *pares, int qtd, int visto) {
  char corpo[SMK_LOTE_MAX * 20 + 200];
  int ok;
  if (!destino || !destino->aut[0] || !imdb) return 0;
  SmkPedido pedido = *destino;
  pthread_mutex_lock(&travaCorpo);
  if (!simkl_corpo_historico_eps(corpo, sizeof corpo, imdb, pares, qtd)) {
    pthread_mutex_unlock(&travaCorpo);
    return 0;
  }
  ok = postarHistorico(pedido, corpo, visto, imdb, qtd > SMK_LOTE_MAX ? SMK_LOTE_MAX : qtd, 1);
  pthread_mutex_unlock(&travaCorpo);
  return ok;
}

int simkl_titulo_marcar_destino(const SimklDestino *destino, const char *imdb, const char *tipo, const int *temporadas,
                        int nt, int visto) {
  char corpo[1200];
  if (!destino || !destino->aut[0] || !imdb) return 0;
  SmkPedido pedido = *destino;
  if (!simkl_corpo_historico_titulo(corpo, sizeof corpo, imdb, tipo, temporadas, nt, visto)) {
    printf("[simkl] historico %s %s: sem corpo seguro (serie sem temporadas?); nada mandado\n",
           visto ? "add" : "del", imdb);
    fflush(stdout);
    return 0;
  }
  return postarHistorico(pedido, corpo, visto, imdb, 0, tipo && !strcmp(tipo, "series"));
}

int simkl_destino_capturar(SimklDestino *d) {
  if (!d) return 0;
  *d = pedidoCapturar();
  return d->aut[0] != 0;
}
int simkl_episodios_marcar(const char *imdb, const VistoPar *pares, int qtd, int visto) {
  SimklDestino d;
  return simkl_destino_capturar(&d) && simkl_episodios_marcar_destino(&d, imdb, pares, qtd, visto);
}
int simkl_titulo_marcar(const char *imdb, const char *tipo, const int *temporadas, int nt, int visto) {
  SimklDestino d;
  return simkl_destino_capturar(&d) && simkl_titulo_marcar_destino(&d, imdb, tipo, temporadas, nt, visto);
}

typedef struct { long long id; SmkPedido pedido; } SmkApagar;
static void *fioApagar(void *u) {
  SmkApagar *a = u; char caminho[64], *r; int st = 0, ok;
  snprintf(caminho, sizeof caminho, "/sync/playback/%lld", a->id);
  r = pedir(a->pedido, 'D', caminho, NULL, NULL, &st);
  ok = st >= 200 && st < 300; free(r);
  if (ok && pedidoTravar(a->pedido)) {
    for (int i = 0; i < nPlay; i++) if (play[i].id == a->id) { play[i].chave[0] = 0; break; }
    assCw = 0; pedidoDestravar();
  }
  printf("[simkl] playback remover id %lld -> HTTP %d\n", a->id, st); fflush(stdout);
  memset(a, 0, sizeof *a); free(a); return NULL;
}
int simkl_playback_remover(const char *imdb) {
  pthread_t t;
  if (!imdb || !imdb[0]) return 0;
  SmkApagar *a = calloc(1, sizeof *a); if (!a) return 0;
  a->pedido = pedidoCapturar();
  if (!a->pedido.aut[0] || !pedidoTravar(a->pedido)) { free(a); return 0; }
  for (int i = 0; i < nPlay; i++) if (!strcmp(play[i].chave, imdb)) { a->id = play[i].id; break; }
  pedidoDestravar();
  if (!a->id || pthread_create(&t, NULL, fioApagar, a) != 0) { free(a); return 0; }
  pthread_detach(t); return 1;
}

// ------------------------------------------------------------ scrobble (#459)
//
// O corpo e compartilhado com Trakt; a decisao e propria: Simkl permite
// pause entre 80 e 90% sem marcar visto. Um fio unico com uma vaga PENDENTE
// manda a chamada, entao pausar repetido nao empilha pedido.
// Rotas e limites: api.simkl.org/api-reference/simkl/scrobble-*. Pontos que
// mudam o desenho:
//   - 1 POST/s por usuario: o fio espera SMK_SCR_ESPACO_MS entre dois;
//   - o lock de 20 s por usuario volta como 400 RATE_LIMIT, nao 429;
//   - 409 num stop = ja registrado (so vale dentro de 1 h);
//   - stop >= 80% o Simkl marca como visto; < 80% guarda como pausado.
#ifndef SMK_SCR_ESPACO_MS
#define SMK_SCR_ESPACO_MS 1100   // um pouco acima do 1/s da doc
#endif
#ifndef SMK_SCR_ESPERA_MS
#define SMK_SCR_ESPERA_MS 30000  // primeira janela apos 429/5xx/RATE_LIMIT; dobra
#endif
#define SMK_SCR_ESPERA_MAX_MS 300000

static pthread_mutex_t travaScr = PTHREAD_MUTEX_INITIALIZER;
static ScrobbleEstado scrEstado;
typedef struct {
  int acao, tem;
  char id[64], jornal[64], token[400];
  double pct;
  unsigned long long cred, ger;
} ScrPedido;
static ScrPedido scrPend;
static int scrFioVivo;
// Janela de espera e espaco entre POSTs, sob travaScr. scrGer muda a cada zerar.
static unsigned long long scrLivreMs, scrUltimoMs, scrGer, scrRetentarMs;
static int scrFalhas;

static unsigned long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ULL + (unsigned long long)t.tv_nsec / 1000000ULL;
}

// "tt123:T:E" -> temporada e episodio; filme "tt123" -> 0, 0. 0 quando o id
// tem ':' mas nao e temporada >= 0 / episodio > 0 (iria como FILME do show).
static int idScrobble(const char *id, int *t, int *e) {
  const char *dp;
  *t = *e = 0;
  if (!imdbValido(id)) return 0;
  dp = strchr(id, ':');
  if (!dp) return 1;
  return sscanf(dp + 1, "%d:%d", t, e) == 2 && *t >= 0 && *e > 0;
}

// Apenas ids no disco, separados por vinculo (assinatura estavel do token).
// Nao persiste credenciais, e outra conta/perfil nao retoma este jornal.
static void scrobbleJornalNome(char *dst, size_t tam, const char *tk) {
  unsigned long long h = 14695981039346656037ULL;
  for (; *tk; tk++) h = (h ^ (unsigned char)*tk) * 1099511628211ULL;
  snprintf(dst, tam, "simkl-vistos-%016llx.txt", h);
}

// Sob travaScr. Preserva todos os stops sem resposta, inclusive de outros ids.
static int scrobbleJornal(const char *nome, const char *id, int adicionar) {
  char *b = dados_ler(nome), *p, *fim, *novo;
  size_t n = b ? strlen(b) : 0, u = 0;
  int ok;
  novo = malloc(n + strlen(id) + 2);
  if (!novo) { free(b); return 0; }
  for (p = b; p && *p; p = fim ? fim + 1 : NULL) {
    size_t k;
    fim = strchr(p, '\n');
    k = fim ? (size_t)(fim - p) : strlen(p);
    if (k == strlen(id) && !strncmp(p, id, k)) continue;
    memcpy(novo + u, p, k); u += k; novo[u++] = '\n';
  }
  if (adicionar) { strcpy(novo + u, id); u += strlen(id); novo[u++] = '\n'; }
  novo[u] = 0;
  ok = u ? dados_gravar(nome, novo) : dados_apagar(nome);
  free(novo); free(b);
  return ok;
}

// O stop que nao chegou ao Simkl cai no historico com a credencial capturada
// na saida. Depois de esperar o corpo compartilhado, perfil trocado deixa o id
// no jornal do perfil original.
static int scrobbleCairNoHistorico(const char *id, const char *tk,
                                   unsigned long long cred) {
  char corpo[SMK_LOTE_MAX * 20 + 200];
  int t, e, ok = 0;
  if (!idScrobble(id, &t, &e) || !tk[0]) return 0;
  pthread_mutex_lock(&travaCorpo);
  if (cred != simklauth_credencial_copiar(NULL, 0)) {
    pthread_mutex_unlock(&travaCorpo);
    return 0;
  }
  if (e > 0) {
    VistoPar par;
    char base[24];
    par.temporada = (short)t; par.episodio = (short)e;
    if (baseImdb(id, base, sizeof base) &&
        simkl_corpo_historico_eps(corpo, sizeof corpo, base, &par, 1))
      ok = postarHistoricoComToken(corpo, 1, base, 1, 1, tk);
  } else if (simkl_corpo_historico_titulo(corpo, sizeof corpo, id, "movie", NULL, 0, 1)) {
    ok = postarHistoricoComToken(corpo, 1, id, 0, 0, tk);
  }
  if (cred != simklauth_credencial_copiar(NULL, 0)) ok = 0;
  pthread_mutex_unlock(&travaCorpo);
  return ok;
}

// Zera a maquina de decisao e a janela de espera: logout e troca de perfil nao
// herdam o "start em pe", o pendente nem a espera do vinculo anterior. scrGer
// avisa o envio em voo que o resultado dele ja nao e deste estado.
static void scrobbleZerar(void) {
  pthread_mutex_lock(&travaScr);
  memset(&scrEstado, 0, sizeof scrEstado);
  memset(&scrPend, 0, sizeof scrPend);
  scrLivreMs = 0;
  scrFalhas = 0;
  scrRetentarMs = 0;
  scrGer++;
  pthread_mutex_unlock(&travaScr);
}

static void scrobbleEnviar(const ScrPedido *p) {
  int acao = p->acao;
  const char *id = p->id;
  double pct = p->pct;
  char caminho[32], corpo[400], tk[400], *r;
  int st = 0, ok, espera = 0;
  unsigned long long agora = agoraMs(), cred, ger, livre, ultimo;
  // A credencial sai copiada AGORA: o 401 e a janela de espera valem para ela,
  // nao para o perfil que estiver ligado quando a resposta chegar.
  cred = simklauth_credencial_copiar(tk, sizeof tk);
  pthread_mutex_lock(&travaScr);
  ger = scrGer; livre = scrLivreMs; ultimo = scrUltimoMs;
  pthread_mutex_unlock(&travaScr);
  if (ger != p->ger || cred != p->cred) { memset(tk, 0, sizeof tk); return; }
  if (!tk[0]) return;   // desvinculado no meio do voo: sem chamada nem janela
  if (agora < livre) {
    // Janela aberta por 429/5xx: nada sai, nem retry. O stop ainda tenta o
    // historico (o que o app fazia antes do scrobble existir).
    printf("[simkl] scrobble %s %s: em espera, descartado\n", scrobble_nome(acao), id);
    fflush(stdout);
    if (acao == SCR_STOP && scrobbleCairNoHistorico(id, p->token, p->cred)) {
      pthread_mutex_lock(&travaScr);
      scrobbleJornal(p->jornal, id, 0);
      pthread_mutex_unlock(&travaScr);
    }
    memset(tk, 0, sizeof tk);
    return;
  }
  if (ultimo && agora - ultimo < SMK_SCR_ESPACO_MS)
    usleep((useconds_t)(SMK_SCR_ESPACO_MS - (agora - ultimo)) * 1000u);
  // Logout/troca de perfil durante o intervalo cancela antes de qualquer POST.
  pthread_mutex_lock(&travaScr);
  ok = ger == scrGer;
  pthread_mutex_unlock(&travaScr);
  if (!ok || cred != simklauth_credencial_copiar(NULL, 0)) {
    memset(tk, 0, sizeof tk);
    return;
  }
  scrobble_corpo(corpo, sizeof corpo, id, pct);
  snprintf(caminho, sizeof caminho, "/scrobble/%s", scrobble_nome(acao));
  r = pedirComToken('P', caminho, NULL, corpo, &st, tk);
  memset(tk, 0, sizeof tk);
  ok = (st >= 200 && st < 300) || st == 409;   // 409: ja registrado
  // O log NUNCA leva o token: so a rota, o titulo e o status.
  printf("[simkl] scrobble %s %s %.1f%% -> %s (HTTP %d)\n", scrobble_nome(acao), id, pct,
         ok ? "ok" : "falhou", st);
  fflush(stdout);
  pthread_mutex_lock(&travaScr);
  scrUltimoMs = agoraMs();
  pthread_mutex_unlock(&travaScr);
  if (cred != simklauth_credencial_copiar(NULL, 0)) {
    // Perfil trocado ou vinculo refeito durante o POST: o resultado e do vinculo
    // anterior. Nem janela, nem desvincular, nem historico no perfil novo.
    free(r);
    return;
  }
  if (st == 401) {
    // Token recusado: o vinculo morreu. Nada mais sai ate vincular de novo.
    if (simklauth_invalidar_se(cred)) simkl_esquecer();
    free(r);
    return;
  }
  // Janela: 429, 5xx, st 0 (rede/timeout) e o 400 RATE_LIMIT do lock de 20 s.
  // Dobra a cada falha seguida ate 5 min; qualquer resposta boa zera.
  if (st == 429 || st >= 500 || st == 0 || (st == 400 && r && strstr(r, "RATE_LIMIT")))
    espera = 1;
  pthread_mutex_lock(&travaScr);
  if (ger == scrGer) {   // um esquecer no meio do voo zerou a janela: nao reabre
    if (espera) {
      unsigned long long w = SMK_SCR_ESPERA_MS;
      int k;
      for (k = 0; k < scrFalhas && w < SMK_SCR_ESPERA_MAX_MS; k++) w *= 2;
      if (w > SMK_SCR_ESPERA_MAX_MS) w = SMK_SCR_ESPERA_MAX_MS;
      scrFalhas++;
      scrLivreMs = agoraMs() + w;
    } else {
      scrFalhas = 0;
      scrLivreMs = 0;
    }
  }
  pthread_mutex_unlock(&travaScr);
  free(r);
  if (acao == SCR_STOP && (ok || scrobbleCairNoHistorico(id, p->token, p->cred))) {
    pthread_mutex_lock(&travaScr);
    scrobbleJornal(p->jornal, id, 0);
    pthread_mutex_unlock(&travaScr);
  }
}

static void *fioScrobble(void *u) {
  (void)u;
  for (;;) {
    ScrPedido pedido;
    pthread_mutex_lock(&travaScr);
    if (!scrPend.tem) { scrFioVivo = 0; pthread_mutex_unlock(&travaScr); return NULL; }
    pedido = scrPend;
    memset(&scrPend, 0, sizeof scrPend);
    pthread_mutex_unlock(&travaScr);
    scrobbleEnviar(&pedido);
    memset(&pedido, 0, sizeof pedido);
  }
}

// Na SAIDA a sessao acabou mesmo sem chamada: com o start "em pe" a proxima
// abertura do mesmo titulo nao mandaria /scrobble/start.
static int scrobbleRecusar(int evento) {
  if (evento == SCR_EV_SAIU) {
    pthread_mutex_lock(&travaScr); scrEstado.ativo = 0; pthread_mutex_unlock(&travaScr);
  }
  return SCR_NADA;
}

static int scrobbleDecidir(int evento, const char *id, double pct) {
  int mesmo = !strcmp(scrEstado.id, id);
  if (evento == SCR_EV_TOCANDO) {
    if (scrEstado.ativo && mesmo) return SCR_NADA;
    snprintf(scrEstado.id, sizeof scrEstado.id, "%s", id);
    scrEstado.ativo = 1;
    return SCR_START;
  }
  if (evento == SCR_EV_PONTO)
    return scrEstado.ativo && mesmo && pct >= 1.0 && pct < 80.0 ? SCR_START : SCR_NADA;
  if (evento == SCR_EV_PAUSOU) {
    if (!scrEstado.ativo || !mesmo) return SCR_NADA;
    scrEstado.ativo = 0;
    return pct >= 1.0 ? SCR_PAUSE : SCR_NADA;
  }
  if (evento == SCR_EV_SAIU) {
    snprintf(scrEstado.id, sizeof scrEstado.id, "%s", id);
    scrEstado.ativo = 0;
    if (pct >= 90.0) return SCR_STOP;
    return pct >= 1.0 ? SCR_PAUSE : SCR_NADA;
  }
  return SCR_NADA;
}

int simkl_scrobble(int evento, const char *id, double posSeg, double durSeg, double assistidoSeg,
                   int assistiu) {
  int acao, t, e;
  char tk[400];
  ScrPedido pedido = {0};
  double pct;
  pthread_t th;
  if (!simkl_ativo() || !nuvem_simkl_cliente()[0] || !id || !idScrobble(id, &t, &e) ||
      !isfinite(posSeg) || !isfinite(durSeg) || !isfinite(assistidoSeg) ||
      durSeg < 120.0 || assistidoSeg < 5.0)
    return scrobbleRecusar(evento);
  pct = 100.0 * posSeg / durSeg;
  // Valida ANTES da vaga: nem um stop sem reproducao entra na fila.
  if (evento == SCR_EV_SAIU && pct >= 90.0 && !assistiu) return scrobbleRecusar(evento);
  pthread_mutex_lock(&travaScr);
  // Um stop pendente nunca e trocado: perder o stop e perder o visto. Quem
  // chama recebe NADA e segue pelo historico.
  if (scrPend.tem && scrPend.acao == SCR_STOP) {
    pthread_mutex_unlock(&travaScr);
    return SCR_NADA;
  }
  acao = scrobbleDecidir(evento, id, pct);
  if (acao != SCR_NADA) {
    pedido.cred = simklauth_credencial_copiar(tk, sizeof tk);
    pedido.ger = scrGer;
    scrobbleJornalNome(pedido.jornal, sizeof pedido.jornal, tk);
    if (acao == SCR_STOP) snprintf(pedido.token, sizeof pedido.token, "%s", tk);
    memset(tk, 0, sizeof tk);
    // Antes do fio: a saida pode matar o POST de 20 s depois da drenagem de 3 s.
    if (acao == SCR_STOP && !scrobbleJornal(pedido.jornal, id, 1)) {
      pthread_mutex_unlock(&travaScr);
      return SCR_NADA;
    }
    pedido.acao = acao; pedido.pct = pct; pedido.tem = 1;
    snprintf(pedido.id, sizeof pedido.id, "%s", id);
    scrPend = pedido;
    if (!scrFioVivo) {
      scrFioVivo = 1;
      if (pthread_create(&th, NULL, fioScrobble, NULL) != 0) {
        scrFioVivo = 0;
        memset(&scrPend, 0, sizeof scrPend);
        acao = SCR_NADA;
      } else pthread_detach(th);
    }
  }
  pthread_mutex_unlock(&travaScr);
  return acao;
}

void simkl_scrobble_retentar(void) {
  char tk[400], nome[64], id[64], *b;
  unsigned long long cred = simklauth_credencial_copiar(tk, sizeof tk);
  unsigned long long agora = agoraMs();
  if (!tk[0]) return;
  scrobbleJornalNome(nome, sizeof nome, tk);
  memset(tk, 0, sizeof tk);
  pthread_mutex_lock(&travaScr);
  if (scrFioVivo || agora < scrRetentarMs) { pthread_mutex_unlock(&travaScr); return; }
  scrRetentarMs = agora + 30000;
  b = dados_ler(nome);
  id[0] = 0;
  if (b) sscanf(b, "%63s", id);
  free(b);
  pthread_mutex_unlock(&travaScr);
  if (id[0] && cred == simklauth_credencial_copiar(NULL, 0))
    simkl_scrobble(SCR_EV_SAIU, id, 1000, 1000, 1000, 1);
}

void simkl_scrobble_drenar(unsigned ms) {
  unsigned i;
  for (i = 0; i < ms / 20 && !simkl_scrobble_ocioso(); i++) usleep(20 * 1000);
}

int simkl_scrobble_ocioso(void) {
  int o;
  pthread_mutex_lock(&travaScr);
  o = !scrFioVivo && !scrPend.tem;
  pthread_mutex_unlock(&travaScr);
  return o;
}
