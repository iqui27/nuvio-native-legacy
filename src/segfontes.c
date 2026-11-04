#include "segfontes.h"
#include "rede.h"
#include "js.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// nuvem.c embute o client_id Simkl do build. Fraca para os testes, que nao
// linkam nuvem.c: sem ela, o Simkl fica desligado e nada vai a rede.
__attribute__((weak)) const char *nuvem_simkl_cliente(void) { return ""; }
__attribute__((weak)) const char *nuvem_simkl_app(void) { return "nuvio"; }

#define SIMKL_BASE "https://api.simkl.com"
#define ANIMESKIP_URL "https://api.anime-skip.com/graphql"

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// ---------------------------------------------------------------- rede
static int getPadrao(const char *url, char **corpo, int *status) {
  RedePedido q; RedeResposta r; int ok;
  memset(&q, 0, sizeof q); memset(&r, 0, sizeof r);
  q.url = url; q.seguir = 1; q.prazo_ms = 10000; q.max_bytes = 2u * 1024u * 1024u;
  ok = rede_pedir(&q, &r);
  *status = ok ? r.status : 0; *corpo = NULL;
  if (ok && r.status >= 200 && r.status <= 299 && r.corpo) { *corpo = r.corpo; r.corpo = NULL; }
  rede_resposta_limpar(&r);
  return *status;
}
static int postPadrao(const char *url, const char *corpo, const char *clientId, char **resp, int *status) {
  RedePedido q; RedeResposta r; int ok;
  char cabId[200];
  const char *cab[3];
  snprintf(cabId, sizeof cabId, "X-Client-ID: %s", clientId);
  cab[0] = cabId; cab[1] = "Content-Type: application/json"; cab[2] = NULL;
  memset(&q, 0, sizeof q); memset(&r, 0, sizeof r);
  q.metodo = "POST"; q.url = url; q.cabecalhos = cab;
  q.corpo = corpo; q.n_corpo = strlen(corpo);
  q.seguir = 1; q.prazo_ms = 10000; q.max_bytes = 2u * 1024u * 1024u;
  ok = rede_pedir(&q, &r);
  *status = ok ? r.status : 0; *resp = NULL;
  if (ok && r.status >= 200 && r.status <= 299 && r.corpo) { *resp = r.corpo; r.corpo = NULL; }
  rede_resposta_limpar(&r);
  return *status;
}
static int (*buscar)(const char *, char **, int *) = getPadrao;
static int (*postar)(const char *, const char *, const char *, char **, int *) = postPadrao;
void segfontes_definir_buscador(int (*get)(const char *, char **, int *)) { buscar = get ? get : getPadrao; }
void segfontes_definir_postador(int (*post)(const char *, const char *, const char *, char **, int *)) {
  postar = post ? post : postPadrao;
}

// ---------------------------------------------------------------- JSON
// Numero OU texto numerico ("mal":"21" e "mal":21 aparecem os dois no Simkl).
static long idDe(const char *ini, const char *fim, const char *chave) {
  char t[32] = "";
  double v = js_num(ini, fim, chave, -1);
  if (v > 0) return (long)v;
  if (js_texto(ini, fim, chave, t, sizeof t) && t[0]) return atol(t);
  return 0;
}
// Numero de `chave` em [ini,fim) FORA do objeto [exIni,exFim): o "episode" do
// item, e nao o "episode" dentro de "tvdb":{...}.
static double numFora(const char *ini, const char *fim, const char *chave,
                      const char *exIni, const char *exFim, double padrao) {
  double v = exIni ? js_num(ini, exIni, chave, padrao) : js_num(ini, fim, chave, padrao);
  if (v == padrao && exFim && exFim < fim) v = js_num(exFim, fim, chave, padrao);
  return v;
}
// Inicio e fim do objeto `chave` dentro de [ini,fim).
static const char *objeto(const char *ini, const char *fim, const char *chave, const char **objFim) {
  char padrao[48];
  const char *p;
  snprintf(padrao, sizeof padrao, "\"%s\"", chave);
  for (p = ini; p && (!fim || p < fim) && (p = strstr(p, padrao)) != NULL && (!fim || p < fim); p++) {
    const char *q = p + strlen(padrao);
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    if (*q != ':') continue;
    q++;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    if (*q != '{') return NULL;
    *objFim = js_fim(q);
    return q;
  }
  return NULL;
}

long segfontes_simkl_busca(const char *json, char *tipo, size_t ntipo) {
  const char *p = json ? js_raiz_array(json) : NULL;
  if (tipo && ntipo) tipo[0] = 0;
  for (; p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *idsFim = NULL, *ids = objeto(p, f, "ids", &idsFim);
    long id = ids ? idDe(ids, idsFim, "simkl") : 0;
    char t[24] = "";
    if (id <= 0) continue;
    js_texto(p, f, "type", t, sizeof t);
    if (tipo && ntipo)
      snprintf(tipo, ntipo, "%s", !strcmp(t, "movie") ? "movies" : !strcmp(t, "show") ? "tv" : "anime");
    return id;   // o primeiro, como o PC (results[0])
  }
  return 0;
}

int segfontes_simkl_detalhes(const char *json, long *mal, long *anilist, int *temporada) {
  const char *fim, *idsFim = NULL, *ids;
  if (!json || *json != '{') return 0;
  fim = js_fim(json);
  ids = objeto(json, fim, "ids", &idsFim);
  if (mal) *mal = ids ? idDe(ids, idsFim, "mal") : 0;
  if (anilist) *anilist = ids ? idDe(ids, idsFim, "anilist") : 0;
  if (temporada) {
    double s = numFora(json, fim, "season", ids, idsFim, -1);
    *temporada = s > 0 ? (int)s : 0;
  }
  return ids != NULL;
}

long segfontes_simkl_temporada(const char *json, int tvdbT) {
  const char *p = json ? js_array(json, NULL, "mapped_tvdb_seasons") : NULL;
  for (; p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    long id = (long)js_num(p, f, "simkl_id", 0);
    int t = (int)js_num(p, f, "tvdb_season", 0);
    if (id > 0 && t == tvdbT) return id;
  }
  return 0;
}

int segfontes_simkl_ep(const char *json, int tvdbT, int tvdbE) {
  const char *p = json ? js_raiz_array(json) : NULL;
  for (; p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *tvFim = NULL, *tv = objeto(p, f, "tvdb", &tvFim);
    int ep;
    if (!tv) continue;
    ep = (int)numFora(p, f, "episode", tv, tvFim, 0);
    if (ep > 0 && (int)js_num(tv, tvFim, "season", 0) == tvdbT &&
        (int)js_num(tv, tvFim, "episode", 0) == tvdbE) return ep;
  }
  return 0;
}

// ---------------------------------------------------------------- Simkl
typedef struct { char chave[40]; SegIds ids; int ok; } SimklCache;
static SimklCache cache[16];
static int cacheProx;

static int simklGet(const char *caminho, char **corpo) {
  char url[512];
  int st = 0;
  const char *cid = nuvem_simkl_cliente();
  snprintf(url, sizeof url, "%s%s%cclient_id=%s&app-name=%s&app-version=1.0", SIMKL_BASE, caminho,
           strchr(caminho, '?') ? '&' : '?', cid, nuvem_simkl_app());
  buscar(url, corpo, &st);
  if (st < 200 || st > 299 || !*corpo) { free(*corpo); *corpo = NULL; }
  return st;
}
// Sem resposta de verdade (rede, timeout, 5xx, 429): nao e "nao conhece". O
// cache so guarda respostas reais, senao um tropeco no primeiro episodio
// desligava o Simkl daquela temporada ate o app reabrir.
static int simklFalhou(int st) { return st < 200 || (st > 299 && st != 404 && st != 204); }

// Entrada Simkl -> ids (detalhes). `tipo`: "anime"/"tv"/"movies".
static int simklDetalhes(long simkl, const char *tipo, SegIds *o, int *falha) {
  char cam[96], *j = NULL;
  int temporada = 0, ok;
  snprintf(cam, sizeof cam, "/%s/%ld?extended=full", tipo, simkl);
  if (simklFalhou(simklGet(cam, &j))) *falha = 1;
  ok = j && segfontes_simkl_detalhes(j, &o->mal, &o->anilist, &temporada);
  free(j);
  o->simkl = simkl;
  return ok ? (temporada > 0 ? temporada : -1) : 0;
}

// Cache em DOIS niveis: a entrada (ids da temporada) por fonte:id:temporada — a
// mesma para todos os episodios dela, sem pedido nenhum do segundo em diante —
// e o mapa de episodios (so anime) por entrada Simkl E temporada TVDB, so com
// os pares dessa temporada. Uma entrada longa (One Piece, Conan: 1000+
// episodios numa so) nao cabe inteira; a temporada cabe, e um episodio tardio
// nao cai no numero do TVDB (que pediria os tempos de outro episodio).
typedef struct { int e, ep; } SimklEp;
typedef struct { long simkl; int t, n; SimklEp v[400]; } SimklEps;
static SimklEps epsCache[4];
static int epsProx;

static int simklEpCache(long simkl, int t, int e, int *ep) {
  int i, k, achou = -1;
  pthread_mutex_lock(&trava);
  for (i = 0; i < 4; i++)
    if (epsCache[i].simkl == simkl && epsCache[i].t == t) {
      achou = 0;
      for (k = 0; k < epsCache[i].n; k++)
        if (epsCache[i].v[k].e == e) { *ep = epsCache[i].v[k].ep; achou = 1; break; }
      break;
    }
  pthread_mutex_unlock(&trava);
  return achou;   // -1 = mapa nao baixado, 0 = baixado sem o episodio, 1 = achou
}
static void simklEpGuardar(long simkl, int t, const char *json) {
  SimklEps *c;
  const char *p = json ? js_raiz_array(json) : NULL;
  pthread_mutex_lock(&trava);
  c = &epsCache[epsProx]; epsProx = (epsProx + 1) % 4;
  c->simkl = simkl; c->t = t; c->n = 0;
  for (; p && c->n < 400; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *tvFim = NULL, *tv = objeto(p, f, "tvdb", &tvFim);
    int ep;
    if (!tv || (int)js_num(tv, tvFim, "season", 0) != t) continue;
    ep = (int)numFora(p, f, "episode", tv, tvFim, 0);
    if (ep <= 0) continue;
    c->v[c->n].e = (int)js_num(tv, tvFim, "episode", 0);
    c->v[c->n].ep = ep;
    if (c->v[c->n].e > 0) c->n++;
  }
  pthread_mutex_unlock(&trava);
}

int segfontes_simkl(const char *fonte, const char *id, int t, int e, SegIds *out) {
  char chave[40], cam[160], tipo[16] = "", *j = NULL;
  SegIds o;
  long simkl;
  int i, temporada, ok = 0, ep, falha = 0;
  if (!out) return 0;
  memset(out, 0, sizeof *out);
  if (!fonte || !id || !id[0] || !nuvem_simkl_cliente()[0]) return 0;
  snprintf(chave, sizeof chave, "%s:%s:%d", fonte, id, t);
  pthread_mutex_lock(&trava);
  for (i = 0; i < 16; i++)
    if (!strcmp(cache[i].chave, chave)) { o = cache[i].ids; ok = cache[i].ok; break; }
  pthread_mutex_unlock(&trava);

  if (i == 16) {
    memset(&o, 0, sizeof o);
    snprintf(cam, sizeof cam, "/search/id?%s=%s", fonte, id);
    if (simklFalhou(simklGet(cam, &j))) falha = 1;
    simkl = j ? segfontes_simkl_busca(j, tipo, sizeof tipo) : 0;
    free(j); j = NULL;
    if (simkl > 0) {
      o.anime = !strcmp(tipo, "anime");
      temporada = simklDetalhes(simkl, tipo, &o, &falha);
      ok = temporada != 0;
      // IMDb de anime com varias temporadas: a temporada TVDB pedida e OUTRA
      // entrada no Simkl (e outro id no MAL). Mesmo caminho do PC.
      if (ok && o.anime && !strcmp(fonte, "imdb") && t > 0 && temporada != t) {
        long irmao;
        snprintf(cam, sizeof cam, "/%s/%ld?extended=full_anime_seasons", tipo, simkl);
        if (simklFalhou(simklGet(cam, &j))) falha = 1;
        irmao = j ? segfontes_simkl_temporada(j, t) : 0;
        free(j); j = NULL;
        if (irmao > 0 && irmao != simkl) {
          SegIds s = o;
          if (simklDetalhes(irmao, tipo, &s, &falha)) o = s;
        }
      }
    }
    if (!falha) {
      pthread_mutex_lock(&trava);
      snprintf(cache[cacheProx].chave, sizeof cache[cacheProx].chave, "%s", chave);
      cache[cacheProx].ids = o; cache[cacheProx].ok = ok;
      cacheProx = (cacheProx + 1) % 16;
      pthread_mutex_unlock(&trava);
    }
  }
  // TVDB T/E -> episodio dentro da entrada do anime (um mapa por entrada).
  o.ep = e;
  if (ok && o.anime && o.simkl > 0 && !strcmp(fonte, "imdb") && t > 0 && e > 0) {
    int r = simklEpCache(o.simkl, t, e, &ep);
    if (r < 0) {
      snprintf(cam, sizeof cam, "/anime/episodes/%ld", o.simkl);
      simklGet(cam, &j);
      if (j) simklEpGuardar(o.simkl, t, j);
      free(j); j = NULL;
      r = simklEpCache(o.simkl, t, e, &ep);
    }
    if (r > 0) o.ep = ep;
  }
  printf("[seg] Simkl %s=%s T%dE%d: simkl=%ld anime=%d mal=%ld anilist=%ld ep=%d\n",
         fonte, id, t, e, o.simkl, o.anime, o.mal, o.anilist, o.ep);
  fflush(stdout);
  *out = o;
  return ok;
}

// ---------------------------------------------------------------- Anime-Skip
int segfontes_animeskip_shows(const char *json, char ids[][48], int max) {
  const char *p = json ? js_array(json, NULL, "findShowsByExternalId") : NULL;
  int n = 0;
  for (; p && n < max; p = js_prox(js_fim(p)))
    if (js_texto(p, js_fim(p), "id", ids[n], sizeof ids[n]) && ids[n][0]) n++;
  return n;
}

static int tipoAnimeSkip(const char *nome) {
  char m[32];
  size_t i;
  snprintf(m, sizeof m, "%s", nome);
  for (i = 0; m[i]; i++) if (m[i] >= 'A' && m[i] <= 'Z') m[i] = (char)(m[i] - 'A' + 'a');
  if (!strcmp(m, "intro") || !strcmp(m, "new intro") || !strcmp(m, "mixed intro")) return INTRO_ABERTURA;
  if (!strcmp(m, "credits") || !strcmp(m, "new credits") || !strcmp(m, "mixed credits")) return INTRO_CREDITOS;
  if (!strcmp(m, "recap")) return INTRO_RESUMO;
  // A previa (PV/yokoku) e marcada pelo Anime-Skip: nao e cena pos-creditos.
  if (!strcmp(m, "preview")) return INTRO_PREVIA;
  return 0;
}

int segfontes_animeskip_ep(const char *json, int ep, IntroTrecho *out, int max) {
  const char *p = json ? js_array(json, NULL, "findEpisodesByShowId") : NULL;
  for (; p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *ts;
    char num[16] = "";
    double at[64];
    int tp[64], canon[64], marca[64], nt = 0, i, j, n = 0, cred = -1;
    // "number" vem como TEXTO ("12"); o "at" dos timestamps e numero.
    if (!js_texto(p, f, "number", num, sizeof num) || atoi(num) != ep) continue;
    for (ts = js_array(p, f, "timestamps"); ts && nt < 64; ts = js_prox(js_fim(ts))) {
      const char *tf = js_fim(ts), *tyFim = NULL, *ty = objeto(ts, tf, "type", &tyFim);
      char nome[32] = "";
      double a = js_num(ts, tf, "at", -1);
      if (a < 0) continue;
      if (ty) js_texto(ty, tyFim, "name", nome, sizeof nome);
      at[nt] = a; tp[nt] = tipoAnimeSkip(nome);
      { char l[16]; size_t q; snprintf(l, sizeof l, "%s", nome);
        for (q = 0; l[q]; q++) if (l[q] >= 'A' && l[q] <= 'Z') l[q] = (char)(l[q] - 'A' + 'a');
        canon[nt] = !strcmp(l, "canon");
        // Logo/cartao/transicao: nao e historia nem cena.
        marca[nt] = !strcmp(l, "branding") || !strcmp(l, "title card") || !strcmp(l, "transition"); }
      nt++;
    }
    // Ordena por `at` (insercao: sao poucos).
    for (i = 1; i < nt; i++)
      for (j = i; j > 0 && at[j] < at[j - 1]; j--) {
        double ta = at[j]; int tt = tp[j], tc = canon[j], tm = marca[j];
        at[j] = at[j - 1]; tp[j] = tp[j - 1]; canon[j] = canon[j - 1]; marca[j] = marca[j - 1];
        at[j - 1] = ta; tp[j - 1] = tt; canon[j - 1] = tc; marca[j - 1] = tm;
      }
    // Fim = proximo timestamp; o ultimo vai "ate o fim" (0), como o PC.
    for (i = 0; i < nt && n < max; i++) {
      int k, ja = 0;
      if (!tp[i]) continue;
      for (k = 0; k < n; k++) if (out[k].tipo == tp[i]) ja = 1;   // primeiro por tipo
      if (ja) continue;
      { int k = i + 1;
        // Os creditos vao ate o proximo marcador que NAO seja logo/cartao:
        // "Branding" logo depois do ED e parte dele, nao cena pos-creditos.
        if (tp[i] == INTRO_CREDITOS) while (k < nt && marca[k]) k++;
        out[n++] = (IntroTrecho){ at[i], k < nt ? at[k] : 0.0, tp[i], 0 }; }
      if (tp[i] == INTRO_CREDITOS && cred < 0) cred = i;
    }
    // Historia ("Canon") LOGO depois dos creditos: cena pos-creditos de
    // verdade (o "C-part"), marcada e nunca pulada.
    if (cred >= 0) {
      int c = cred + 1;
      while (c < nt && marca[c]) c++;   // pula o logo entre os creditos e a cena
      if (c < nt && canon[c] && n < max)
        out[n++] = (IntroTrecho){ at[c], c + 1 < nt ? at[c + 1] : 0.0, INTRO_POS_CREDITOS, 0 };
    }
    return n;
  }
  return 0;
}

static struct { long anilist; char show[48]; int nada; } showCache[16];
static int showProx;

int segfontes_animeskip(long anilist, int ep, const char *clientId,
                        IntroTrecho *out, int max, int *falha) {
  char corpo[400], shows[4][48], *j = NULL;
  int st = 0, ns = 0, i, n = 0;
  if (falha) *falha = 0;
  if (!clientId || !clientId[0] || anilist <= 0 || ep <= 0 || !out || max <= 0) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < 16; i++)
    if (showCache[i].anilist == anilist) {
      if (showCache[i].nada) { pthread_mutex_unlock(&trava); return 0; }
      snprintf(shows[0], sizeof shows[0], "%s", showCache[i].show); ns = 1; break;
    }
  pthread_mutex_unlock(&trava);
  if (!ns) {
    snprintf(corpo, sizeof corpo,
             "{\"query\":\"{ findShowsByExternalId(service: ANILIST, serviceId: \\\"%ld\\\") { id } }\"}", anilist);
    postar(ANIMESKIP_URL, corpo, clientId, &j, &st);
    if (st < 200 || st > 299 || !j) {
      free(j);
      if (falha && st != 404) *falha = 1;
      printf("[seg] Anime-Skip anilist %ld: HTTP %d\n", anilist, st); fflush(stdout);
      return 0;
    }
    ns = segfontes_animeskip_shows(j, shows, 4);
    free(j); j = NULL;
    pthread_mutex_lock(&trava);
    if (ns <= 1) {   // so guarda resposta inequivoca, como o PC
      showCache[showProx].anilist = anilist; showCache[showProx].nada = ns == 0;
      snprintf(showCache[showProx].show, sizeof showCache[showProx].show, "%s", ns ? shows[0] : "");
      showProx = (showProx + 1) % 16;
    }
    pthread_mutex_unlock(&trava);
  }
  for (i = 0; i < ns && !n; i++) {
    snprintf(corpo, sizeof corpo,
             "{\"query\":\"{ findEpisodesByShowId(showId: \\\"%s\\\") { season number timestamps { at type { name } } } }\"}",
             shows[i]);
    postar(ANIMESKIP_URL, corpo, clientId, &j, &st);
    if (st >= 200 && st <= 299 && j) n = segfontes_animeskip_ep(j, ep, out, max);
    else if (falha && st != 404) *falha = 1;
    free(j); j = NULL;
  }
  printf("[seg] Anime-Skip anilist %ld ep %d: %d shows, %d trechos\n", anilist, ep, ns, n);
  fflush(stdout);
  return n;
}

// ---------------------------------------------------------------- Client ID
static char idConta[96], idLocal[96];
// O ID vai no cabecalho X-Client-ID: so letras, digitos, '-' e '_' (o mesmo
// filtro do campo de ajustes). O da conta vem do servidor sem filtro nenhum —
// um CR/LF ali viraria outro cabecalho no pedido.
static void idLimpo(char *dst, size_t n, const char *t) {
  size_t i, k = 0;
  for (i = 0; t && t[i] && k + 1 < n; i++)
    if ((t[i] >= '0' && t[i] <= '9') || (t[i] >= 'a' && t[i] <= 'z') ||
        (t[i] >= 'A' && t[i] <= 'Z') || t[i] == '-' || t[i] == '_') dst[k++] = t[i];
  dst[k] = 0;
}
void segfontes_animeskip_id_conta(const char *id) {
  pthread_mutex_lock(&trava); idLimpo(idConta, sizeof idConta, id); pthread_mutex_unlock(&trava);
}
void segfontes_animeskip_id_local(const char *id) {
  pthread_mutex_lock(&trava); idLimpo(idLocal, sizeof idLocal, id); pthread_mutex_unlock(&trava);
}
const char *segfontes_animeskip_id(char *buf, size_t n) {
  if (!buf || !n) return "";
  pthread_mutex_lock(&trava);
  snprintf(buf, n, "%s", idLocal[0] ? idLocal : idConta);
  pthread_mutex_unlock(&trava);
  return buf;
}

void segfontes_limpar_cache(void) {
  pthread_mutex_lock(&trava);
  memset(cache, 0, sizeof cache); cacheProx = 0;
  memset(epsCache, 0, sizeof epsCache); epsProx = 0;
  memset(showCache, 0, sizeof showCache); showProx = 0;
  pthread_mutex_unlock(&trava);
}
