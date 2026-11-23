// Simkl como fonte da retomada e destino do "+". Ver simkl.h: la estao as
// rotas (todas conferidas na documentacao oficial), o que ficou de fora e o
// modelo de fios.
#include "simkl.h"
#include "perfisessao.h"
#include "ajustes.h"
#include "simklauth.h"
#include "simklbib.h"
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

// Anime CW may ship `show` instead of nested `anime`, or ids at activity root (#714).
static int idNumOk(double v, long *out) {
  if (v <= 0.0 || v >= 2147483647.0) return 0;
  *out = (long)v;
  if (*out <= 0) return 0;
  return 1;
}

static int lerIdsSimkl(const char *oi, const char *of, char *imdb, size_t nImdb,
                       long *tmdb) {
  const char *ii, *ifim;
  long id;
  double v;
  if (tmdb) *tmdb = 0;
  imdb[0] = 0;
  if (!objetoRaiz(oi, of, "ids", &ii, &ifim)) return 0;
  js_texto(ii, ifim, "imdb", imdb, nImdb);
  if (imdb[0] == 't' && imdb[1] == 't') return 1;
  imdb[0] = 0;
  v = js_num(ii, ifim, "tmdb", 0);
  if (idNumOk(v, &id)) {
    snprintf(imdb, nImdb, "tmdb:%ld", id);
    if (tmdb) *tmdb = id;
    return 1;
  }
  v = js_num(ii, ifim, "simkl", 0);
  if (idNumOk(v, &id)) { snprintf(imdb, nImdb, "simkl:%ld", id); return 1; }
  return 0;
}

static int obraDeAssistindo(const char *p, const char *f, int anime,
                            const char *chave, const char **oi, const char **of) {
  char imdb[24];
  CatItem d;
  if (objetoRaiz(p, f, chave, oi, of)) {
    if (!anime) return 1;
    imdb[0] = 0;
    if (lerObra(*oi, *of, &d, imdb, sizeof imdb)) return 1;
    if (lerIdsSimkl(*oi, *of, imdb, sizeof imdb, NULL)) return 1;
  } else if (!anime) return 0;
  if (anime && objetoRaiz(p, f, "show", oi, of)) {
    imdb[0] = 0;
    if (lerObra(*oi, *of, &d, imdb, sizeof imdb)) return 1;
    if (lerIdsSimkl(*oi, *of, imdb, sizeof imdb, NULL)) return 1;
    return 0;
  }
  if (anime && lerIdsSimkl(p, f, imdb, sizeof imdb, NULL)) {
    *oi = p;
    *of = f;
    return 1;
  }
  return 0;
}

// ------------------------------------------------------------ leitores puros

int simkl_ler_playback(const char *json, CatItem *dst, long long *ids, int max) {
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
    n++;
  }
  return n;
}

// Reuse the provider metadata cache; an unknown mapping opens the title only.
static int animeAbsoluto(const char *id, int absoluto, int *t, int *e) {
  char chave[64];
  MetaEpInfo ep;
  int ok;
  snprintf(chave, sizeof chave, "series/%s/%s", id, metaprov_idioma());
  char *corpo = fichameta_ler(chave, 6 * 3600);
  ok = metaep_buscar(corpo, 1, 1, &ep);
  // ponytail: repeated scans reuse provider ordering; index if long shows cost time.
  for (int i = 1; ok && i < absoluto; i++) {
    MetaEpInfo prox;
    ok = metaep_proximo(corpo, ep.temporada, ep.episodio, &prox) &&
         ((prox.temporada == ep.temporada && prox.episodio == ep.episodio + 1) ||
          (prox.temporada == ep.temporada + 1 && prox.episodio == 1));
    if (ok) ep = prox;
  }
  free(corpo);
  *t = ok ? ep.temporada : 0;
  *e = ok ? ep.episodio : 0;
  return ok;
}

int simkl_ler_assistindo(const char *json, CatItem *dst, int max) {
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
    if (!obraDeAssistindo(p, f, k == 1, chaves[k], &oi, &of)) goto watchingDropped;
    if (!lerObra(oi, of, d, imdb, sizeof imdb)) {
      reason = "missing supported id";
      if (k != 1) goto watchingDropped;
      if (!lerIdsSimkl(oi, of, imdb, sizeof imdb, &d->tmdb) &&
          (oi != p || !lerIdsSimkl(p, f, imdb, sizeof imdb, &d->tmdb)))
        goto watchingDropped;
      if (oi == p) js_texto_raiz_em(p, f, "title", d->titulo, sizeof d->titulo);
    } else {
      const char *ii, *ifim;
      long tid;
      if (objetoRaiz(oi, of, "ids", &ii, &ifim) &&
          idNumOk(js_num(ii, ifim, "tmdb", 0), &tid))
        d->tmdb = tid;
    }
    if (absoluto) animeAbsoluto(imdb, absoluto, &t, &e);
    d->temporada = t;
    d->episodio = e;
    d->progresso = 0;
    if (e > 0) snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", imdb, t, e);
    else snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
    snprintf(d->tipo, sizeof d->tipo, "series");
    // O instante do "a seguir" e o do ULTIMO EPISODIO VISTO, como no Trakt
    // (ult[].quandoMs): e ele que diz quando a pessoa mexeu nesta serie.
    if (js_texto_raiz_em(p, f, "last_watched_at", quando, sizeof quando))
      d->retomadoMs = js_ms_iso(quando);
    n++;
    if (k == 1) {
      animeKept++;
      printf("[simkl] anime watching #%d id=%s next=%s: candidate (%s)\n",
             animeEntries, imdb, prox, e > 0 ? "episode mapped" : "title fallback");
    }
    continue;
watchingDropped:
    if (k == 1)
      printf("[simkl] anime watching #%d: dropped (%s)\n", animeEntries, reason);
  }
  printf("[simkl] anime watching: %d entries, %d candidates, %d dropped\n",
         animeEntries, animeKept, animeEntries - animeKept);
  return n;
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
    if (!objetoRaiz(p, f, chaves[k], &oi, &of) &&
        (k != 2 || !objetoRaiz(p, f, "show", &oi, &of))) continue;
    if (!lerObra(oi, of, d, imdb, sizeof imdb)) continue;
    snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
    snprintf(d->tipo, sizeof d->tipo, "%s", serie ? "series" : "movie");
    d->naLista = 1;
    d->fontes = CAT_FONTE_SIMKL;
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
  d->diaHoje = mes.tm_mday;
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

static void trocar(char **dst, char *novo) { free(*dst); *dst = novo; }

static int pedidoPerfil(SmkPedido p);
static char *bibAtualizada(SmkPedido pedido, int perfil, char **ativ);

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

// ------------------------------------------------------------ vistos (issue #643)

static long long vistoEpoch(const char *iso) {
  struct tm t = {0};
  int a, m, d, h = 0, mi = 0, s = 0;
  if (!iso || sscanf(iso, "%d-%d-%dT%d:%d:%d", &a, &m, &d, &h, &mi, &s) < 3) return 0;
  t.tm_year = a - 1900; t.tm_mon = m - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
  return (long long)timegm(&t);
}

// Insere em ordem decrescente; mesmo imdb+T:E fica com o mais novo.
static void vistoAdd(SimklVisto *dst, int *n, int max, const SimklVisto *v) {
  int i, k;
  for (i = 0; i < *n; i++)
    if (!strcmp(dst[i].imdb, v->imdb) && dst[i].temporada == v->temporada &&
        dst[i].episodio == v->episodio) {
      if (dst[i].quando >= v->quando) return;
      memmove(dst + i, dst + i + 1, sizeof *dst * (size_t)(*n - i - 1)); (*n)--;
      break;
    }
  for (k = 0; k < *n && dst[k].quando >= v->quando; k++) ;
  if (k >= max) return;
  if (*n < max) (*n)++;
  memmove(dst + k + 1, dst + k, sizeof *dst * (size_t)(*n - k - 1));
  dst[k] = *v;
}

static void vistosTipo(const char *json, const char *lista, int filme,
                       SimklVisto *dst, int *n, int max) {
  const char *p;
  int semData = 0;
  for (p = js_array(json, NULL, lista); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *obra, *obraFim, *ids, *idsFim;
    SimklVisto v = {0};
    char quando[48] = "";
    // Simkl's anime lists put each item under "show"; "anime" is kept too.
    if (!objetoRaiz(p, f, filme ? "movie" : "show", &obra, &obraFim) &&
        (filme || strcmp(lista, "anime") || !objetoRaiz(p, f, "anime", &obra, &obraFim)))
      continue;
    js_texto_raiz_em(obra, obraFim, "title", v.titulo, sizeof v.titulo);
    if (objetoRaiz(obra, obraFim, "ids", &ids, &idsFim)) js_texto(ids, idsFim, "imdb", v.imdb, sizeof v.imdb);
    if (!imdbValido(v.imdb)) continue;
    snprintf(v.poster, sizeof v.poster, "https://images.metahub.space/poster/medium/%s/img", v.imdb);
    if (filme) {
      snprintf(v.tipo, sizeof v.tipo, "movie");
      js_texto_raiz_em(p, f, "last_watched_at", quando, sizeof quando);
      v.quando = vistoEpoch(quando);
      if (v.quando) vistoAdd(dst, n, max, &v);
    } else {
      const char *s;
      snprintf(v.tipo, sizeof v.tipo, "series");
      for (s = js_array(p, f, "seasons"); s && *s == '{'; s = js_prox(js_fim(s))) {
        const char *sf = js_fim(s), *e;
        int temporada = (int)js_num(s, sf, "number", 0);
        for (e = js_array(s, sf, "episodes"); e && *e == '{'; e = js_prox(js_fim(e))) {
          const char *ef = js_fim(e);
          quando[0] = 0;
          js_texto_raiz_em(e, ef, "watched_at", quando, sizeof quando);
          v.temporada = temporada; v.episodio = (int)js_num(e, ef, "number", 0);
          v.quando = vistoEpoch(quando);
          if (v.quando) vistoAdd(dst, n, max, &v); else semData++;
        }
      }
    }
  }
  if (semData) printf("[simkl] %d episodio(s) sem watched_at descartados em %s\n", semData, lista);
}

int simkl_ler_vistos(const char *json, SimklVisto *dst, int max) {
  int n = 0;
  if (!dst || max <= 0 || !perfilHistoricoValido(json)) return 0;
  vistosTipo(json, "movies", 1, dst, &n, max);
  vistosTipo(json, "shows", 0, dst, &n, max);
  vistosTipo(json, "anime", 0, dst, &n, max);
  return n;
}

// Only activities are polled. HTTP, token files and persistence stay off UI.
#define VISTOS_TTL_S 120
#define VISTOS_CACHE_MAX 24
#define VISTOS_PERFIS 16
static struct {
  SimklVisto itens[VISTOS_CACHE_MAX];
  int n, vivo, pronto;
  char nome[128], datas[3][48];
  time_t quando;
} vistosCache[VISTOS_PERFIS + 1];
static unsigned vistosRev;
unsigned simkl_vistos_revisao(void) { return __atomic_load_n(&vistosRev, __ATOMIC_ACQUIRE); }
typedef struct {
  int perfil;
  SmkPedido pedido;
} VistosPedido;

// Reuse the scrobble journal's stable token fingerprint; never persist tokens.
static void scrobbleJornalNome(char *, size_t, const char *);
static void vistosNome(char *dst, size_t tam, int perfil, const char *tk) {
  char vinculo[80]; scrobbleJornalNome(vinculo, sizeof vinculo, tk);
  snprintf(dst, tam, "simkl-history-p%d-%s", perfil, vinculo);
}

// Local, versioned binary records encoded as hex for dados_gravar's atomic
// string writer. A different record size/ABI invalidates this disposable cache.
static int vistosDisco(const char *nome, SimklVisto *itens, int *n,
                        char datas[3][48], int gravar) {
  size_t bytes = sizeof *itens * (size_t)*n;
  char *b;
  if (gravar) {
    b = calloc(1, 256 + bytes * 2); if (!b) return 0;
    int k = snprintf(b, 256, "SMKH1 %zu %d\n%s\n%s\n%s\n", sizeof *itens, *n, datas[0], datas[1], datas[2]);
    const unsigned char *p = (const unsigned char *)itens;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < bytes; i++) { b[k++] = hex[p[i] >> 4]; b[k++] = hex[p[i] & 15]; }
    int ok = dados_gravar(nome, b); free(b); return ok;
  }
  b = dados_ler(nome); if (!b) return 0;
  size_t tamanho = 0; int qtd = 0, k = 0;
  int ok = sscanf(b, "SMKH1 %zu %d\n%47[^\n]\n%47[^\n]\n%47[^\n]\n%n",
                  &tamanho, &qtd, datas[0], datas[1], datas[2], &k) == 5;
  bytes = qtd >= 0 && qtd <= VISTOS_CACHE_MAX ? sizeof *itens * (size_t)qtd : 0;
  ok = ok && tamanho == sizeof *itens && qtd >= 0 && qtd <= VISTOS_CACHE_MAX &&
       strlen(b + k) == bytes * 2;
  for (int i = 0; ok && i < 3; i++) ok = js_ms_iso(datas[i]) > 0;
  unsigned char *p = (unsigned char *)itens;
  for (size_t i = 0; ok && i < bytes; i++) {
    unsigned v = 0;
    const char *hex = "0123456789abcdef";
    if (!strchr(hex, b[k + i * 2]) || !strchr(hex, b[k + i * 2 + 1]) ||
        sscanf(b + k + i * 2, "%2x", &v) != 1) ok = 0; else p[i] = (unsigned char)v;
  }
  for (int i = 0; ok && i < qtd; i++)
    ok = memchr(itens[i].imdb, 0, sizeof itens[i].imdb) &&
         memchr(itens[i].tipo, 0, sizeof itens[i].tipo) &&
         memchr(itens[i].titulo, 0, sizeof itens[i].titulo) &&
         memchr(itens[i].poster, 0, sizeof itens[i].poster);
  if (ok) *n = qtd;
  free(b); return ok;
}

static int vistosDatas(const char *json, char datas[3][48]) {
  static const char *tipos[] = { "movies", "tv_shows", "anime" };
  if (!json || *json != '{' || js_fim(json)[-1] != '}') return 0;
  const char *end = js_fim(json);
  while (*end && (unsigned char)*end <= ' ') end++;
  if (*end) return 0;
  for (int i = 0; i < 3; i++) {
    const char *p, *fim;
    const char *v, *vf;
    if (!objetoRaiz(json, js_fim(json), tipos[i], &p, &fim) ||
        !valorRaiz(p, fim, "completed", &v, &vf)) return 0;
    // An unused list has a null timestamp, not a reason to refetch everything.
    if (vf - v == 4 && !strncmp(v, "null", 4))
      snprintf(datas[i], 48, "1970-01-01T00:00:01Z");
    else if (!js_texto_raiz_em(p, fim, "completed", datas[i], 48) ||
             !js_ms_iso(datas[i])) return 0;
  }
  return 1;
}

static void *fioVistos(void *u) {
  VistosPedido a = *(VistosPedido *)u; free(u);
  SimklVisto itens[VISTOS_CACHE_MAX] = {0}, novos[VISTOS_CACHE_MAX] = {0};
  char tk[300] = "", atual[300] = "", nome[128];
  char datas[3][48] = {{0}}, recebidas[3][48] = {{0}};
  int n = 0, st = 0, pronto = 0, vinculado = 0;
  if (!simklauth_token_perfil(a.perfil, tk, sizeof tk)) goto fim;
  vistosNome(nome, sizeof nome, a.perfil, tk);
  if (!pedidoTravar(a.pedido)) goto fim;
  if (vistosCache[a.perfil].pronto && !strcmp(vistosCache[a.perfil].nome, nome)) {
    n = vistosCache[a.perfil].n;
    memcpy(itens, vistosCache[a.perfil].itens, sizeof itens);
    memcpy(datas, vistosCache[a.perfil].datas, sizeof datas);
    pronto = 1;
  }
  pedidoDestravar();
  if (!pronto) pronto = vistosDisco(nome, itens, &n, datas, 0);
  char *r = pedirComToken('G', "/sync/activities", NULL, NULL, &st, tk);
  int valido = st >= 200 && st < 300 && vistosDatas(r, recebidas);
  free(r);
  if (valido && (!pronto || memcmp(datas, recebidas, sizeof datas))) {
    char consulta[256] = "extended=full&episode_watched_at=yes";
    if (pronto) {
      // Earliest prior type watermark covers independent movie/show/anime updates.
      const char *desde = datas[0];
      for (int i = 1; i < 3; i++) if (js_ms_iso(datas[i]) < js_ms_iso(desde)) desde = datas[i];
      char escaped[160]; nuvem_url_escapar(desde, escaped, sizeof escaped);
      snprintf(consulta, sizeof consulta, "extended=full&episode_watched_at=yes&date_from=%s", escaped);
    }
    if (!pedidoAtual(a.pedido)) goto fim;
    r = pedirComToken('G', "/sync/all-items/all/completed", consulta, NULL, &st, tk);
    if (st >= 200 && st < 300 && perfilHistoricoValido(r)) {
      int nn = simkl_ler_vistos(r, novos, VISTOS_CACHE_MAX);
      for (int i = 0; i < nn; i++) vistoAdd(itens, &n, VISTOS_CACHE_MAX, &novos[i]);
      memcpy(datas, recebidas, sizeof datas); pronto = 1;
    }
    free(r);
  }
  vinculado = simklauth_token_perfil(a.perfil, atual, sizeof atual) && !strcmp(tk, atual);
  if (vinculado && pronto && pedidoAtual(a.pedido))
    // Disk I/O never holds the cache mutex read by UI.
    vistosDisco(nome, itens, &n, datas, 1);
fim:
  memset(tk, 0, sizeof tk); memset(atual, 0, sizeof atual);
  if (pedidoTravar(a.pedido)) {
    int publicar = vinculado && pronto;
    if (publicar) {
      memcpy(vistosCache[a.perfil].itens, itens, sizeof itens);
      memcpy(vistosCache[a.perfil].datas, datas, sizeof datas);
      snprintf(vistosCache[a.perfil].nome, sizeof vistosCache[a.perfil].nome, "%s", nome);
      vistosCache[a.perfil].n = n; vistosCache[a.perfil].pronto = 1;
    } else if (!vinculado) {
      publicar = vistosCache[a.perfil].pronto;
      vistosCache[a.perfil].n = vistosCache[a.perfil].pronto = 0;
    }
    vistosCache[a.perfil].vivo = 0;
    if (publicar) __atomic_add_fetch(&vistosRev, 1, __ATOMIC_RELEASE);
    pedidoDestravar();
  }
  return NULL;
}

int simkl_vistos_perfil(int perfil, SimklVisto *dst, int max) {
  if (perfil < 1 || perfil > VISTOS_PERFIS || !dst || max <= 0) return 0;
  SmkPedido pedido = pedidoCapturar();
  if (!pedidoTravar(pedido)) return 0;
  time_t agora = time(NULL);
  if (!vistosCache[perfil].vivo && (!vistosCache[perfil].quando ||
      agora - vistosCache[perfil].quando >= VISTOS_TTL_S)) {
    VistosPedido *a = malloc(sizeof *a);
    if (a) {
      a->perfil = perfil; a->pedido = pedido;
      pthread_t t;
      vistosCache[perfil].vivo = 1;
      if (!pthread_create(&t, NULL, fioVistos, a)) {
        vistosCache[perfil].quando = agora; pthread_detach(t);
      } else { vistosCache[perfil].vivo = 0; free(a); }
    }
  }
  int n = vistosCache[perfil].n < max ? vistosCache[perfil].n : max;
  memcpy(dst, vistosCache[perfil].itens, sizeof *dst * (size_t)n);
  pedidoDestravar(); return n;
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

// CACHE PELO /sync/activities. A doc e explicita: "For continuous sync, do NOT
// call /sync/all-items on a timer", e ameaca suspender o client_id. A regra
// aqui e a mais simples que a cumpre: o corpo das atividades e guardado por
// assinatura; igual ao da ultima leitura = nada mudou em lista nenhuma, e os
// corpos guardados valem. As atividades custam um GET pequeno por ciclo, que e
// o que a doc chama de "the cheapest call in the API". Nao usa date_from: as
// listas lidas aqui (watching, plantowatch) sao pequenas, e o incremental
// exigiria guardar e fundir o estado entre arranques.
static unsigned long assCw;
static char *corpoPlay;

// THE LIBRARY IN MEMORY, for the session. Loaded in the background when the
// profile opens (simkl_biblioteca_iniciar): the saved copy first, then the
// network only when /sync/activities moved since it.
// Under `trava`. The body is replaced whole, never edited in place.
static char *bibCorpo;      // all-items body
static char *bibAtiv;       // the activities it reflects; NULL = unknown
static uint64_t bibSessao, bibGeracao;
static time_t bibConferida; // last activities check that reached Simkl
static int bibFio;          // a loader is running for bibFioSessao/Geracao
static int bibVivos;        // loader threads alive, of any session
static time_t bibTentarApos;
static unsigned bibFalhas;
static uint64_t bibFioSessao, bibFioGeracao;
static pthread_cond_t bibMudou = PTHREAD_COND_INITIALIZER;

static void scrobbleZerar(void);

void simkl_esquecer(void) {
  scrobbleZerar();
  pthread_mutex_lock(&trava);
  if (!++cacheGeracao) ++cacheGeracao;
  listaFioVivo = 0;
  __atomic_store_n(&listaEstado, SMK_OP_NENHUMA, __ATOMIC_RELEASE);
  nPlay = nProx = nPtw = 0;
  memset(vistosCache, 0, sizeof vistosCache);
  __atomic_add_fetch(&vistosRev, 1, __ATOMIC_RELEASE);
  assCw = 0;
  trocar(&corpoPlay, NULL);
  trocar(&perfilSettings, NULL);
  perfilSettingsQuando[0] = 0;
  trocar(&bibCorpo, NULL); trocar(&bibAtiv, NULL);
  bibSessao = bibGeracao = 0;
  bibConferida = 0;
  bibTentarApos = 0;
  bibFalhas = 0;
  bibFio = 0;
  pthread_cond_broadcast(&bibMudou);
  pthread_mutex_unlock(&trava);
}

void simkl_abrir_sessao(void) { simkl_esquecer(); }

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


// THE SAVED LIBRARY (simklbib.h). The whole pull below joins its answers (one
// per type, or per status over max_items) into one body of all-items' shape,
// and saves it with the activities
// asked just before: a later delta from that "all" may repeat a change, never
// miss one.
typedef struct { char *p; size_t n, cap; } SmkTexto;

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

static char *bibliotecaPedido(SmkPedido pedido, const char *caminho, const char *episodios,
                              const char *desde, int *st, int *grande) {
  char url[720], consulta[300], *erro = NULL, *r;
  const char *cab[3];
  int proxima;   // same transport as Simkl's other GETs; all-items has no pages
  // full_anime_seasons is full plus, for anime, each episode's TVDB season and
  // episode and the entry's mapped_tvdb_seasons. It also gives a per-season
  // entry the show's IDs: with full, 21 of 86 anime entries on a real account
  // came with no IMDb or TMDB ID (My Hero Academia S2-7, Spy x Family...).
  // language=en: an anime's title is otherwise its romaji name; language=pt
  // was tried and gave romaji too. No page/limit: Simkl documents no paging
  // for all-items, it answers the whole list or max_items.
  snprintf(consulta, sizeof consulta,
           "extended=full_anime_seasons&include_all_episodes=%s&episode_watched_at=yes"
           "&episode_tvdb_id=yes&language=en%s%s",
           episodios, desde ? "&date_from=" : "", desde ? desde : "");
  snprintf(url, sizeof url, "%s%s?client_id=%s&app-name=%s&app-version=1.0.1&%s",
           SMK_API, caminho, pedido.cid, pedido.nome, consulta);
  cab[0] = pedido.aut;
  cab[1] = "Accept: application/json";
  cab[2] = NULL;
  *grande = 0;
  r = provedorGetErro(url, 25, cab, st, &proxima, 1, &erro);
  if (!r && *st == 400 && erro && strstr(erro, "\"max_items\"")) *grande = 1;
  free(erro);
  return r;
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

static const char *const bibTipos[3] = { "shows", "anime", "movies" };

// Appends one answer's items, list by list. `estado`: the answer is one
// status list; an item without its own "status" gets it, since the library's
// readers pick items by that field. 0 when the answer is not all-items.
static int bibliotecaJuntarEstado(SmkTexto tipos[3], const char *resposta, const char *estado) {
  const char *ini = resposta, *fim, *v, *vf;
  if (!perfilHistoricoValido(resposta)) return 0;
  while ((unsigned char)*ini <= ' ') ini++;
  fim = js_fim(ini);
  for (int i = 0; i < 3; i++) {
    if (!valorRaiz(ini, fim, bibTipos[i], &v, &vf)) continue;
    if (estado) {
      for (const char *p = v + 1; p < vf; ) {
        const char *f, *sv, *svf;
        while (p < vf && ((unsigned char)*p <= ' ' || *p == ',')) p++;
        if (p >= vf || *p == ']') break;
        if (*p != '{' || !(f = js_fim(p)) || f > vf) return 0;
        if (tipos[i].n && !textoPor(&tipos[i], ",", 1)) return 0;
        if (valorRaiz(p, f, "status", &sv, &svf)) {
          if (!textoPor(&tipos[i], p, (size_t)(f - p))) return 0;
        } else {
          const char *q = p + 1;
          char cab[40];
          while (q < f && (unsigned char)*q <= ' ') q++;
          int n = snprintf(cab, sizeof cab, "{\"status\":\"%s\"%s", estado, *q == '}' ? "" : ",");
          if (!textoPor(&tipos[i], cab, (size_t)n) ||
              !textoPor(&tipos[i], p + 1, (size_t)(f - p - 1))) return 0;
        }
        p = f;
      }
      continue;
    }
    const char *a = v + 1, *b = vf - 1;
    while (a < b && (unsigned char)*a <= ' ') a++;
    while (b > a && (unsigned char)b[-1] <= ' ') b--;
    if (a == b) continue;
    if ((tipos[i].n && !textoPor(&tipos[i], ",", 1)) ||
        !textoPor(&tipos[i], a, (size_t)(b - a))) return 0;
  }
  return 1;
}

#define SMK_BIB_ITENS_MAX 20000
#define SMK_BIB_BYTES_MAX (8u * 1024u * 1024u)

static int bibliotecaLimitada(const char *corpo) {
  const char *fim = corpo ? js_fim(corpo) : NULL;
  int n = 0;
  if (!fim || strlen(corpo) > SMK_BIB_BYTES_MAX) return 0;
  for (int i = 0; i < 3; i++) {
    const char *a, *af;
    if (!valorRaiz(corpo, fim, bibTipos[i], &a, &af) || *a != '[') continue;
    for (const char *p = js_array(corpo, NULL, bibTipos[i]); p && *p == '{'; p = js_prox(js_fim(p)))
      if (++n > SMK_BIB_ITENS_MAX) return 0;
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
  if (!ok || !bibliotecaLimitada(t.p)) { free(t.p); return NULL; }
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

// One request joined into `tipos`: 1 joined, 0 failed, -1 max_items.
static int bibliotecaJuntarPedido(SmkPedido pedido, SmkTexto tipos[3], const char *caminho,
                                  const char *episodios, const char *desde,
                                  const char *estado, int *st) {
  int grande = 0;
  char *json;
  if (!pedidoAtual(pedido)) return 0;
  // A body only comes with a 2xx answer.
  json = bibliotecaPedido(pedido, caminho, episodios, desde, st, &grande);
  if (!json) return grande ? -1 : 0;
  int ok = corpoVazio(json) || bibliotecaJuntarEstado(tipos, json, estado);
  free(json);
  return ok;
}

// One type of the whole library. Over Simkl's item or episode cap (400
// max_items), the guide's split: one request per status, and a status still
// over it with include_all_episodes=original. 1 = joined.
static int bibliotecaPuxarTipo(SmkPedido pedido, SmkTexto tipos[3], const char *tipo, int *st) {
  // Movies have no watching or hold list.
  static const char *const estados[] = { "watching", "plantowatch", "hold", "completed", "dropped" };
  static const char *const estadosFilmes[] = { "plantowatch", "completed", "dropped" };
  const char *const *lista = strcmp(tipo, "movies") ? estados : estadosFilmes;
  int n = strcmp(tipo, "movies") ? 5 : 3, r;
  char caminho[64];
  snprintf(caminho, sizeof caminho, "/sync/all-items/%s", tipo);
  r = bibliotecaJuntarPedido(pedido, tipos, caminho, "yes", NULL, NULL, st);
  if (r >= 0) return r;
  printf("[simkl] library: %s over Simkl's max_items, asking it status by status\n", tipo);
  for (int i = 0; i < n; i++) {
    snprintf(caminho, sizeof caminho, "/sync/all-items/%s/%s", tipo, lista[i]);
    r = bibliotecaJuntarPedido(pedido, tipos, caminho, "yes", NULL, lista[i], st);
    if (r < 0) {
      printf("[simkl] library: %s %s still over it, only the recorded episodes\n",
             tipo, lista[i]);
      r = bibliotecaJuntarPedido(pedido, tipos, caminho, "original", NULL, lista[i], st);
    }
    if (r <= 0) return 0;
  }
  return 1;
}

// The library from the network: whole (one type after the other, as the
// sync guide asks), or (`desde`) the items changed since, in one request on
// bare /sync/all-items (the guide's path for an app with several types).
// 1 = *corpo set, 0 failed, -1 the delta was over max_items.
static int bibliotecaPuxar(SmkPedido pedido, const char *desde, char **corpo, int *st) {
  SmkTexto tipos[3] = {{0}};
  int ok = 1;
  *corpo = NULL;
  if (desde) ok = bibliotecaJuntarPedido(pedido, tipos, "/sync/all-items", "yes",
                                         desde, NULL, st);
  else
    for (int i = 0; ok > 0 && i < 3; i++) ok = bibliotecaPuxarTipo(pedido, tipos, bibTipos[i], st);
  if (ok > 0 && !(*corpo = bibliotecaMontar(tipos))) ok = 0;
  for (int i = 0; i < 3; i++) free(tipos[i].p);
  return ok;
}

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
static void bibPublicar(SmkPedido p, char *corpo, char *ativ, int conferida) {
  if (corpo && !bibliotecaLimitada(corpo)) { free(corpo); free(ativ); return; }
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
  if (!ok || !bibliotecaLimitada(t.p)) { free(t.p); return NULL; }
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
// 2 *novo and *delta set, 3 a merge cannot be trusted or the changes are over
// max_items: download it whole.
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
    int r = bibliotecaPuxar(pedido, desde, delta, st);
    if (r < 0) {
      printf("[simkl] library: changes over Simkl's max_items, downloading it whole\n");
      return 3;
    }
    if (!r) return 0;
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
  // The delta's own items: one that replaced nothing is new to the library,
  // so "replaced" alone reads 0 for an item just added on Simkl.
  {
    SmkItem *ds = NULL;
    int nd = itensLer(*delta, &ds);
    free(ds);
    printf("[simkl] library: delta since %s: %d item(s) in it, %d library item(s) replaced, "
           "%d removed%s\n", desde, nd < 0 ? 0 : nd, trocados, removidos,
           removeu ? " (removals checked)" : "");
  }
  return 2;
}

typedef struct { SmkPedido pedido; int perfil; unsigned long long cred; } BibCarga;

static void bibGravar(BibCarga c, char *ativ, char *corpo) {
  if (pedidoTravar(c.pedido)) {
    if (simklauth_credencial_travar(c.cred, c.perfil, c.pedido.aut)) {
      simklbib_gravar(c.perfil, pedidoConta(c.pedido), &(SimklBib){ ativ, corpo });
      simklauth_credencial_destravar();
    }
    pedidoDestravar();
  }
}

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
      bibPublicar(pedido, salvo.biblioteca, salvo.atividades, 0);
    }
  }
  // 2. Activities: unchanged since the copy = nothing to download. A write
  // after this point flags the library again.
  __atomic_store_n(&bibSujo, 0, __ATOMIC_RELEASE);
  ativ = bibliotecaAtividades(pedido);
  if (pedidoTravar(pedido)) {
    if (ativ) {
      bibFalhas = 0;
      bibTentarApos = 0;
    } else {
      if (bibFalhas < 5) bibFalhas++;
      bibTentarApos = time(NULL) + (30 << (bibFalhas - 1));
    }
    pedidoDestravar();
  }
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
  // r 3: no library yet, one without its activities, one a merge cannot be
  // trusted with, or changes over max_items: the whole library below.
  if (tem && ativ && bibAtivCopiar(pedido, &corpo, &ativVelho))
    r = bibliotecaAtualizar(pedido, corpo, ativVelho, ativ, &novo, &delta, &st);
  free(corpo);
  free(ativVelho);
  corpo = NULL;
  if (r == 2) {
    free(delta);
    bibGravar(c, ativ, novo);
    bibPublicar(pedido, novo, ativ, 1);
  } else if (r == 1) {
    printf("[simkl] library: only playback/ratings/settings moved, nothing to download\n");
    bibPublicar(pedido, NULL, ativ, 1);
  } else if (r == 0) {
    // Nothing saved or published: the next reader asks again.
    printf("[simkl] library: delta failed (HTTP %d), keeping the library\n", st);
    free(ativ);
    bibPublicar(pedido, NULL, NULL, 0);
  } else if (bibliotecaPuxar(pedido, NULL, &corpo, &st) > 0) {
    // Without activities the copy is used but not saved: a later delta would
    // have no date to start from.
    if (ativ) bibGravar(c, ativ, corpo);
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
  if (pedido.geracao != cacheGeracao || bibFio || agora < bibTentarApos ||
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
    c->cred = simklauth_credencial_copiar(NULL, 0);
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
#define SMK_BIB_FRESCA 900
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

int simkl_continuar(CatItem *saida, int max) {
  CatItem *prox;
  long long ids[SMK_PLAY_MAX];
  unsigned long h;
  char *cPlay = NULL, *cAssist = NULL, *cAnime = NULL;
  char proxLocal[SMK_PLAY_MAX][28]; int nProxLocal = 0;
  int n, nP, nA, i, k;
  SmkPedido pedido = pedidoCapturar();
  char *lib, *ativ = NULL;
  int perfil;
  if (!saida || max <= 0 || !pedido.aut[0]) return 0;
  if (max > SMK_PLAY_MAX) max = SMK_PLAY_MAX;
  if ((perfil = pedidoPerfil(pedido)) < 0) return 0;
  // The lists come from the library (status watching); only playback is not
  // in it. Playback is asked again when its own timestamps move
  // (assinarPlayback) or after a write from this app.
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
  free(ativ); free(lib);
  if (!pedidoAtual(pedido) || !cPlay || !cAssist || !cAnime) {
    free(cPlay); free(cAssist); free(cAnime); return 0;
  }
  n = simkl_ler_playback(cPlay, saida, ids, max);
  int nPausados = n;
  prox = malloc(sizeof(CatItem) * SMK_PLAY_MAX);
  nP = prox && ajustes_cw_proximo() ? simkl_ler_assistindo(cAssist, prox, SMK_PLAY_MAX) : 0;
  // O mesmo leitor aceita a raiz "anime"; funde aqui, dedup por serie, para
  // nao repetir um titulo que o endpoint de shows ja tenha trazido.
  if (!prox || !ajustes_cw_proximo())
    printf("[simkl] anime watching: skipped (%s)\n", prox ? "next-up disabled" : "allocation failed");
  if (prox && ajustes_cw_proximo()) {
    nA = simkl_ler_assistindo(cAnime, prox + nP, SMK_PLAY_MAX - nP);
    for (k = nP; k < nP + nA; k++) {
      int j, ja = 0;
      for (j = 0; j < nP && !ja; j++) ja = mesmaSerie(prox[j].imdb, prox[k].imdb);
      if (ja) {
        printf("[simkl] anime watching id=%s: dropped (duplicate show)\n", prox[k].imdb);
        memmove(prox + k, prox + k + 1, sizeof prox[0] * (size_t)(nP + nA - k - 1));
        nA--; k--;
      }
    }
    nP += nA;
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
