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
    if (!objetoRaiz(p, f, chaves[k], &oi, &of)) goto watchingDropped;
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
      if (objetoRaiz(oi, of, "ids", &ii, &ifim))
        d->tmdb = (long)js_num(ii, ifim, "tmdb", 0);
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
    if (!objetoRaiz(p, f, chaves[k], &oi, &of)) continue;
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
}

static void perfilLerTipo(const char *json, const char *lista, int filme,
                          PerfilDados *d, const struct tm *mes) {
  const char *p;
  for (p = js_array(json, NULL, lista); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *obra, *obraFim, *ids, *idsFim;
    char titulo[128] = "", imdb[24] = "", quando[48] = "";
    int runtime;
    PerfilDestaque r = {0};
    if (!objetoRaiz(p, f, filme ? "movie" : !strcmp(lista, "anime") ? "anime" : "show", &obra, &obraFim)) continue;
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
      for (s = js_array(p, f, "seasons"); s && *s == '{'; s = js_prox(js_fim(s))) {
        const char *sf = js_fim(s), *e;
        int temporada = (int)js_num(s, sf, "number", 0);
        for (e = js_array(s, sf, "episodes"); e && *e == '{'; e = js_prox(js_fim(e))) {
          const char *ef = js_fim(e);
          int episodio = (int)js_num(e, ef, "number", 0);
          int duracao = (int)js_num(e, ef, "runtime", runtime);
          quando[0] = 0;
          js_texto_raiz_em(e, ef, "watched_at", quando, sizeof quando);
          perfilSomar(d, mes, quando, &r, duracao, 0, temporada, episodio);
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

int simkl_perfil(PerfilDados *d) {
  char inicio[48], consulta[256], tk[300];
  unsigned long long credencial;
  char *settings = NULL, *itens = NULL;
  time_t agora = time(NULL), limite;
  struct tm primeiro, utc;
  int st = 0, ok;
  if (!d) return 0;
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

// CACHE PELO /sync/activities. A doc e explicita: "For continuous sync, do NOT
// call /sync/all-items on a timer", e ameaca suspender o client_id. A regra
// aqui e a mais simples que a cumpre: o corpo das atividades e guardado por
// assinatura; igual ao da ultima leitura = nada mudou em lista nenhuma, e os
// corpos guardados valem. As atividades custam um GET pequeno por ciclo, que e
// o que a doc chama de "the cheapest call in the API". Nao usa date_from: as
// listas lidas aqui (watching, plantowatch) sao pequenas, e o incremental
// exigiria guardar e fundir o estado entre arranques.
static unsigned long assCw, assPtw;
static char *corpoPlay, *corpoAssist, *corpoAssistAnime,
            *corpoPtwFilmes, *corpoPtwSeries, *corpoPtwAnime;

static void trocar(char **dst, char *novo) { free(*dst); *dst = novo; }

static void scrobbleZerar(void);

void simkl_esquecer(void) {
  scrobbleZerar();
  pthread_mutex_lock(&trava);
  if (!++cacheGeracao) ++cacheGeracao;
  listaFioVivo = 0;
  __atomic_store_n(&listaEstado, SMK_OP_NENHUMA, __ATOMIC_RELEASE);
  nPlay = nProx = nPtw = 0;
  assCw = assPtw = 0;
  trocar(&corpoPlay, NULL); trocar(&corpoAssist, NULL);
  trocar(&corpoAssistAnime, NULL);
  trocar(&corpoPtwFilmes, NULL); trocar(&corpoPtwSeries, NULL);
  trocar(&corpoPtwAnime, NULL);
  pthread_mutex_unlock(&trava);
}

void simkl_abrir_sessao(void) { simkl_esquecer(); }

static unsigned long atividades(SmkPedido pedido) {
  int st = 0;
  char *r = pegar(pedido, "/sync/activities", NULL, &st);
  unsigned long h = assinar(r);
  free(r);
  return h;
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
  if (!saida || max <= 0 || !pedido.aut[0]) return 0;
  if (max > SMK_PLAY_MAX) max = SMK_PLAY_MAX;
  h = atividades(pedido);
  if (!pedidoTravar(pedido)) return 0;
  if (h && h == assCw && corpoPlay && corpoAssist && corpoAssistAnime) {
    cPlay = strdup(corpoPlay); cAssist = strdup(corpoAssist);
    cAnime = strdup(corpoAssistAnime);
    printf("[simkl] resume: activities unchanged, cached playback/shows/anime bodies\n");
  }
  pedidoDestravar();
  if (!cPlay || !cAssist || !cAnime) {
    int st = 0; free(cPlay); free(cAssist); free(cAnime);
    cPlay = pegar(pedido, "/sync/playback", NULL, &st);
    cAssist = pegar(pedido, "/sync/all-items/shows/watching", NULL, &st);
    // Anime e um TERCEIRO tipo no Simkl: /sync/all-items/anime/<estado>.
    // Sem este pedido, nenhum anime aparece no Continuar assistindo.
    cAnime = pegar(pedido, "/sync/all-items/anime/watching", NULL, &st);
    printf("[simkl] anime watching fetch: status=%d body=%s\n", st, cAnime ? "received" : "missing");
    if (pedidoTravar(pedido)) {
      if (h && cPlay && cAssist && cAnime) {
        trocar(&corpoPlay, strdup(cPlay)); trocar(&corpoAssist, strdup(cAssist));
        trocar(&corpoAssistAnime, strdup(cAnime)); assCw = h;
      } else assCw = 0;
      pedidoDestravar();
    }
  }
  if (!pedidoAtual(pedido)) { free(cPlay); free(cAssist); free(cAnime); return 0; }
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
  unsigned long h; char *cF = NULL, *cS = NULL, *cA = NULL; int n = 0, nA, i, j;
  SmkPedido pedido = pedidoCapturar();
  if (!saida || max <= 0 || !pedido.aut[0]) return 0;
  h = atividades(pedido);
  if (!pedidoTravar(pedido)) return 0;
  if (h && h == assPtw && corpoPtwFilmes && corpoPtwSeries && corpoPtwAnime) {
    cF = strdup(corpoPtwFilmes); cS = strdup(corpoPtwSeries);
    cA = strdup(corpoPtwAnime);
  }
  pedidoDestravar();
  if (!cF || !cS || !cA) {
    int st = 0; free(cF); free(cS); free(cA);
    cF = pegar(pedido, "/sync/all-items/movies/plantowatch", NULL, &st);
    cS = pegar(pedido, "/sync/all-items/shows/plantowatch", NULL, &st);
    // Anime e o terceiro tipo do Simkl: vai para o lado das series.
    cA = pegar(pedido, "/sync/all-items/anime/plantowatch", NULL, &st);
    if (pedidoTravar(pedido)) {
      if (h && cF && cS && cA) {
        trocar(&corpoPtwFilmes, strdup(cF)); trocar(&corpoPtwSeries, strdup(cS));
        trocar(&corpoPtwAnime, strdup(cA)); assPtw = h;
      } else assPtw = 0;
      pedidoDestravar();
    }
  }
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
  assPtw = 0;
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
