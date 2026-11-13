// Simkl como fonte do "Continuar assistindo" e destino do "+" (issue #110),
// contra uma rede FALSA que responde no formato da documentacao oficial
// (api.simkl.org / simkl.docs.apiary.io). Sem SDL, sem rede.
//
// A rede falsa DESPACHA PELA ROTA e conta cada uma: e o que prova que a
// retomada nao baixa all-items quando /sync/activities nao mudou (a doc ameaca
// suspender o client_id de quem faz isso num timer), e que o "-" fora do Plan
// to Watch conhecido NAO manda nada — /sync/history/remove apagaria o
// historico do titulo.
//
// STDOUT E CAPTURADO e conferido no fim: o token nao pode aparecer no que o
// modulo imprime. O progresso do proprio teste sai por stderr.
#include "../src/simkl.h"
#include "nvtmp.h"
#include "../src/ajustes.h"
#include "../src/js.h"
#include "../src/metaprov.h"
#include "../src/rede.h"
#include "../src/simklbib.h"
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOKEN "TOKEN-SIMKL-SEGREDO-110"
#define OK(s) fprintf(stderr, "ok  %s\n", s)

// OS INDICES GRAVADOS nao mudam de sentido. ajustes.txt de quem ja usa o app
// tem "cwFonteLocal 2" (Trakt) e "salvosDestino 1" (Trakt); o valor novo so
// pode ter entrado no fim. O rotulo de cada indice e conferido no .sh, contra
// V_CW_FONTE e V_SALVOS em ajustes.c.
_Static_assert(AJ_CWF_AMBAS == 0 && AJ_CWF_CONTA == 1 && AJ_CWF_TRAKT == 2 &&
               AJ_CWF_SIMKL == 3, "indices de cwFonteLocal mudaram de sentido");
_Static_assert(AJ_SALVOS_LOCAL == 0 && AJ_SALVOS_TRAKT == 1 && AJ_SALVOS_SIMKL == 2,
               "indices de salvosDestino mudaram de sentido");

// ---------------------------------------------------------------- stubs

#ifndef SIMKL_ANIME_FIXTURE
const char *metaprov_idioma(void) { return "en"; }
char *fichameta_ler(const char *key, long ttl) { (void)key; (void)ttl; return NULL; }
#endif

// One file kept for real: the saved library (simklbib.c). The rest of the
// disk stays write-only, as before.
static char bibNome[48], *bibTexto;
static int bibGravacoes;
// And the Simkl anime cache (simklanime.c), for the same reason.
static char *animeTexto;
static int ehBib(const char *nome) { return !strncmp(nome, "simkl-biblioteca-", 17); }
static int ehAnime(const char *nome) { return !strcmp(nome, "simkl-anime.txt"); }
char *dados_ler(const char *nome) {
  if (ehAnime(nome)) return animeTexto ? strdup(animeTexto) : NULL;
  return ehBib(nome) && bibTexto && !strcmp(nome, bibNome) ? strdup(bibTexto) : NULL;
}
int dados_gravar(const char *nome, const char *buf) { (void)nome; (void)buf; return 1; }
int dados_gravar_leve(const char *nome, const char *buf) {
  if (ehAnime(nome)) { free(animeTexto); animeTexto = strdup(buf); return 1; }
  if (!ehBib(nome)) return 1;
  free(bibTexto);
  bibTexto = strdup(buf);
  snprintf(bibNome, sizeof bibNome, "%s", nome);
  bibGravacoes++;
  return 1;
}
int dados_apagar(const char *nome) {
  if (ehBib(nome) && !strcmp(nome, bibNome)) { free(bibTexto); bibTexto = NULL; }
  return 1;
}
#ifndef SIMKL_ANIME_FIXTURE
char *metaprov_meta(const char *tipo, const char *id, int seg, int *prov) {
  (void)tipo; (void)id; (void)seg; (void)prov; return NULL;
}
char *metaprov_meta_cinemeta_com(const char *tipo, const char *id, int seg,
                                 MetaprovGet get, void *ctx,
                                 MetaprovCineResult *result) {
  (void)tipo; (void)id; (void)seg; (void)get; (void)ctx;
  if (result) *result = METAPROV_CINE_RETRY;
  return NULL;
}
void fichameta_gravar(const char *key, const char *body) { (void)key; (void)body; }
#endif
static int proximoTeste = 1;
int ajustes_cw_proximo(void) { return proximoTeste; }
int ajustes_cw_minimo(void) { return 1; }
int ajustes_cw_concluido(void) { return 90; }
static const char *tokenAtual = "";
const char *simklauth_token(void) { return tokenAtual; }
int simklauth_invalidar_se(unsigned long long cred) { (void)cred; tokenAtual = ""; return 1; }
static unsigned long long credGeracao = 1;
unsigned long long simklauth_credencial_copiar(char *dst, size_t tam) {
  if (dst && tam) snprintf(dst, tam, "%s", tokenAtual);
  return credGeracao;
}
int simklauth_perfil(void) { return 2; }
const char *nuvem_simkl_cliente(void) { return "cid-teste"; }
const char *nuvem_simkl_app(void) { return "nuvio"; }
void nuvem_url_escapar(const char *v, char *dst, unsigned tam) { snprintf(dst, tam, "%s", v); }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
// O enfeite real vai ao Cinemeta; aqui devolve o lote como veio.
int trakt_enfeitar_lote(CatItem *saida, int n) { (void)saida; return n; }
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
int trakt_enfeitar_lote_sessao(CatItem *saida, int n, uint64_t g) {
  assert(perfis_sessao_atual(g));
  return trakt_enfeitar_lote(saida, n);
}

// ---------------------------------------------------------------- rede falsa

// Resposta de /sync/playback copiada do exemplo da doc, mais um anime com
// numeracao TVDB, um anime SO com AniDB (tem de sair) e um item sem imdb.
static const char *PLAYBACK =
  "[{\"id\":123,\"progress\":45.5,\"paused_at\":\"2024-01-15T10:30:00.000Z\","
  "\"type\":\"episode\",\"episode\":{\"season\":1,\"episode\":5,\"title\":\"Episode 5\","
  "\"tvdb_season\":1,\"tvdb_number\":5},\"show\":{\"title\":\"Breaking Bad\",\"year\":2008,"
  "\"ids\":{\"simkl\":12345,\"slug\":\"breaking-bad\",\"tmdb\":1429,\"imdb\":\"tt0903747\"}}},"
  "{\"id\":124,\"progress\":75,\"paused_at\":\"2024-01-15T11:15:00.000Z\",\"type\":\"movie\","
  "\"movie\":{\"title\":\"Inception\",\"year\":2010,\"ids\":{\"simkl\":67890,"
  "\"slug\":\"inception\",\"tmdb\":\"27205\",\"imdb\":\"tt1375666\"}}},"
  "{\"id\":125,\"progress\":30,\"paused_at\":\"2024-01-14T09:00:00Z\",\"type\":\"episode\","
  "\"episode\":{\"season\":1,\"number\":40,\"tvdb_season\":2,\"tvdb_number\":3},"
  "\"anime\":{\"title\":\"Attack on Titan\",\"ids\":{\"mal\":16498,\"imdb\":\"tt2560140\"}}},"
  "{\"id\":126,\"progress\":20,\"paused_at\":\"2024-01-13T09:00:00Z\",\"type\":\"episode\","
  "\"episode\":{\"season\":1,\"number\":7},"
  "\"anime\":{\"title\":\"So AniDB\",\"ids\":{\"mal\":1,\"imdb\":\"tt0000001\"}}},"
  "{\"id\":127,\"progress\":50,\"paused_at\":\"2024-01-12T09:00:00Z\",\"type\":\"movie\","
  "\"movie\":{\"title\":\"Sem imdb\",\"ids\":{\"simkl\":1}}}]";

// /sync/all-items/shows/watching: Breaking Bad (ja pausado — nao pode entrar
// duas vezes), uma serie com proximo episodio mais nova que tudo, uma em dia
// (next_to_watch null) e uma sem imdb.
static const char *WATCHING =
  "{\"shows\":[{\"last_watched_at\":\"2024-01-10T08:00:00Z\",\"status\":\"watching\","
  "\"next_to_watch\":\"S01E06\",\"show\":{\"title\":\"Breaking Bad\",\"year\":2008,"
  "\"ids\":{\"simkl\":12345,\"imdb\":\"tt0903747\"}}},"
  "{\"last_watched_at\":\"2024-01-20T21:00:00Z\",\"status\":\"watching\","
  "\"next_to_watch\":\"S02E03\",\"watched_episodes_count\":12,"
  "\"show\":{\"title\":\"Severance\",\"year\":2022,\"ids\":{\"simkl\":9,\"imdb\":\"tt11280740\"}}},"
  "{\"last_watched_at\":\"2024-01-19T21:00:00Z\",\"status\":\"watching\","
  "\"next_to_watch\":null,\"show\":{\"title\":\"Em dia\",\"ids\":{\"imdb\":\"tt7654321\"}}},"
  "{\"last_watched_at\":\"2024-01-18T21:00:00Z\",\"status\":\"watching\","
  "\"next_to_watch\":\"S01E02\",\"show\":{\"title\":\"Sem imdb\",\"ids\":{\"simkl\":5}}}]}";

// /sync/all-items/anime/watching: raiz "anime", objeto interno "anime".
// Um anime com proximo episodio, um em dia (null) e um sem imdb.
static const char *WATCHING_ANIME =
  "{\"anime\":[{\"last_watched_at\":\"2024-01-22T21:00:00Z\",\"status\":\"watching\","
  "\"next_to_watch\":\"S03E04\",\"anime\":{\"title\":\"Frieren\",\"year\":2023,"
  "\"ids\":{\"simkl\":77,\"mal\":52991,\"imdb\":\"tt22248376\"}}},"
  "{\"last_watched_at\":\"2024-01-21T21:00:00Z\",\"status\":\"watching\","
  "\"next_to_watch\":null,\"anime\":{\"title\":\"Em dia\",\"ids\":{\"imdb\":\"tt1111111\"}}},"
  "{\"status\":\"watching\",\"next_to_watch\":\"S01E01\","
  "\"anime\":{\"title\":\"Sem imdb\",\"ids\":{\"simkl\":8}}}]}";

static const char *PTW_FILMES =
  "{\"movies\":[{\"added_to_watchlist_at\":\"2024-01-01T00:00:00Z\",\"status\":\"plantowatch\","
  "\"movie\":{\"title\":\"Dune\",\"year\":2021,\"ids\":{\"simkl\":1,\"imdb\":\"tt1160419\"}}}]}";
static const char *PTW_SERIES =
  "{\"shows\":[{\"status\":\"plantowatch\",\"next_to_watch\":\"S01E01\","
  "\"show\":{\"title\":\"Shogun\",\"year\":2024,\"ids\":{\"imdb\":\"tt2788316\"}}},"
  // O MESMO titulo listado tambem no endpoint de anime: tem de entrar uma vez.
  "{\"status\":\"plantowatch\",\"show\":{\"title\":\"Repetido\","
  "\"ids\":{\"imdb\":\"tt22248376\"}}}]}";
static const char *PTW_ANIME =
  "{\"anime\":[{\"status\":\"plantowatch\","
  "\"anime\":{\"title\":\"Frieren\",\"year\":2023,"
  "\"ids\":{\"mal\":52991,\"imdb\":\"tt22248376\"}}}]}";
static const char *FEITO_ANIME = "{\"anime\":[]}";
// Titles on hold, dropped, or shows completed: empty unless a test fills them.
static const char *HOLD = "{}", *DROPPED = "{}", *FEITO_SERIES = "{}";
static int detalhesAnime;
static const char *WATCHED_HISTORY =
  "{\"movies\":[],"
  "\"shows\":[{\"show\":{\"ids\":{\"imdb\":\"tt0903747\"}},\"seasons\":["
    "{\"number\":1,\"episodes\":[{\"number\":1,\"watched_at\":\"2026-10-10T12:00:00Z\"},"
    "{\"number\":2}]}]}],"
  "\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},\"seasons\":["
    "{\"number\":22,\"episodes\":[{\"number\":1122,\"watched_at\":\"2026-10-11T12:00:00Z\"},"
    "{\"number\":1123,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":1124}]}]},"
    "{\"anime\":{\"ids\":{\"simkl\":38636,\"tmdb\":37854}},\"seasons\":["
    "{\"number\":22,\"episodes\":[{\"number\":36,\"tvdb_season\":22,"
    "\"tvdb_number\":36,\"watched_at\":\"2026-10-13T12:00:00Z\"}]}]}]}";

static char atividades[200] = "{\"all\":\"2024-01-20T21:00:00Z\"}";

// Kaiju No. 8 as Simkl keeps it: two entries with one IMDb ID, episodes
// 1..N each, and every episode's place in TVDB's numbering. Season 2 first:
// with one slot per show, the last entry (S1) decided the next-up.
static const char *KAIJU_HISTORY =
  "{\"movies\":[],\"shows\":[],\"anime\":["
  "{\"show\":{\"title\":\"Kaiju No. 8 Season 2\",\"ids\":{\"simkl\":2638830,\"imdb\":\"tt21975436\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1,\"watched_at\":\"2026-10-03T12:00:00Z\",\"tvdb\":{\"season\":2,\"episode\":1}},"
    "{\"number\":2,\"watched_at\":\"2026-10-04T12:00:00Z\",\"tvdb\":{\"season\":2,\"episode\":2}}]}]},"
  "{\"show\":{\"title\":\"Kaiju No. 8\",\"ids\":{\"simkl\":2071761,\"imdb\":\"tt21975436\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1,\"watched_at\":\"2026-10-01T12:00:00Z\",\"tvdb\":{\"season\":1,\"episode\":1}},"
    "{\"number\":2,\"watched_at\":\"2026-10-02T12:00:00Z\",\"tvdb\":{\"season\":1,\"episode\":2}}]}]}]}";
static const char *historicoCorpo;
static void bibEsperar(void);
static int marcou(const char *id, int t, int e);

// The library /sync/all-items/all/all answers: the status fixtures above
// (watching, plan to watch, completed) and the watched history, one array per
// type, built when asked so a test can swap a fixture.
// Each item of the fixture's array, with the status of the list it stands for
// when the fixture leaves it out (a status endpoint did not need it).
static void juntarArray(char **dst, size_t *n, const char *corpo, const char *chave,
                        const char *estado) {
  const char *p, *f;
  char st[24];
  for (p = js_array(corpo, NULL, chave); p && *p == '{'; p = js_prox(f)) {
    size_t k, extra;
    f = js_fim(p);
    assert(f);
    k = (size_t)(f - p);
    st[0] = 0;
    js_texto_raiz_em(p, f, "status", st, sizeof st);
    extra = !st[0] && estado ? strlen(estado) + 16 : 0;
    *dst = realloc(*dst, *n + k + extra + 2);
    assert(*dst);
    if (*n) (*dst)[(*n)++] = ',';
    if (extra) {
      *n += (size_t)sprintf(*dst + *n, "{\"status\":\"%s\"%s", estado, k > 2 ? "," : "");
      memcpy(*dst + *n, p + 1, k - 1);
      *n += k - 1;
    } else {
      memcpy(*dst + *n, p, k);
      *n += k;
    }
    (*dst)[*n] = 0;
  }
}
static char *bibliotecaFalsa(void);
// The fixture's "all", followed by a fingerprint of the library it would
// download: as on Simkl, data that changed moves the activities, so a test
// that swaps a fixture sees it after the next check.
static char *atividadesFalsas(void) {
  char all[64] = "", *lib = historicoCorpo ? strdup(historicoCorpo) : bibliotecaFalsa(), *r;
  unsigned h = 2166136261u;
  for (const char *c = lib; c && *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
  free(lib);
  if (!js_texto_raiz(atividades, "all", all, sizeof all)) return strdup(atividades);
  r = malloc(strlen(atividades) + 16);
  sprintf(r, "{\"all\":\"%s~%08x\"}", all, h);
  return r;
}
static char *bibliotecaFalsa(void) {
  static const char *const tipos[3] = { "shows", "anime", "movies" };
  // Hold, dropped and completed first: on Simkl an item has one status, and
  // a test puts a title there to move it out of watching.
  struct { const char *corpo, *estado; } fontes[3][8] = {
    { { HOLD, "hold" }, { DROPPED, "dropped" }, { FEITO_SERIES, "completed" },
      { WATCHING, "watching" }, { PTW_SERIES, "plantowatch" }, { WATCHED_HISTORY, NULL } },
    { { HOLD, "hold" }, { DROPPED, "dropped" }, { WATCHING_ANIME, "watching" },
      { PTW_ANIME, "plantowatch" }, { FEITO_ANIME, "completed" }, { WATCHED_HISTORY, NULL } },
    { { HOLD, "hold" }, { DROPPED, "dropped" }, { PTW_FILMES, "plantowatch" },
      { WATCHED_HISTORY, NULL } } };
  size_t tam = 32;
  char *r, *partes[3] = { NULL, NULL, NULL };
  size_t ns[3] = { 0, 0, 0 };
  for (int t = 0; t < 3; t++) {
    for (int f = 0; f < 8 && fontes[t][f].corpo; f++)
      juntarArray(&partes[t], &ns[t], fontes[t][f].corpo, tipos[t], fontes[t][f].estado);
    tam += ns[t] + 16;
  }
  r = malloc(tam);
  snprintf(r, tam, "{\"shows\":[%s],\"anime\":[%s],\"movies\":[%s]}",
           partes[0] ? partes[0] : "", partes[1] ? partes[1] : "", partes[2] ? partes[2] : "");
  for (int t = 0; t < 3; t++) free(partes[t]);
  return r;
}

static int batidas[12];  // activities, playback, watching, ptw filmes, ptw series,
                         // post, delete, outro, anime, watched history,
                         // library delta (date_from), IDs only
// Library deltas (step 2): real-shaped activities, the date_from answer, the
// IDs-only answer, a failing delta, and the date_from last sent.
static const char *atividadesInteiras, *historicoDelta, *historicoSoIds;
static int deltaFalha;
static char ultimoDesde[48];
static char ultimaRota[64], ultimoCorpo[300], ultimaUrl[700];
static int  semBearer, comApiKey;
static VistoPar historicoPares[32];
static char historicoIds[32][24];
static long long historicoMs[32];
static int nHistorico;
static long historicoTmdb;
static char historicoCatalogId[24];
static int historicoCatalogTvdb, historicoCatalogTemporada, historicoCatalogEpisodio;
static CatEp historicoCatalogEps[1300];
static int historicoCatalogEpsQtd, historicoCatalogEpsFinal;

// Whole-show runs (1122 episodes) only record which pairs were marked.
static unsigned char historicoGrade[32][1300];
static int historicoSoGrade;
// Marks also come from Simkl's own threads (library load, mapping again).
static pthread_mutex_t historicoTrava = PTHREAD_MUTEX_INITIALIZER;
static int historicoQtd(void) {
  int n;
  pthread_mutex_lock(&historicoTrava);
  n = nHistorico;
  pthread_mutex_unlock(&historicoTrava);
  return n;
}
static int segurosOnePiece, segurosOutros, marcasSeguradas;   // vistoep_segurar
static int segurosPedidos;   // how many times a count was held at all
static int vistoepFonteTravado(const char *imdb, int temporada, int episodio, int visto,
                               long long remotoMs, VistoFonte *c, uint64_t sessao);
int vistoep_fonte_se_sessao(const char *imdb, int temporada, int episodio, int visto,
                            long long remotoMs, VistoFonte *c, uint64_t sessao) {
  int r;
  pthread_mutex_lock(&historicoTrava);
  r = vistoepFonteTravado(imdb, temporada, episodio, visto, remotoMs, c, sessao);
  pthread_mutex_unlock(&historicoTrava);
  return r;
}
static int vistoepFonteTravado(const char *imdb, int temporada, int episodio, int visto,
                               long long remotoMs, VistoFonte *c, uint64_t sessao) {
  if (historicoSoGrade) {
    assert(sessao == 1 && visto && temporada >= 0 && temporada < 32 &&
           episodio > 0 && episodio < 1300);
    assert(!historicoGrade[temporada][episodio]);
    historicoGrade[temporada][episodio] = 1;
    nHistorico++;
    if (c) c->vistos++;
    return 1;
  }
  if (segurosOnePiece > 0 && !strcmp(imdb, "tt0388629")) marcasSeguradas++;
  assert(sessao == 1 && visto && nHistorico < 32);
  snprintf(historicoIds[nHistorico], sizeof historicoIds[0], "%s", imdb);
  historicoPares[nHistorico].temporada = (short)temporada;
  historicoPares[nHistorico].episodio = (short)episodio;
  historicoMs[nHistorico] = remotoMs;
  nHistorico++;
  if (c) c->vistos++;
  return 1;
}
void vistoep_fonte_log(const char *imdb, const char *fonte, const VistoFonte *c) {
  (void)imdb; (void)fonte; (void)c;
}
// vistoep_esvaziado: a case that starts over with an empty map bumps it, as
// vistoep_esquecer does in the app.
static unsigned mapaEsvaziado;
unsigned vistoep_esvaziado(void) { return mapaEsvaziado; }
// vistoep_segurar: open holds per title, and marks written while One Piece
// was held (the page shows no count while its anime history is mapped).
void vistoep_segurar(const char *imdb, int segurar) {
  int *c = imdb && !strcmp(imdb, "tt0388629") ? &segurosOnePiece : &segurosOutros;
  pthread_mutex_lock(&historicoTrava);
  *c += segurar ? 1 : -1;
  if (segurar) segurosPedidos++;
  assert(*c >= 0);
  pthread_mutex_unlock(&historicoTrava);
}
// Runs once inside the next cat_copiar_por_id: the anime remap thread calls it
// before it looks at the episode list, so a test can change the list there.
static void (*aoCopiarPorId)(void);
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *saida) {
  if (aoCopiarPorId) { void (*f)(void) = aoCopiarPorId; aoCopiarPorId = NULL; f(); }
  if (!id || !tipo || !saida || strcmp(id, "tt0388629") ||
      strcmp(tipo, "series") || historicoTmdb <= 0) return 0;
  memset(saida, 0, sizeof *saida);
  saida->tmdb = historicoTmdb;
  return 1;
}
int cat_indice_por_imdb_tipo(const char *id, const char *tipo) {
  return id && tipo && historicoCatalogId[0] &&
         !strcmp(id, historicoCatalogId) && !strcmp(tipo, "series") ? 0 : -1;
}
int cat_episodios_finais(int indice, const char *id) {
  return indice == 0 && id && historicoCatalogId[0] &&
         !strcmp(id, historicoCatalogId) && historicoCatalogEpsFinal;
}
int cat_n_episodios(int indice) {
  return indice == 0 ? historicoCatalogEpsQtd : 0;
}
// The real counters move whenever a list or its final flag changes; here the
// tests change the stand-in list by hand, so its fingerprint stands in.
unsigned cat_versao_episodios(void) {
  unsigned h = 2166136261u;
  const unsigned char *p = (const unsigned char *)historicoCatalogId;
  for (; *p; p++) h = (h ^ *p) * 16777619u;
  h = (h ^ (unsigned)historicoCatalogEpsQtd) * 16777619u;
  h = (h ^ (unsigned)historicoCatalogEpsFinal) * 16777619u;
  for (int i = 0; i < historicoCatalogEpsQtd && i < (int)(sizeof historicoCatalogEps /
                                                         sizeof *historicoCatalogEps); i++) {
    h = (h ^ (unsigned)historicoCatalogEps[i].temporada) * 16777619u;
    h = (h ^ (unsigned)historicoCatalogEps[i].episodio) * 16777619u;
    h = (h ^ (unsigned)historicoCatalogEps[i].tvdbEp) * 16777619u;
  }
  return h;
}
unsigned cat_revisao(void) { return 0; }
unsigned cat_revisao_itens(void) { return 0; }
const CatEp *cat_episodio(int indice, int i) {
  return indice == 0 && i >= 0 && i < historicoCatalogEpsQtd
           ? &historicoCatalogEps[i] : NULL;
}
int cat_episodio_por_tvdb(int indice, const char *id, int tvdb, int *t, int *e) {
  if (indice != 0 || !id || strcmp(id, historicoCatalogId) ||
      tvdb != historicoCatalogTvdb || tvdb <= 0) return 0;
  if (t) *t = historicoCatalogTemporada;
  if (e) *e = historicoCatalogEpisodio;
  return 1;
}

static void conferirCabecalhos(const char *url, const char *const *cab) {
  int k, bearer = 0;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  // Os tres parametros que a doc nova chama de obrigatorios, na QUERY.
  assert(strstr(url, "client_id=cid-teste"));
  assert(strstr(url, "app-name=nuvio"));
  assert(strstr(url, "app-version="));
  for (k = 0; cab && cab[k]; k++) {
    if (!strcmp(cab[k], "Authorization: Bearer " TOKEN)) bearer = 1;
    if (!strncasecmp(cab[k], "simkl-api-key", 13)) comApiKey = 1;
  }
  if (!bearer) semBearer = 1;
}

static void rota(const char *url, char *dst, size_t tam) {
  const char *p = strstr(url, "api.simkl.com");
  size_t n;
  assert(p);
  p += strlen("api.simkl.com");
  n = strcspn(p, "?");
  if (n >= tam) n = tam - 1;
  memcpy(dst, p, n); dst[n] = 0;
}

static int perfilRede, trocarNaResposta, perfilHistorySt = 200, libraryFailure;
static int pedidosSettings, pedidosMesPerfil;   // Profile & Stats requests
// History pull: activities fail, Simkl forgotten mid-pull, activities count
// when the library was asked.
static int atividadesFalham, esquecerNoHistorico, atividadesAntes = -1;
static const char *perfilHistoryBody = "{\"movies\":[],\"shows\":[],\"anime\":[]}";

char *rede_baixar_st(const char *url, int s, const char *const *cab, int *st) {
  char r[64];
  (void)s;
  conferirCabecalhos(url, cab);
  rota(url, r, sizeof r);
  snprintf(ultimaRota, sizeof ultimaRota, "%s", r);
  *st = 200;
  if (libraryFailure && !strcmp(r, "/sync/all-items/shows/plantowatch")) {
    *st = 400; return NULL;
  }
  if (perfilRede && !strcmp(r, "/users/settings")) {
    pedidosSettings++;
    if (trocarNaResposta == 1) { tokenAtual = "OUTRO-TOKEN-FALSO"; credGeracao++; }
    return strdup("{\"user\":{\"name\":\"Identidade Simkl\"}}");
  }
  // The profile's own month request (no library); the library's pull asks
  // include_all_episodes=yes and goes on below.
  if (perfilRede && !strcmp(r, "/sync/all-items/all/all") &&
      strstr(url, "include_all_episodes=original")) {
    assert(strstr(url, "episode_watched_at=yes"));
    pedidosMesPerfil++;
    if (trocarNaResposta == 2) { tokenAtual = "OUTRO-TOKEN-FALSO"; credGeracao++; }
    *st = perfilHistorySt;
    return perfilHistoryBody ? strdup(perfilHistoryBody) : NULL;
  }
  if (!strcmp(r, "/anime/263883")) {
    batidas[7]++;
    return strdup("{\"title\":\"Anime sem IDs externos\",\"poster\":\"poster-key\","
                  "\"ids\":{\"simkl\":263883,\"imdb\":\"tt2560140\",\"tmdb\":98765}}");
  }
  if (!strcmp(r, "/anime/2638832")) {
    batidas[7]++;
    return strdup("{\"title\":\"Welcome to Demon School! Iruma-kun season 4\","
                  "\"season\":4,\"mapped_tvdb_seasons\":[4],"
                  "\"poster\":\"19/19613958579eba8187\","
                  "\"ids\":{\"simkl\":2638832,\"imdb\":\"tt11034066\","
                  "\"tmdb\":91801,\"tvdb\":369144}}");
  }
  // A per-season entry's own episode list (Kaiju No. 8 Season 2 shape): the
  // episode name, still and TVDB placement of each episode.
  if (!strcmp(r, "/anime/episodes/2638832")) {
    batidas[7]++;
    return strdup("[{\"title\":\"Iruma's Fourth Year\",\"episode\":1,\"type\":\"episode\","
                  "\"img\":\"19/19613958579eba8188\",\"tvdb\":{\"season\":4,\"episode\":1}},"
                  "{\"title\":\"Seventh\",\"episode\":7,\"type\":\"episode\","
                  "\"img\":\"19/19613958579eba8189\",\"tvdb\":{\"season\":4,\"episode\":7}}]");
  }
  // Real shapes (Simkl, 01/11/2026): season 2 has its own entry and only the
  // details carry the IMDb ID and the TVDB season.
  if (!strcmp(r, "/anime/2830474")) {
    detalhesAnime++;
    return strdup("{\"title\":\"Cyberpunk: Edgerunners II\",\"season\":null,"
                  "\"mapped_tvdb_seasons\":[2],\"first_aired\":\"2026-10-20T00:00:00Z\","
                  "\"ids\":{\"simkl\":2830474,\"imdb\":\"tt12590266\",\"tmdb\":\"326788\","
                  "\"tvdb\":\"384541\"}}");
  }
  if (!strcmp(r, "/anime/episodes/2830474")) {
    detalhesAnime++;
    return strdup("[{\"title\":\"Night City Again\",\"episode\":1,\"type\":\"episode\","
                  "\"aired\":true,\"img\":\"17/17570079032e93ce31\","
                  "\"tvdb\":{\"season\":2,\"episode\":1}},"
                  "{\"title\":\"Second\",\"episode\":2,\"type\":\"episode\"}]");
  }
  if (!strcmp(r, "/anime/2830475")) {
    detalhesAnime++;
    return strdup("{\"title\":\"Not aired yet\",\"mapped_tvdb_seasons\":[3],"
                  "\"first_aired\":\"2099-01-01T00:00:00Z\","
                  "\"ids\":{\"simkl\":2830475,\"imdb\":\"tt12590266\"}}");
  }
  // Placeholders as Simkl had them on 10/11/2026: no IMDb ID, season 1 in the
  // TVDB mapping. Spy x Family S4 has aired here, still mapped [1].
  if (!strcmp(r, "/anime/3300001")) {   // a movie: never asked
    detalhesAnime += 100;
    return strdup("{}");
  }
  if (!strcmp(r, "/anime/3300002") || !strcmp(r, "/anime/3300003")) {
    detalhesAnime++;
    return strdup("{\"title\":\"Placeholder\",\"mapped_tvdb_seasons\":[1],"
                  "\"first_aired\":null,\"ids\":{\"simkl\":3300002}}");
  }
  if (!strcmp(r, "/anime/3320816")) {
    detalhesAnime++;
    return strdup("{\"title\":\"Spy x Family\",\"season\":4,\"mapped_tvdb_seasons\":[1],"
                  "\"first_aired\":\"2026-10-04T00:00:00Z\","
                  "\"ids\":{\"simkl\":3320816,\"imdb\":\"tt13706018\"}}");
  }
  if (!strcmp(r, "/anime/38636")) {
    batidas[7]++;
    return strdup("{\"title\":\"One Piece\",\"ids\":{\"simkl\":38636,"
                  "\"imdb\":\"tt0388629\",\"tmdb\":37854}}");
  }
  if (!strcmp(r, "/sync/activities")) {
    batidas[0]++;
    if (atividadesFalham) { *st = 503; return strdup("<html>busy</html>"); }
    if (atividadesInteiras) return strdup(atividadesInteiras);
    return atividadesFalsas();
  }
  if (!strcmp(r, "/sync/playback"))                     { batidas[1]++; return strdup(PLAYBACK); }
  if (!strcmp(r, "/sync/all-items") && strstr(url, "extended=simkl_ids_only")) {
    batidas[11]++;
    if (historicoSoIds) return strdup(historicoSoIds);
  }
  if (!strcmp(r, "/sync/all-items/shows/watching"))     { batidas[2]++; return strdup(WATCHING); }
  if (!strcmp(r, "/sync/all-items/all/all")) {
    assert(strstr(url, "include_all_episodes=yes") &&
           strstr(url, "episode_watched_at=yes") &&
           strstr(url, "episode_tvdb_id=yes") && strstr(url, "limit=500") &&
           strstr(url, "extended=full_anime_seasons") && strstr(url, "language=en"));
    { const char *d = strstr(url, "date_from=");
      if (d) {
        size_t k = strcspn(d + 10, "&");
        batidas[10]++;
        snprintf(ultimoDesde, sizeof ultimoDesde, "%.*s", (int)k, d + 10);
        if (deltaFalha) { *st = 500; return NULL; }
        if (historicoDelta) return strdup(historicoDelta);
      } }
    batidas[9]++;
    atividadesAntes = batidas[0];
    if (esquecerNoHistorico) simkl_esquecer();
    if (libraryFailure) { *st = 400; return NULL; }
    if (historicoCorpo) return strdup(historicoCorpo);
    return bibliotecaFalsa();
  }
  if (!strcmp(r, "/sync/all-items/movies/plantowatch")) { batidas[3]++; return strdup(PTW_FILMES); }
  if (!strcmp(r, "/sync/all-items/shows/plantowatch"))  { batidas[4]++; return strdup(PTW_SERIES); }
  // Anime e um terceiro tipo: /anime/<estado>, raiz "anime".
  if (!strncmp(r, "/sync/all-items/anime/", 22)) {
    batidas[8]++;
    if (!strcmp(r, "/sync/all-items/anime/watching"))    return strdup(WATCHING_ANIME);
    if (!strcmp(r, "/sync/all-items/anime/plantowatch")) return strdup(PTW_ANIME);
    if (!strcmp(r, "/sync/all-items/anime/completed"))   return strdup(FEITO_ANIME);
  }
  batidas[7]++;
  *st = 404;
  return strdup("{\"error\":\"id_err\",\"code\":404}");
}

#include "library639_http.inc"

char *rede_postar_st(const char *url, int s, const char *const *cab,
                     const char *corpo, int *st) {
  char r[64];
  (void)s;
  conferirCabecalhos(url, cab);
  rota(url, r, sizeof r);
  snprintf(ultimaRota, sizeof ultimaRota, "%s", r);
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo ? corpo : "");
  batidas[5]++;
  *st = !strcmp(r, "/sync/add-to-list") ? 201 : 200;
  return strdup("{\"added\":{},\"not_found\":{\"movies\":[],\"shows\":[]}}");
}

char *rede_apagar(const char *url, int s, const char *const *cab, int *st) {
  char r[64];
  (void)s;
  conferirCabecalhos(url, cab);
  rota(url, r, sizeof r);
  snprintf(ultimaRota, sizeof ultimaRota, "%s", r);
  batidas[6]++;
  *st = 204;
  return strdup("");
}

static int totalBatidas(void) {
  int i, t = 0;
  for (i = 0; i < 12; i++) t += batidas[i];
  return t;
}

// As escritas saem num fio; espera o estado sair de PENDENTE (ate 2 s).
static int esperarEscrita(void) {
  int i;
  for (i = 0; i < 200 && simkl_lista_estado() == SMK_OP_PENDENTE; i++) usleep(10000);
  usleep(20000);   // o fio solta a trava logo depois de publicar o estado
  return simkl_lista_estado();
}

// ---------------------------------------------------------------- casos

static void semVinculo(void) {
  CatItem v[12];
  tokenAtual = "";
  assert(!simkl_ativo());
  // A frase honesta sai so quando a opcao e o Simkl.
  assert(simkl_aviso_sem_vinculo(1) && !strcmp(simkl_aviso_sem_vinculo(1), "Vincule o Simkl em Ajustes"));
  assert(simkl_aviso_sem_vinculo(0) == NULL);
  // E nenhum pedido sai sem token.
  assert(simkl_continuar(v, 12) == 0);
  assert(simkl_plantowatch(v, 12) == 0);
  assert(simkl_lista_tipo("tt1375666", "movie", 1) == 0);
  assert(simkl_lista_estado() == SMK_OP_FALHA);
  assert(simkl_playback_remover("tt1375666") == 0);
  assert(totalBatidas() == 0);
  tokenAtual = TOKEN;
  assert(simkl_aviso_sem_vinculo(1) == NULL);
  OK("opcao Simkl sem vinculo: \"Vincule o Simkl em Ajustes\" e zero pedidos");
}

static void leitores(void) {
  CatItem v[12];
  long long ids[12];
  int n = simkl_ler_playback(PLAYBACK, v, ids, 12);
  assert(n == 3);   // anime so-AniDB e item sem imdb ficam fora
  assert(!strcmp(v[0].imdb, "tt0903747:1:5") && !strcmp(v[0].tipo, "series"));
  assert(v[0].temporada == 1 && v[0].episodio == 5 && v[0].progresso == 45);
  assert(!strcmp(v[0].titulo, "Breaking Bad") && !strcmp(v[0].nomeEpisodio, "Episode 5"));
  assert(ids[0] == 123 && v[0].retomadoMs > 0);
  assert(!strcmp(v[1].imdb, "tt1375666") && !strcmp(v[1].tipo, "movie"));
  assert(v[1].progresso == 75 && ids[1] == 124 && !strcmp(v[1].meta, "2010"));
  assert(v[1].retomadoMs > v[0].retomadoMs);   // 11:15 depois de 10:30
  // Anime: numeracao TVDB (2x3), nao a do AniDB (1x40).
  assert(!strcmp(v[2].imdb, "tt2560140:2:3") && ids[2] == 125);
  OK("playback: serie, filme e anime (TVDB) viram itens; AniDB puro e sem imdb ficam fora");
  {
    const char *especiais =
      "[{\"progress\":50,\"episode\":{\"tvdb_season\":0,\"tvdb_number\":1},\"show\":{\"ids\":{\"imdb\":\"tt12409194\"}}},"
      "{\"progress\":50,\"episode\":{\"season\":0,\"number\":2},\"show\":{\"ids\":{\"imdb\":\"tt12409194\"}}},"
      "{\"progress\":50,\"episode\":{\"season\":1,\"number\":77,\"tvdb_season\":0,\"tvdb_number\":3},\"anime\":{\"ids\":{\"imdb\":\"tt12409194\"}}},"
      "{\"progress\":50,\"episode\":{\"season\":1,\"number\":77,\"tvdb_number\":3},\"anime\":{\"ids\":{\"imdb\":\"tt12409194\"}}},"
      "{\"progress\":50,\"episode\":{\"number\":1},\"show\":{\"ids\":{\"imdb\":\"tt12409194\"}}}]";
    n = simkl_ler_playback(especiais, v, ids, 12);
    assert(n == 3);
    for (int i = 0; i < n; i++) {
      char id[64]; snprintf(id, sizeof id, "tt12409194:0:%d", i + 1);
      assert(!strcmp(v[i].imdb, id) && !strcmp(v[i].tipo, "series"));
      assert(v[i].temporada == 0 && v[i].episodio == i + 1);
    }
    assert(simkl_ler_assistindo("{\"shows\":[{\"next_to_watch\":\"S00E06\",\"show\":{\"ids\":{\"imdb\":\"tt12409194\"}}}]}", v, 12) == 1);
    assert(v[0].temporada == 0 && v[0].episodio == 6 && !strcmp(v[0].imdb, "tt12409194:0:6"));
  }
  OK("#556: Simkl preserva S00 explicito, sem inventar mapeamento TVDB ausente");


  n = simkl_ler_assistindo(WATCHING, v, 12);
  assert(n == 2);   // "em dia" (null) e sem imdb ficam fora
  assert(!strcmp(v[0].imdb, "tt0903747:1:6") && v[0].progresso == 0);
  assert(!strcmp(v[1].imdb, "tt11280740:2:3") && v[1].temporada == 2 && v[1].episodio == 3);
  assert(!strcmp(v[1].titulo, "Severance") && v[1].retomadoMs > v[0].retomadoMs);
  // E a resposta do endpoint de ANIME, pelo mesmo leitor (raiz "anime").
  n = simkl_ler_assistindo(WATCHING_ANIME, v, 12);
  assert(n == 2 && !strcmp(v[0].imdb, "tt22248376:3:4") && !strcmp(v[0].tipo, "series"));
  assert(!strcmp(v[0].titulo, "Frieren"));
  assert(simkl_ler_assistindo("null", v, 12) == 0);
  assert(simkl_ler_assistindo("{}", v, 12) == 0);
  OK("watching: next_to_watch \"S02E03\" vira o episodio seguinte com 0%; anime entra");

  n = simkl_ler_plantowatch(PTW_FILMES, 0, v, 12);
  assert(n == 1 && !strcmp(v[0].imdb, "tt1160419") && v[0].naLista && !strcmp(v[0].tipo, "movie"));
  assert(v[0].poster[0]);   // arte do metahub, sem GET
  n = simkl_ler_plantowatch(PTW_SERIES, 1, v, 12);
  assert(n == 2 && !strcmp(v[0].imdb, "tt2788316") && !strcmp(v[0].tipo, "series"));
  n = simkl_ler_plantowatch(PTW_ANIME, 1, v, 12);
  assert(n == 1 && !strcmp(v[0].imdb, "tt22248376") && !strcmp(v[0].tipo, "series"));
  OK("plantowatch: filmes, series e anime com naLista");
}

// An anime dropped as a duplicate of a show: the anime after it kept reading
// the dropped one's Simkl ID (and absolute number), so it took that entry's
// details: Frieren's card titled "Cyberpunk: Edgerunners II".
static void retomadaAnimeRepetido(void) {
  CatItem v[12];
  const char *original = WATCHING_ANIME;
  int n, i, frieren = 0;
  WATCHING_ANIME =
    "{\"anime\":[{\"last_watched_at\":\"2024-01-23T21:00:00Z\",\"status\":\"watching\","
    "\"next_to_watch\":\"S02E03\",\"anime\":{\"title\":\"Severance\","
    "\"ids\":{\"simkl\":2830474,\"imdb\":\"tt11280740\"}}},"
    "{\"last_watched_at\":\"2024-01-22T21:00:00Z\",\"status\":\"watching\","
    "\"next_to_watch\":\"S03E04\",\"anime\":{\"title\":\"Frieren\","
    "\"ids\":{\"imdb\":\"tt22248376\"}}}]}";
  snprintf(atividades, sizeof atividades, "{\"all\":\"2024-03-01T00:00:00Z\"}");
  n = simkl_continuar(v, 12);
  for (i = 0; i < n; i++)
    if (!strcmp(v[i].imdb, "tt22248376:3:4")) {
      assert(!strcmp(v[i].titulo, "Frieren"));
      frieren++;
    }
  assert(frieren == 1);
  WATCHING_ANIME = original;
  snprintf(atividades, sizeof atividades, "{\"all\":\"2024-03-02T00:00:00Z\"}");
  OK("retomada: an anime after a dropped duplicate keeps its own details");
}

static int temId(const CatItem *v, int n, const char *id) {
  for (int i = 0; i < n; i++) if (!strcmp(v[i].imdb, id)) return 1;
  return 0;
}

// Paused rows Simkl no longer means, read from the library (no list of their
// own): on hold, dropped, completed after the pause, or a show watched past
// the paused episode. Breaking Bad is paused at S01E05 on 2024-01-15 10:30,
// Inception on 2024-01-15 11:15.
static void retomadaVencidos(void) {
  CatItem v[12];
  const char *watchingOriginal = WATCHING;
  int n;
  n = simkl_continuar(v, 12);
  assert(n == 6 && temId(v, n, "tt1375666") && temId(v, n, "tt0903747:1:5"));

  HOLD = "{\"movies\":[{\"movie\":{\"title\":\"Inception\",\"ids\":{\"simkl\":67890,"
         "\"imdb\":\"tt1375666\"}}}]}";
  n = simkl_continuar(v, 12);
  assert(n == 5 && !temId(v, n, "tt1375666"));
  HOLD = "{}";
  DROPPED = "{\"shows\":[{\"show\":{\"title\":\"Breaking Bad\",\"ids\":{\"simkl\":12345,"
            "\"imdb\":\"tt0903747\"}}}]}";
  n = simkl_continuar(v, 12);
  assert(!temId(v, n, "tt0903747:1:5") && temId(v, n, "tt1375666"));
  DROPPED = "{}";
  n = simkl_continuar(v, 12);
  assert(temId(v, n, "tt0903747:1:5"));
  OK("pausados: titulo em hold ou dropped sai da retomada, sem lista propria");

  // Watched later up to S01E06, next S01E07: the paused row goes and the next
  // episode takes its place.
  WATCHING =
    "{\"shows\":[{\"last_watched_at\":\"2024-01-16T08:00:00Z\",\"status\":\"watching\","
    "\"next_to_watch\":\"S01E07\",\"show\":{\"title\":\"Breaking Bad\",\"year\":2008,"
    "\"ids\":{\"simkl\":12345,\"imdb\":\"tt0903747\"}}}]}";
  n = simkl_continuar(v, 12);
  assert(!temId(v, n, "tt0903747:1:5") && temId(v, n, "tt0903747:1:7"));
  // Watched after the pause but Simkl's next is still S01E05: the row stays.
  WATCHING =
    "{\"shows\":[{\"last_watched_at\":\"2024-01-16T08:00:00Z\",\"status\":\"watching\","
    "\"next_to_watch\":\"S01E05\",\"show\":{\"title\":\"Breaking Bad\",\"year\":2008,"
    "\"ids\":{\"simkl\":12345,\"imdb\":\"tt0903747\"}}}]}";
  n = simkl_continuar(v, 12);
  assert(temId(v, n, "tt0903747:1:5"));
  OK("pausados: serie vista alem do episodio pausado troca pelo seguinte");

  // Completed after the pause: gone. Completed before it (watched again
  // since): the row stays.
  WATCHING = "{\"shows\":[]}";
  FEITO_SERIES =
    "{\"shows\":[{\"last_watched_at\":\"2024-01-20T08:00:00Z\",\"next_to_watch\":null,"
    "\"show\":{\"title\":\"Breaking Bad\",\"ids\":{\"simkl\":12345,\"imdb\":\"tt0903747\"}}}]}";
  n = simkl_continuar(v, 12);
  assert(!temId(v, n, "tt0903747:1:5"));
  FEITO_SERIES =
    "{\"shows\":[{\"last_watched_at\":\"2024-01-14T08:00:00Z\",\"next_to_watch\":null,"
    "\"show\":{\"title\":\"Breaking Bad\",\"ids\":{\"simkl\":12345,\"imdb\":\"tt0903747\"}}}]}";
  n = simkl_continuar(v, 12);
  assert(temId(v, n, "tt0903747:1:5"));
  FEITO_SERIES = "{}";
  WATCHING = watchingOriginal;
  n = simkl_continuar(v, 12);
  assert(n == 6);
  OK("pausados: concluida depois da pausa sai; concluida antes fica");
}

static void retomada(void) {
  CatItem v[12];
  int n, i;
  memset(batidas, 0, sizeof batidas);
  n = simkl_continuar(v, 12);
  // 3 pausados + Frieren (anime) + Severance (a seguir). Breaking Bad do
  // watching NAO entra de novo: a serie ja esta pausada no S01E05.
  assert(n == 6);
  int frieren = 0, severance = 0, paused = 0;
  for (i = 0; i < n; i++) {
    assert(strncmp(v[i].imdb, "tt0903747:1:6", 13));
    if (i) assert(v[i - 1].retomadoMs >= v[i].retomadoMs);
    frieren += !strcmp(v[i].imdb, "tt22248376:3:4") && !strcmp(v[i].tipo, "series");
    severance += !strcmp(v[i].imdb, "tt11280740:2:3");
    paused += !strcmp(v[i].imdb, "tt1375666");
  }
  assert(frieren == 1 && severance == 1 && paused == 1);
  assert(simkl_e_a_seguir("tt22248376:3:4"));
  assert(simkl_e_a_seguir("tt11280740:2:3"));
  assert(!simkl_e_a_seguir("tt1375666"));
  // Lists from the library (one all-items/all/all), playback asked; no
  // status list is downloaded (shows/watching, anime/...).
  assert(batidas[0] == 1 && batidas[1] == 1 && batidas[9] == 1 &&
         batidas[2] == 0 && batidas[8] == 0);
  assert(!semBearer && !comApiKey);
  OK("retomada: pausados + a seguir de shows E anime, sem repetir serie");

  // Atividades iguais: nada de playback nem biblioteca de novo.
  n = simkl_continuar(v, 12);
  assert(n == 6 && batidas[0] == 2 && batidas[1] == 1 && batidas[9] == 1);
  // Atividade nova: a biblioteca e o playback de novo.
  // The fake answers the date_from delta with its whole library (batidas[9]
  // counts it too), whose items have no Simkl ID and repeat across lists:
  // the merge cannot tell them apart, so the library is downloaded again.
  snprintf(atividades, sizeof atividades, "{\"all\":\"2024-01-21T00:00:00Z\"}");
  n = simkl_continuar(v, 12);
  assert(n == 6 && batidas[0] == 3 && batidas[1] == 2 && batidas[10] == 1 && batidas[9] == 3);
  assert(batidas[2] == 0 && batidas[8] == 0);
  OK("retomada: /sync/activities igual nao baixa a biblioteca de novo");

  // Lista cheia: a sugestao nao expulsa nenhum pausado.
  n = simkl_continuar(v, 3);
  assert(n == 3 && !strcmp(v[0].imdb, "tt1375666"));
  for (i = 0; i < n; i++) assert(strcmp(v[i].imdb, "tt11280740:2:3") && strcmp(v[i].imdb, "tt22248376:3:4"));
  assert(!strcmp(v[2].imdb, "tt2560140:2:3"));
  OK("retomada: com a fileira cheia, todos os pausados ficam");
  proximoTeste = 0;
  n = simkl_continuar(v, 12);
  assert(n == 3 && !simkl_e_a_seguir("tt11280740:2:3"));
  proximoTeste = 1;
  OK("retomada: OFF filtra sugestoes antes da disputa");

  // Tirar da retomada: DELETE /sync/playback/<id do registro>.
  n = simkl_continuar(v, 12);
  memset(batidas, 0, sizeof batidas);
  assert(simkl_playback_remover("tt1375666") == 1);
  for (i = 0; i < 200 && !batidas[6]; i++) usleep(10000);
  assert(batidas[6] == 1 && !strcmp(ultimaRota, "/sync/playback/124"));
  assert(simkl_playback_remover("tt9999999") == 0);   // nao veio do Simkl
  OK("tirar da retomada: DELETE /sync/playback/124");
}

static void maisMenos(void) {
  char corpo[200];
  CatItem v[12];
  // Os corpos, exatamente como a doc do add-to-list e do history/remove.
  assert(simkl_corpo_lista(corpo, sizeof corpo, "tt1375666", "movie", 1));
  assert(!strcmp(corpo, "{\"movies\":[{\"to\":\"plantowatch\",\"ids\":{\"imdb\":\"tt1375666\"}}]}"));
  assert(simkl_corpo_lista(corpo, sizeof corpo, "tt0903747:1:5", "series", 1));
  assert(!strcmp(corpo, "{\"shows\":[{\"to\":\"plantowatch\",\"ids\":{\"imdb\":\"tt0903747\"}}]}"));
  assert(simkl_corpo_lista(corpo, sizeof corpo, "tt0903747", "series", 0));
  assert(!strcmp(corpo, "{\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"}}]}"));
  assert(!simkl_corpo_lista(corpo, sizeof corpo, "kitsu:123", "series", 1));
  assert(!simkl_corpo_lista(corpo, sizeof corpo, "tt12\"}]", "movie", 1));
  assert(!strcmp(simkl_rota_lista(1), "/sync/add-to-list"));
  assert(!strcmp(simkl_rota_lista(0), "/sync/history/remove"));
  OK("+ e -: corpo e rota do Simkl");

  // O "+" de verdade, pelo fio.
  memset(batidas, 0, sizeof batidas);
  assert(simkl_lista_tipo("tt1375666", "movie", 1) == 1);
  assert(esperarEscrita() == SMK_OP_CONFIRMADA);
  assert(batidas[5] == 1 && !strcmp(ultimaRota, "/sync/add-to-list"));
  assert(!strcmp(ultimoCorpo, "{\"movies\":[{\"to\":\"plantowatch\",\"ids\":{\"imdb\":\"tt1375666\"}}]}"));
  assert(simkl_na_plantowatch("tt1375666"));
  // E o "-" do que o "+" acabou de por.
  assert(simkl_lista_tipo("tt1375666", "movie", 0) == 1);
  assert(esperarEscrita() == SMK_OP_CONFIRMADA);
  assert(batidas[5] == 2 && !strcmp(ultimaRota, "/sync/history/remove"));
  assert(!strcmp(ultimoCorpo, "{\"movies\":[{\"ids\":{\"imdb\":\"tt1375666\"}}]}"));
  assert(!simkl_na_plantowatch("tt1375666"));
  // "-" num titulo FORA do Plan to Watch conhecido: nada sai.
  assert(simkl_lista_tipo("tt0903747", "series", 0) == 0);
  assert(batidas[5] == 2);
  OK("+ poe no Plan to Watch; - so tira o que esta la (history/remove apagaria o historico)");

  // O Plan to Watch lido da rede alimenta a guarda do "-".
  memset(batidas, 0, sizeof batidas);
  // Dune + Shogun + Repetido. Frieren (anime) tem o MESMO imdb de Repetido:
  // baixado, mas entra uma vez so.
  assert(simkl_plantowatch(v, 12) == 3);
  // From the library: no movies/shows/anime plantowatch request.
  assert(batidas[3] == 0 && batidas[4] == 0 && batidas[8] == 0);
  assert(v[0].naLista && v[1].naLista && v[2].naLista);
  assert(!strcmp(v[2].imdb, "tt22248376") && !strcmp(v[2].tipo, "series"));
  assert(simkl_na_plantowatch("tt2788316") && simkl_na_plantowatch("tt1160419"));
  assert(simkl_na_plantowatch("tt22248376"));
  assert(simkl_lista_tipo("tt2788316", "series", 0) == 1);
  assert(esperarEscrita() == SMK_OP_CONFIRMADA);
  assert(!strcmp(ultimoCorpo, "{\"shows\":[{\"ids\":{\"imdb\":\"tt2788316\"}}]}"));
  OK("plantowatch da rede: naLista e a guarda do - conhece o que veio de la");
  // No library (download failed, nothing saved): no list, not an empty one.
  bibEsperar();
  simkl_esquecer();
  free(bibTexto); bibTexto = NULL;
  libraryFailure = 1;
  assert(simkl_plantowatch(v, 12) == -1);
  libraryFailure = 0;
  bibEsperar();
  simkl_esquecer();
  OK("partial Simkl library is rejected instead of replacing complete membership");
}

static void perfilRedeCasos(void) {
  PerfilDados d;
  // No library (its download fails): the month is asked from Simkl.
  bibEsperar();
  simkl_esquecer();
  free(bibTexto); bibTexto = NULL;
  libraryFailure = 1;
  perfilRede = 1;
  assert(simkl_perfil(&d) && !strcmp(d.nome, "Identidade Simkl"));
  perfilHistorySt = 500;
  assert(!simkl_perfil(&d));
  perfilHistorySt = 200; perfilHistoryBody = "<html>erro</html>";
  assert(!simkl_perfil(&d));
  perfilHistoryBody = "{}";
  semBearer = 0; trocarNaResposta = 1;
  assert(!simkl_perfil(&d));
  assert(!semBearer);  // os dois pedidos usam a mesma credencial capturada
  tokenAtual = TOKEN; credGeracao++; trocarNaResposta = 2;
  assert(!simkl_perfil(&d));
  tokenAtual = TOKEN; credGeracao++; trocarNaResposta = 0; perfilRede = 0;
  libraryFailure = 0;
  bibEsperar();
  simkl_esquecer();
  OK("perfil: HTTP/HTML falham e relink no meio da consulta nao mistura contas");
}

static const char *ativDelta(const char *all, const char *playback, const char *watching,
                             const char *removidoFilmes);
// With a library: the month's plays come from it (no month request), and the
// name only when Simkl's settings activity moved.
static void perfilBiblioteca(void) {
  PerfilDados d;
  char agora[32], lib[2000], ativ[300];
  time_t t = time(NULL);
  int settingsAntes, mesAntes;
  strftime(agora, sizeof agora, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
  snprintf(lib, sizeof lib,
           "{\"shows\":[{\"status\":\"watching\",\"show\":{\"title\":\"Show B\",\"runtime\":40,"
           "\"ids\":{\"simkl\":2,\"imdb\":\"tt0000002\"}},\"seasons\":[{\"number\":1,\"episodes\":["
           "{\"number\":1,\"watched_at\":\"2020-01-01T00:00:00Z\"},"
           "{\"number\":2,\"watched_at\":\"%s\"},{\"number\":3,\"watched_at\":\"%s\"}]}]}],"
           "\"anime\":[{\"status\":\"watching\",\"mapped_tvdb_seasons\":[2],"
           "\"show\":{\"title\":\"Kaiju S2\",\"ids\":{\"simkl\":6,\"imdb\":\"tt0000006\"}},"
           "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":8,\"watched_at\":\"%s\","
           "\"tvdb\":{\"season\":2,\"episode\":8}}]}]},"
           "{\"status\":\"watching\",\"mapped_tvdb_seasons\":[1,2,22],"
           "\"show\":{\"title\":\"Long Anime\",\"ids\":{\"simkl\":7,\"imdb\":\"tt0000007\"}},"
           "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":1122,\"watched_at\":\"%s\","
           "\"tvdb\":{\"season\":22,\"episode\":37}}]}]}],"
           "\"movies\":[{\"status\":\"completed\",\"last_watched_at\":\"%s\","
           "\"movie\":{\"title\":\"Movie A\",\"runtime\":100,\"ids\":{\"simkl\":3,\"imdb\":\"tt0000003\"}}}]}",
           agora, agora, agora, agora, agora);
  bibEsperar();
  simkl_esquecer();
  free(bibTexto); bibTexto = NULL;
  memset(batidas, 0, sizeof batidas);
  historicoCorpo = lib;
  snprintf(ativ, sizeof ativ, "%s", ativDelta("2026-11-01T00:00:00Z", "2026-11-01T00:00:00Z",
                                              "2026-11-01T00:00:00Z", NULL));
  atividadesInteiras = ativ;
  perfilRede = 1;
  settingsAntes = pedidosSettings; mesAntes = pedidosMesPerfil;
  assert(simkl_perfil(&d) && !strcmp(d.nome, "Identidade Simkl"));
  // Two show episodes this month (not the 2020 one), two anime episodes and
  // the movie; no month request.
  assert(d.episodios == 4 && d.filmes == 1 && d.plays == 5 && d.minutos == 180);
  // Anime labels: a per-season entry by its TVDB placement, one numbered
  // across the show by Simkl's number (not "T1E8" / "T1E1122").
  { int kaiju = 0, longo = 0;
    for (int i = 0; i < d.nDestaques; i++) {
      if (!strcmp(d.destaques[i].id, "tt0000006")) kaiju = !strcmp(d.destaques[i].detalhe, "T2E8");
      if (!strcmp(d.destaques[i].id, "tt0000007")) longo = !strcmp(d.destaques[i].detalhe, "E1122");
    }
    assert(kaiju && longo); }
  assert(pedidosMesPerfil == mesAntes && pedidosSettings == settingsAntes + 1);
  // Opened again, settings activity the same: the name is kept.
  assert(simkl_perfil(&d) && !strcmp(d.nome, "Identidade Simkl"));
  assert(pedidosSettings == settingsAntes + 1 && pedidosMesPerfil == mesAntes);
  // The settings moved (name or avatar changed on Simkl; "all" moves with
  // them, as on Simkl): asked again.
  snprintf(ativ, sizeof ativ, "%s", ativDelta("2026-11-02T00:00:00Z", "2026-11-01T00:00:00Z",
                                              "2026-11-01T00:00:00Z", NULL));
  { char *m = strstr(ativ, "\"settings\":{\"all\":\"2026-01-01");
    assert(m);
    memcpy(m + 19, "2027", 4); }
  assert(simkl_perfil(&d) && pedidosSettings == settingsAntes + 2);
  assert(pedidosMesPerfil == mesAntes);
  perfilRede = 0;
  bibEsperar();
  historicoCorpo = atividadesInteiras = NULL;
  simkl_esquecer();
  free(bibTexto); bibTexto = NULL;
  OK("perfil: the month from the library, the name only when its settings moved");
}

static void historicoEpisodios(void) {
  bibEsperar();
  simkl_esquecer();
  free(bibTexto); bibTexto = NULL;   // no saved copy: the whole pull
  tokenAtual = TOKEN;
  historicoTmdb = 37854;
  nHistorico = 0; mapaEsvaziado++;
  batidas[9] = 0;
  assert(simkl_historico_aplicar("tt0388629"));
  assert(batidas[9] == 1 && nHistorico == 4);
  assert(!strcmp(historicoIds[0], "tt0903747"));
  assert(historicoPares[0].temporada == 1 && historicoPares[0].episodio == 1);
  assert(!strcmp(historicoIds[1], "tt0388629"));
  assert(historicoPares[1].temporada == 22 && historicoPares[1].episodio == 1122);
  assert(historicoPares[2].temporada == 22 && historicoPares[2].episodio == 1123);
  assert(!strcmp(historicoIds[3], "tt0388629"));
  assert(historicoPares[3].temporada == 22 && historicoPares[3].episodio == 36);
  assert(historicoMs[0] == js_ms_iso("2026-10-10T12:00:00Z"));
  // One Piece's marks were written while it was held, and the hold is gone.
  assert(marcasSeguradas == 3 && segurosOnePiece == 0 && segurosOutros == 0);
  assert(simkl_historico_aplicar("tt0903747"));
  assert(batidas[9] == 1);
  assert(segurosOnePiece == 0 && segurosOutros == 0);   // a regular show: no hold
  OK("history: all-item watched_at imported across shows and anime, cached");
}

static void bibEsperar(void) {
  for (int i = 0; i < 2000 && simkl_biblioteca_carregando(); i++) usleep(1000);
  assert(!simkl_biblioteca_carregando());
}
static int marcouSerie(const char *id) {
  for (int i = 0; i < nHistorico; i++) if (!strcmp(historicoIds[i], id)) return 1;
  return 0;
}

// The whole pull saves the library: activities asked first, the pages as one
// all-items body, signed by the link. Read back, it imports the same marks.
static void historicoBiblioteca(void) {
  SimklBib b = {0};
  int antes;
  bibEsperar();
  simkl_esquecer();
  tokenAtual = TOKEN;
  free(bibTexto); bibTexto = NULL; bibGravacoes = 0;
  antes = batidas[0]; nHistorico = 0; mapaEsvaziado++;
  assert(simkl_historico_aplicar("tt0388629"));
  assert(atividadesAntes == antes + 1);
  assert(bibGravacoes == 1 && !strcmp(bibNome, "simkl-biblioteca-p2.txt"));
  assert(!strstr(bibTexto, TOKEN));
  assert(!simklbib_ler(2, simklbib_conta("OUTRO-TOKEN"), &b));
  assert(!bibTexto);   // another link's copy is deleted
  OK("history: the whole pull saves the library for the profile, never the token");

  bibEsperar();
  simkl_esquecer();
  nHistorico = 0; mapaEsvaziado++;
  assert(simkl_historico_aplicar("tt0388629") && bibGravacoes == 2);
  assert(simklbib_ler(2, simklbib_conta(TOKEN), &b));
  // Through the file text: tests/i610.c renames `atividades` (the fixture).
  { char all[64] = "";
    assert(js_texto_raiz(atividades, "all", all, sizeof all) && strstr(bibTexto, all)); }
  { static const char ini[] = "{\"shows\":[{";
    assert(!strncmp(b.biblioteca, ini, sizeof ini - 1)); }
  assert(strstr(b.biblioteca, "\"anime\":[{") && strstr(b.biblioteca, "\"movies\":[{"));
  nHistorico = 0; mapaEsvaziado++;
  // 3, not 4: the TMDB-only One Piece entry imports only for the show being
  // opened (simkl_historico_aplicar's fallback), not in a plain read.
  { int n = simkl_historico_ler(b.biblioteca, NULL, 1);
    assert(n == 3 && nHistorico == 3); }
  assert(!strcmp(historicoIds[1], "tt0388629") && historicoPares[1].episodio == 1122);
  simklbib_liberar(&b);
  OK("history: the saved library reads back and imports the same episodes");

  bibEsperar();
  simkl_esquecer();
  atividadesFalham = 1; nHistorico = 0; mapaEsvaziado++;
  assert(simkl_historico_aplicar("tt0388629"));
  assert(nHistorico == 4 && bibGravacoes == 2);
  atividadesFalham = 0;
  OK("history: activities failing still imports, saves nothing");

  bibEsperar();
  simkl_esquecer();
  esquecerNoHistorico = 1; nHistorico = 0; mapaEsvaziado++;
  simkl_historico_aplicar("tt0388629");
  assert(bibGravacoes == 2);
  esquecerNoHistorico = 0;
  OK("history: Simkl forgotten during the pull saves nothing");
}

// Step 1c: the library loads in the background when the profile opens; the
// show page reads it from memory.
static void bibliotecaNaAbertura(void) {
  int ativ, puxadas, gravadas;
  bibEsperar();
  simkl_esquecer();
  tokenAtual = TOKEN;
  historicoTmdb = 37854;
  free(bibTexto); bibTexto = NULL; bibGravacoes = 0;

  // No saved copy: activities, the whole library, saved; shows imported, no
  // anime (mapping one needs its show's episode list).
  nHistorico = 0; mapaEsvaziado++; ativ = batidas[0]; puxadas = batidas[9];
  simkl_biblioteca_iniciar();
  bibEsperar();
  assert(batidas[0] == ativ + 1 && batidas[9] == puxadas + 1 && bibGravacoes == 1);
  assert(nHistorico == 1 && marcouSerie("tt0903747") && !marcouSerie("tt0388629"));
  OK("open: no saved copy downloads and saves the library, imports shows not anime");

  // The show page maps its anime from memory, TMDB-only entry included, with
  // no request.
  ativ = batidas[0]; puxadas = batidas[9];
  assert(simkl_historico_aplicar("tt0388629"));
  assert(batidas[0] == ativ && batidas[9] == puxadas);
  assert(nHistorico == 4 && marcouSerie("tt0388629"));
  assert(historicoPares[3].temporada == 22 && historicoPares[3].episodio == 36);
  OK("show page: its anime mapped from memory, no request");

  // Next launch, nothing changed on Simkl: the saved copy, one activities
  // request, no download, no save.
  bibEsperar();
  simkl_esquecer();
  nHistorico = 0; mapaEsvaziado++; ativ = batidas[0]; puxadas = batidas[9]; gravadas = bibGravacoes;
  simkl_biblioteca_iniciar();
  bibEsperar();
  assert(batidas[0] == ativ + 1 && batidas[9] == puxadas && bibGravacoes == gravadas);
  assert(nHistorico == 1 && marcouSerie("tt0903747"));
  OK("open: activities unchanged uses the saved copy, downloads nothing");

  // Something changed: download again and save. The fake answers the delta
  // with its whole library, items without Simkl IDs repeated across lists,
  // which the merge cannot place: a delta, then the whole library.
  bibEsperar();
  simkl_esquecer();
  snprintf(atividades, sizeof atividades, "{\"all\":\"2024-01-22T00:00:00Z\"}");
  ativ = batidas[0]; puxadas = batidas[9];
  int deltasAntes = batidas[10];
  simkl_biblioteca_iniciar();
  bibEsperar();
  assert(batidas[0] == ativ + 1 && batidas[10] == deltasAntes + 1 &&
         batidas[9] == puxadas + 2 && bibGravacoes == gravadas + 1);
  assert(strstr(bibTexto, "2024-01-22T00:00:00Z"));
  OK("open: activities moved downloads and saves again");

  // The show page waits for a load already on its way instead of starting one.
  bibEsperar();
  simkl_esquecer();
  nHistorico = 0; mapaEsvaziado++; puxadas = batidas[9];
  simkl_biblioteca_iniciar();
  assert(simkl_historico_aplicar("tt0388629"));
  bibEsperar();
  assert(batidas[9] == puxadas && marcouSerie("tt0388629"));
  OK("show page: waits for the load in progress, no second one");

  // No link: nothing starts.
  bibEsperar();
  simkl_esquecer();
  tokenAtual = "";
  ativ = batidas[0];
  simkl_biblioteca_iniciar();
  assert(!simkl_biblioteca_carregando() && batidas[0] == ativ);
  assert(!simkl_historico_aplicar("tt0388629"));
  tokenAtual = TOKEN;
  OK("open: no Simkl link starts nothing");
}

// STEP 2: the library brought up to date by what moved in the activities.
// Every item has a Simkl ID, as on Simkl.
static const char *DELTA_L0 =
  "{\"shows\":[{\"status\":\"watching\",\"next_to_watch\":\"S01E03\","
  "\"show\":{\"title\":\"Show B\",\"ids\":{\"simkl\":2,\"imdb\":\"tt0000002\"}},"
  "\"seasons\":[{\"number\":1,\"episodes\":["
  "{\"number\":1,\"watched_at\":\"2026-10-01T00:00:00Z\"},"
  "{\"number\":2,\"watched_at\":\"2026-10-02T00:00:00Z\"}]}]}],"
  "\"anime\":[],"
  "\"movies\":[{\"status\":\"plantowatch\",\"movie\":{\"title\":\"Movie A\","
  "\"ids\":{\"simkl\":3,\"imdb\":\"tt0000003\"}}}]}";
// Show B finished (episode 3) and Show C started.
static const char *DELTA_D1 =
  "{\"shows\":[{\"status\":\"completed\",\"next_to_watch\":null,"
  "\"show\":{\"title\":\"Show B\",\"ids\":{\"simkl\":2,\"imdb\":\"tt0000002\"}},"
  "\"seasons\":[{\"number\":1,\"episodes\":["
  "{\"number\":1,\"watched_at\":\"2026-10-01T00:00:00Z\"},"
  "{\"number\":2,\"watched_at\":\"2026-10-02T00:00:00Z\"},"
  "{\"number\":3,\"watched_at\":\"2026-11-02T00:00:00Z\"}]}]},"
  "{\"status\":\"watching\",\"next_to_watch\":\"S01E02\","
  "\"show\":{\"title\":\"Show C\",\"ids\":{\"simkl\":4,\"imdb\":\"tt0000004\"}}}]}";
static char deltaAtiv[600];
static const char *ativDelta(const char *all, const char *playback, const char *watching,
                             const char *removidoFilmes) {
  snprintf(deltaAtiv, sizeof deltaAtiv,
           "{\"all\":\"%s\",\"settings\":{\"all\":\"2026-01-01T00:00:00Z\"},"
           "\"tv_shows\":{\"all\":\"%s\",\"rated_at\":null,\"playback\":\"%s\","
           "\"plantowatch\":null,\"watching\":\"%s\",\"completed\":\"%s\",\"hold\":null,"
           "\"dropped\":null,\"removed_from_list\":null},"
           "\"anime\":{\"all\":null,\"playback\":null,\"watching\":null,\"removed_from_list\":null},"
           "\"movies\":{\"all\":\"%s\",\"playback\":null,\"plantowatch\":\"2026-10-01T00:00:00Z\","
           "\"completed\":null,\"removed_from_list\":%s%s%s}}",
           all, all, playback, watching, watching, all,
           removidoFilmes ? "\"" : "", removidoFilmes ? removidoFilmes : "null",
           removidoFilmes ? "\"" : "");
  return deltaAtiv;
}
static int contar(const char *s, const char *t) {
  int n = 0;
  for (const char *p = s; p && (p = strstr(p, t)); p += strlen(t)) n++;
  return n;
}
// A list read: with SMK_BIB_FRESCA 0 here, each one checks with Simkl.
static char *listaDelta(const char *tipo, const char *estado) {
  char *r = simkl_lista_corpo(tipo, estado);
  bibEsperar();
  return r;
}

static void bibliotecaDelta(void) {
  char *r;
  int puxadas, deltas, grav;
  bibEsperar();
  simkl_esquecer();
  tokenAtual = TOKEN;
  free(bibTexto); bibTexto = NULL;
  memset(batidas, 0, sizeof batidas);
  historicoCorpo = DELTA_L0;
  atividadesInteiras = ativDelta("2026-11-01T00:00:00Z", "2026-11-01T00:00:00Z",
                                 "2026-11-01T00:00:00Z", NULL);
  simkl_biblioteca_iniciar();
  bibEsperar();
  assert(batidas[9] == 1 && batidas[10] == 0 && bibGravacoes > 0);
  grav = bibGravacoes;

  // Only playback moved: nothing to download, nothing saved. Not on the next
  // launch either (the saved copy's buckets are the same).
  atividadesInteiras = ativDelta("2026-11-01T01:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-01T00:00:00Z", NULL);
  r = listaDelta("shows", "watching");
  assert(r && strstr(r, "Show B"));
  free(r);
  assert(batidas[9] == 1 && batidas[10] == 0 && batidas[11] == 0 && bibGravacoes == grav);
  simkl_esquecer();
  simkl_biblioteca_iniciar();
  bibEsperar();
  assert(batidas[9] == 1 && batidas[10] == 0 && batidas[11] == 0 && bibGravacoes == grav);
  OK("delta: only playback moved downloads nothing, at once or on the next launch");

  // A status bucket moved: one date_from request from the last activities;
  // Show B replaced (completed, 3 episodes), Show C added, saved.
  historicoDelta = DELTA_D1;
  atividadesInteiras = ativDelta("2026-11-02T00:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-02T00:00:00Z", NULL);
  nHistorico = 0; mapaEsvaziado++;
  r = listaDelta("shows", "watching");
  assert(batidas[9] == 1 && batidas[10] == 1 && batidas[11] == 0);
  // From the activities last confirmed: the playback-only check moved them on.
  assert(!strcmp(ultimoDesde, "2026-11-01T01:00:00Z"));
  assert(r && strstr(r, "Show C") && !strstr(r, "Show B"));
  free(r);
  r = listaDelta("shows", "completed");
  assert(r && contar(r, "Show B") == 1);
  free(r);
  assert(bibGravacoes == grav + 1 && strstr(bibTexto, "Show C") &&
         strstr(bibTexto, "2026-11-02T00:00:00Z") && contar(bibTexto, "\"simkl\":2,") == 1);
  assert(marcou("tt0000002", 1, 3));
  OK("delta: a moved bucket asks date_from once, replaces by Simkl ID, adds, saves");

  // Only removed_from_list moved: the IDs-only list, no delta; Movie A goes.
  historicoSoIds = "{\"shows\":[{\"show\":{\"ids\":{\"simkl\":2}}},"
                 "{\"show\":{\"ids\":{\"simkl\":4}}}],\"anime\":[],\"movies\":[]}";
  atividadesInteiras = ativDelta("2026-11-03T00:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-02T00:00:00Z", "2026-11-03T00:00:00Z");
  r = listaDelta("movies", "plantowatch");
  assert(batidas[10] == 1 && batidas[11] == 1);
  assert(r && !strstr(r, "Movie A"));
  free(r);
  assert(bibGravacoes == grav + 2 && !strstr(bibTexto, "Movie A") && strstr(bibTexto, "Show C"));
  OK("delta: removed_from_list asks the IDs-only list and drops what is gone");

  // The delta fails: the library stays, nothing saved, the next read retries.
  atividadesInteiras = ativDelta("2026-11-04T00:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-04T00:00:00Z", "2026-11-03T00:00:00Z");
  deltaFalha = 1;
  deltas = batidas[10];
  r = listaDelta("shows", "watching");
  assert(r && strstr(r, "Show C"));
  free(r);
  assert(batidas[10] == deltas + 1 && bibGravacoes == grav + 2);
  deltaFalha = 0;
  r = listaDelta("shows", "watching");
  free(r);
  assert(batidas[10] == deltas + 2 && bibGravacoes == grav + 3);
  assert(!strcmp(ultimoDesde, "2026-11-03T00:00:00Z"));
  OK("delta: a failed delta keeps the library and asks again on the next read");

  // Items without a Simkl ID (Simkl always sends one) still go by delta. A new
  // one is added; the two Kaiju seasons share an IMDb ID, not a MAL ID.
  historicoDelta = "{\"shows\":[{\"status\":\"watching\",\"show\":{\"title\":\"No ID\","
                   "\"ids\":{\"imdb\":\"tt0000009\"}}}],"
                   "\"anime\":[{\"status\":\"watching\",\"show\":{\"title\":\"Kaiju S1\","
                   "\"ids\":{\"simkl\":5,\"imdb\":\"tt0000008\",\"mal\":\"1\"}}},"
                   "{\"status\":\"watching\",\"show\":{\"title\":\"Kaiju S2\","
                   "\"ids\":{\"simkl\":6,\"imdb\":\"tt0000008\",\"mal\":\"2\"}}}]}";
  atividadesInteiras = ativDelta("2026-11-05T00:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-05T00:00:00Z", "2026-11-03T00:00:00Z");
  puxadas = batidas[9]; deltas = batidas[10];
  r = listaDelta("shows", "watching");
  free(r);
  assert(batidas[10] == deltas + 1 && batidas[9] == puxadas);
  assert(contar(bibTexto, "No ID") == 1 && strstr(bibTexto, "Kaiju S2"));
  // Without a Simkl ID, its IMDb and MAL IDs pick season 2 alone.
  historicoDelta = "{\"anime\":[{\"status\":\"completed\",\"show\":{\"title\":\"Kaiju S2 done\","
                   "\"ids\":{\"imdb\":\"tt0000008\",\"mal\":2}}}]}";
  atividadesInteiras = ativDelta("2026-11-06T00:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-06T00:00:00Z", "2026-11-03T00:00:00Z");
  r = listaDelta("shows", "watching");
  free(r);
  assert(batidas[10] == deltas + 2 && batidas[9] == puxadas);
  assert(strstr(bibTexto, "Kaiju S1\"") && strstr(bibTexto, "Kaiju S2 done") &&
         !strstr(bibTexto, "Kaiju S2\""));
  OK("delta: items without a Simkl ID are added or matched by their other IDs");

  // Only the IMDb ID: both seasons fit, so the whole library instead.
  historicoDelta = "{\"anime\":[{\"status\":\"completed\",\"show\":{\"title\":\"Kaiju ?\","
                   "\"ids\":{\"imdb\":\"tt0000008\"}}}]}";
  atividadesInteiras = ativDelta("2026-11-07T00:00:00Z", "2026-11-01T01:00:00Z",
                                 "2026-11-07T00:00:00Z", "2026-11-03T00:00:00Z");
  r = listaDelta("shows", "watching");
  free(r);
  assert(batidas[10] == deltas + 3 && batidas[9] == puxadas + 1);
  assert(!strstr(bibTexto, "Kaiju") && strstr(bibTexto, "Movie A"));   // DELTA_L0 again
  OK("delta: an item without a Simkl ID that fits several downloads the whole library");

  // Continue watching asks /sync/playback only when the playback timestamps
  // move: watching an episode or changing a list moves "all", not them.
  { CatItem v[12];
    int play;
    historicoDelta = "{\"shows\":[]}";
    atividadesInteiras = ativDelta("2026-11-08T00:00:00Z", "2026-11-08T00:00:00Z",
                                   "2026-11-07T00:00:00Z", "2026-11-03T00:00:00Z");
    play = batidas[1];
    simkl_continuar(v, 12);
    bibEsperar();
    assert(batidas[1] == play + 1);
    atividadesInteiras = ativDelta("2026-11-09T00:00:00Z", "2026-11-08T00:00:00Z",
                                   "2026-11-09T00:00:00Z", "2026-11-03T00:00:00Z");
    simkl_continuar(v, 12);
    bibEsperar();
    assert(batidas[1] == play + 1);
    atividadesInteiras = ativDelta("2026-11-10T00:00:00Z", "2026-11-10T00:00:00Z",
                                   "2026-11-09T00:00:00Z", "2026-11-03T00:00:00Z");
    simkl_continuar(v, 12);
    bibEsperar();
    assert(batidas[1] == play + 2); }
  OK("playback: asked again only when its own timestamps move");

  bibEsperar();
  historicoCorpo = historicoDelta = historicoSoIds = atividadesInteiras = NULL;
  simkl_esquecer();
  free(bibTexto); bibTexto = NULL;
}

// A TVDB-numbered list: S1E1-12, S2E1-11 (+ S3E1-6 with `s3`).
static void kaijuLista(int s3) {
  historicoCatalogEpsQtd = 0;
  for (int t = 1; t <= (s3 ? 3 : 2); t++)
    for (int e = 1; e <= (t == 1 ? 12 : t == 2 ? 11 : 6); e++) {
      CatEp *ep = &historicoCatalogEps[historicoCatalogEpsQtd++];
      memset(ep, 0, sizeof *ep);
      ep->temporada = t;
      ep->episodio = e;
    }
  historicoCatalogEpsFinal = 1;
}
static int marcou(const char *id, int t, int e) {
  int r = 0;
  pthread_mutex_lock(&historicoTrava);
  for (int i = 0; i < nHistorico && !r; i++)
    r = !strcmp(historicoIds[i], id) && historicoPares[i].temporada == t &&
        historicoPares[i].episodio == e;
  pthread_mutex_unlock(&historicoTrava);
  return r;
}

static void kaijuListaDeVolta(void) { kaijuLista(1); }

// The log nuvio-rewrite1 (2026-11-08): Bleach 366 -> 0 and Kaiju No. 8 20 -> 8
// when the add-on replaced the episode list. Every Simkl entry of the show is
// mapped again, from the library in memory.
static void animeVariasEntradas(void) {
  int t = 0, e = 0;
  bibEsperar();
  simkl_esquecer();
  tokenAtual = TOKEN;
  free(bibTexto); bibTexto = NULL;
  historicoCorpo = KAIJU_HISTORY;
  snprintf(historicoCatalogId, sizeof historicoCatalogId, "tt21975436");
  kaijuLista(0);
  nHistorico = 0; mapaEsvaziado++;
  { int antes = segurosPedidos;
    assert(simkl_historico_aplicar("tt21975436"));
    assert(segurosPedidos == antes + 1);   // first open: held once, for both entries
  }
  assert(nHistorico == 4);
  assert(marcou("tt21975436", 1, 1) && marcou("tt21975436", 1, 2));
  assert(marcou("tt21975436", 2, 1) && marcou("tt21975436", 2, 2));
  OK("anime: two Simkl entries of one show map by Simkl's TVDB numbers");

  assert(simkl_anime_proximo("tt21975436", &t, &e) && t == 2 && e == 3);
  OK("anime: next-up is the furthest of the show's entries (S2E3, not S1E3)");

  // Opened again in the same session with nothing changed: the marks are in
  // the map, so nothing is mapped and the page's count is not held.
  nHistorico = 0;
  { int antes = segurosPedidos;
    assert(simkl_historico_aplicar("tt21975436"));
    // Never held, not even while the entries were checked: "Your progress"
    // blinked on every reopen of Bleach when the hold came first.
    assert(segurosPedidos == antes);
  }
  assert(nHistorico == 0 && segurosOutros == 0);
  // Its Simkl entry changed (an episode watched elsewhere): mapped again.
  { char *mudado = strdup(KAIJU_HISTORY), *w = strstr(mudado, "watched_at\":\"2");
    assert(w);
    w[strlen("watched_at\":\"2")] = '1';   // another year
    { int antes = segurosPedidos;
      assert(simkl_historico_ler(mudado, "tt21975436", 1) > 0 && nHistorico > 0);
      assert(segurosPedidos == antes + 1 && segurosOutros == 0);   // mapped: held, then shown
    }
    nHistorico = 0;
    assert(simkl_historico_ler(mudado, "tt21975436", 1) == 0 && nHistorico == 0);
    free(mudado); }
  // An emptied map (the profile opened again): mapped again.
  mapaEsvaziado++;
  assert(simkl_historico_ler(KAIJU_HISTORY, "tt21975436", 1) > 0 && nHistorico == 4);
  OK("anime: opened again unchanged maps nothing and never hides the count; a changed entry or emptied map maps");

  // Opened again before the page's episode list is back in memory (Bleach in
  // nuvio-rewrite4): nothing mapped, nothing held. The same list arriving
  // maps nothing; a different one maps every entry (just below).
  mapaEsvaziado++;
  nHistorico = 0;
  assert(simkl_historico_aplicar("tt21975436") && nHistorico == 4);   // onto this list
  historicoCatalogEpsQtd = historicoCatalogEpsFinal = 0;
  nHistorico = 0;
  assert(simkl_historico_aplicar("tt21975436"));
  assert(nHistorico == 0 && segurosOutros == 0);
  kaijuLista(0);
  simkl_anime_lista_mudou("tt21975436");
  usleep(50000);
  assert(historicoQtd() == 0 && segurosOutros == 0);
  OK("anime: opened again with no list loaded maps nothing; the same list back maps nothing");

  // The add-on's list: same numbering, one more season. Both entries again.
  kaijuLista(1);
  pthread_mutex_lock(&historicoTrava); nHistorico = 0; pthread_mutex_unlock(&historicoTrava);
  simkl_anime_lista_mudou("tt21975436");
  // Both entries' marks, then the next-up of each entry on the new list.
  for (int i = 0; i < 2000 && historicoQtd() < 4; i++) usleep(1000);
  assert(historicoQtd() == 4);
  assert(marcou("tt21975436", 1, 1) && marcou("tt21975436", 2, 2));
  t = e = 0;
  for (int i = 0; i < 2000 && !simkl_anime_proximo("tt21975436", &t, &e); i++) usleep(1000);
  assert(t == 2 && e == 3);
  // The same list published again maps nothing.
  usleep(50000);
  pthread_mutex_lock(&historicoTrava); nHistorico = 0; pthread_mutex_unlock(&historicoTrava);
  simkl_anime_lista_mudou("tt21975436");
  usleep(50000);
  assert(historicoQtd() == 0);
  OK("anime: a new episode list maps every entry of the show again, once");

  // Another list is published and claimed, but by the time the remap thread
  // looks, the list it had mapped onto is back. The claim is not a finished
  // mapping: skipped as "unchanged", the show kept no next-up, and the claimed
  // list published again was taken as already mapped.
  kaijuLista(0);
  pthread_mutex_lock(&historicoTrava); nHistorico = 0; pthread_mutex_unlock(&historicoTrava);
  aoCopiarPorId = kaijuListaDeVolta;
  simkl_anime_lista_mudou("tt21975436");
  for (int i = 0; i < 2000 && historicoQtd() < 4; i++) usleep(1000);
  assert(historicoQtd() == 4);
  t = e = 0;
  for (int i = 0; i < 2000 && !simkl_anime_proximo("tt21975436", &t, &e); i++) usleep(1000);
  assert(t == 2 && e == 3);
  OK("anime: a list claimed but not mapped is mapped, even with the old list back");

  bibEsperar();
  historicoCorpo = NULL;
  historicoCatalogId[0] = 0;
  historicoCatalogEpsQtd = historicoCatalogEpsFinal = 0;
}

static void perfilFalhas(void) {
  PerfilDados d;
  time_t agora = (time_t)(js_ms_iso("2026-10-12T12:00:00Z") / 1000);
  assert(!simkl_perfil_ler("{\"user\":{\"name\":\"Pessoa\"}}", NULL, &d, agora));
  assert(!simkl_perfil_ler(NULL, "<html>erro</html>", &d, agora));
  assert(!simkl_perfil_ler(NULL, "{\"error\":\"unauthorized\"}", &d, agora));
  assert(!simkl_perfil_ler(NULL, "{\"movies\":[", &d, agora));
  assert(!simkl_perfil_ler(NULL, "{\"movies\":null}", &d, agora));
  assert(simkl_perfil_ler(NULL, "{\"movies\":[],\"shows\":[],\"anime\":[]}", &d, agora));
  assert(d.plays == 0 && d.nDestaques == 0);
  assert(simkl_perfil_ler(NULL, "{}", &d, agora));
  OK("perfil: falha de historico nao vira zero reproducoes; vazio confirmado e aceito");
}

static void perfilAnimeRanking(void) {
  const char *itens =
    "{\"movies\":["
    "{\"last_watched_at\":\"2026-10-10T12:00:00Z\",\"movie\":{\"ids\":{\"imdb\":\"tt1\"}}},"
    "{\"last_watched_at\":\"2026-10-10T12:00:00Z\",\"movie\":{\"ids\":{\"imdb\":\"tt2\"}}},"
    "{\"last_watched_at\":\"2026-10-10T12:00:00Z\",\"movie\":{\"ids\":{\"imdb\":\"tt3\"}}},"
    "{\"last_watched_at\":\"2026-10-10T12:00:00Z\",\"movie\":{\"ids\":{\"imdb\":\"tt4\"}}}],"
    "\"anime\":[{\"anime\":{\"title\":\"Anime\",\"runtime\":24,\"ids\":{\"imdb\":\"tt5\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":1,\"watched_at\":\"2026-10-11T12:00:00Z\"},"
    "{\"number\":2,\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  PerfilDados d;
  time_t agora = (time_t)(js_ms_iso("2026-10-12T15:00:00Z") / 1000);
  assert(simkl_perfil_ler(NULL, itens, &d, agora));
  assert(d.plays == 6 && d.filmes == 4 && d.episodios == 2 && d.minutos == 48);
  assert(d.nDestaques == 4 && !strcmp(d.destaques[0].id, "tt5"));
  assert(d.destaques[0].plays == 2 && d.destaques[0].minutos == 48);
  assert(d.streakAtual == 3 && d.diasAtivosMes == 3 && d.atividade[11] == 1);
  assert(d.nGeneros == 0 && d.parcial && !d.anoCompleto && !d.streakCompleto);
  OK("perfil: anime conta e o quinto titulo mais visto entra nos quatro destaques");

  // Simkl's real anime shape: each item under "show", not "anime".
  { const char *comShow =
      "{\"movies\":[],\"shows\":[],"
      "\"anime\":[{\"show\":{\"title\":\"Frieren\",\"runtime\":24,\"ids\":{\"imdb\":\"tt22248376\"}},"
      "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":1,\"watched_at\":\"2026-10-11T12:00:00Z\"},"
      "{\"number\":2,\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
    assert(simkl_perfil_ler(NULL, comShow, &d, agora));
    assert(d.plays == 2 && d.episodios == 2 && d.minutos == 48 && d.diasAtivosMes == 2);
    assert(d.nDestaques == 1 && !strcmp(d.destaques[0].id, "tt22248376"));
    CatItem v[4];
    const char *ptw =
      "{\"anime\":[{\"status\":\"plantowatch\",\"show\":{\"title\":\"Frieren\","
      "\"ids\":{\"simkl\":77,\"imdb\":\"tt22248376\"}}}]}";
    assert(simkl_ler_plantowatch(ptw, 1, v, 4) == 1 && !strcmp(v[0].imdb, "tt22248376"));
    assert(simkl_ler_plantowatch(ptw, 0, v, 4) == 0); }
  OK("perfil e plan to watch: anime sob \"show\" (forma real do Simkl) conta");
}

static void perfilStats(void) {
  static const char *settings =
    "{\"user\":{\"name\":\"Tolu no Simkl\",\"avatar\":\"https://simkl.in/avatars/tolu_100.jpg\"},"
    "\"account\":{\"id\":42,\"type\":\"free\"}}";
  static const char *itens =
    "{\"movies\":[{\"last_watched_at\":\"2026-10-10T20:00:00Z\","
      "\"movie\":{\"title\":\"Dune\",\"runtime\":120,\"ids\":{\"imdb\":\"tt1160419\"}}},"
      "{\"last_watched_at\":\"2026-09-30T20:00:00Z\","
      "\"movie\":{\"title\":\"Old\",\"runtime\":99,\"ids\":{\"imdb\":\"tt0000001\"}}}],"
    "\"shows\":[{\"show\":{\"title\":\"Severance\",\"runtime\":45,"
      "\"ids\":{\"imdb\":\"tt11280740\"}},\"seasons\":[{\"number\":2,\"episodes\":["
      "{\"number\":1,\"watched_at\":\"2026-10-11T19:00:00Z\"},"
      "{\"number\":2,\"runtime\":50,\"watched_at\":\"2026-10-11T20:00:00Z\"}]}]}],"
    "\"anime\":[]}";
  PerfilDados d;
  time_t agora = (time_t)(js_ms_iso("2026-10-12T12:00:00Z") / 1000);
  assert(simkl_perfil_ler(settings, itens, &d, agora));
  assert(!strcmp(d.nome, "Tolu no Simkl"));
  assert(strstr(d.avatar, "simkl.in/avatars"));
  assert(d.plays == 3 && d.filmes == 1 && d.episodios == 2);
  assert(d.minutos == 215 && d.diasAtivosMes == 2);
  assert(d.nDestaques == 2 && !strcmp(d.destaques[0].id, "tt11280740"));
  assert(d.destaques[0].plays == 2 && strstr(d.destaques[0].detalhe, "2"));
  OK("perfil: identidade e estatisticas mensais saem do Simkl; outro mes fica fora");
}

int main(int argc, char **argv) {
  (void)argv;
  if (argc > 1) {
    CatItem *v = calloc(12, sizeof *v);
    assert(v); tokenAtual = TOKEN; libraryFailure = 1;
    assert(simkl_plantowatch(v, 12) == -1);
    free(v); puts("#639 Simkl partial snapshot: PASS"); return 0;
  }
  char caminho[NVT_MAX]; nvt_set(caminho, sizeof caminho, "/nuvio-simkl-cw-XXXXXX");
  int fd = mkstemp(caminho), salvo = dup(1);
  FILE *f;
  char linha[512];
  int vazou = 0;
  assert(fd >= 0 && salvo >= 0);
  fflush(stdout);
  dup2(fd, 1);

  semVinculo();
  leitores();
  retomada();
  retomadaVencidos();
  retomadaAnimeRepetido();
  maisMenos();
  perfilStats();
  perfilFalhas();
  perfilAnimeRanking();
  perfilRedeCasos();
  perfilBiblioteca();
  historicoEpisodios();
  historicoBiblioteca();
  bibliotecaNaAbertura();
  animeVariasEntradas();
  bibliotecaDelta();
  simkl_esquecer();
  assert(!simkl_na_plantowatch("tt2788316"));

  fflush(stdout);
  dup2(salvo, 1);
  f = fopen(caminho, "r");
  assert(f);
  while (fgets(linha, sizeof linha, f)) if (strstr(linha, TOKEN)) vazou = 1;
  fclose(f);
  unlink(caminho);
  assert(!vazou);
  OK("o token nunca aparece no stdout do modulo");
  printf("simkl_cw: tudo ok\n");
  return 0;
}
