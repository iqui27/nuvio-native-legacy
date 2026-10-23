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
#include "../src/rede.h"
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

char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *buf) { (void)nome; (void)buf; return 1; }
int dados_apagar(const char *nome) { (void)nome; return 1; }
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

static char atividades[200] = "{\"all\":\"2024-01-20T21:00:00Z\"}";
static int batidas[9];   // activities, playback, watching, ptw filmes, ptw series,
                         // post, delete, outro, anime
static char ultimaRota[64], ultimoCorpo[300], ultimaUrl[700];
static int  semBearer, comApiKey;

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
    if (trocarNaResposta == 1) { tokenAtual = "OUTRO-TOKEN-FALSO"; credGeracao++; }
    return strdup("{\"user\":{\"name\":\"Identidade Simkl\"}}");
  }
  if (perfilRede && !strcmp(r, "/sync/all-items/all/all")) {
    assert(strstr(url, "include_all_episodes=original") && strstr(url, "episode_watched_at=yes"));
    if (trocarNaResposta == 2) { tokenAtual = "OUTRO-TOKEN-FALSO"; credGeracao++; }
    *st = perfilHistorySt;
    return perfilHistoryBody ? strdup(perfilHistoryBody) : NULL;
  }
  if (!strcmp(r, "/sync/activities"))                   { batidas[0]++; return strdup(atividades); }
  if (!strcmp(r, "/sync/playback"))                     { batidas[1]++; return strdup(PLAYBACK); }
  if (!strcmp(r, "/sync/all-items/shows/watching"))     { batidas[2]++; return strdup(WATCHING); }
  if (!strcmp(r, "/sync/all-items/movies/plantowatch")) { batidas[3]++; return strdup(PTW_FILMES); }
  if (!strcmp(r, "/sync/all-items/shows/plantowatch"))  { batidas[4]++; return strdup(PTW_SERIES); }
  // Anime e um terceiro tipo: /anime/<estado>, raiz "anime".
  if (!strncmp(r, "/sync/all-items/anime/", 22)) {
    batidas[8]++;
    if (!strcmp(r, "/sync/all-items/anime/watching"))    return strdup(WATCHING_ANIME);
    if (!strcmp(r, "/sync/all-items/anime/plantowatch")) return strdup(PTW_ANIME);
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
  for (i = 0; i < 9; i++) t += batidas[i];
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
  assert(batidas[0] == 1 && batidas[1] == 1 && batidas[2] == 1 && batidas[8] == 1);
  assert(!semBearer && !comApiKey);
  OK("retomada: pausados + a seguir de shows E anime, sem repetir serie");

  // Atividades iguais: nada de playback nem all-items de novo.
  n = simkl_continuar(v, 12);
  assert(n == 6 && batidas[0] == 2 && batidas[1] == 1 && batidas[2] == 1 && batidas[8] == 1);
  // Atividade nova: rebaixa.
  snprintf(atividades, sizeof atividades, "{\"all\":\"2024-01-21T00:00:00Z\"}");
  n = simkl_continuar(v, 12);
  assert(n == 6 && batidas[0] == 3 && batidas[1] == 2 && batidas[2] == 2 && batidas[8] == 2);
  OK("retomada: /sync/activities igual nao baixa all-items de novo");

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
  assert(batidas[3] == 1 && batidas[4] == 1 && batidas[8] == 1);
  assert(v[0].naLista && v[1].naLista && v[2].naLista);
  assert(!strcmp(v[2].imdb, "tt22248376") && !strcmp(v[2].tipo, "series"));
  assert(simkl_na_plantowatch("tt2788316") && simkl_na_plantowatch("tt1160419"));
  assert(simkl_na_plantowatch("tt22248376"));
  assert(simkl_lista_tipo("tt2788316", "series", 0) == 1);
  assert(esperarEscrita() == SMK_OP_CONFIRMADA);
  assert(!strcmp(ultimoCorpo, "{\"shows\":[{\"ids\":{\"imdb\":\"tt2788316\"}}]}"));
  OK("plantowatch da rede: naLista e a guarda do - conhece o que veio de la");
  simkl_esquecer();
  libraryFailure = 1;
  assert(simkl_plantowatch(v, 12) == -1);
  libraryFailure = 0;
  simkl_esquecer();
  OK("partial Simkl library is rejected instead of replacing complete membership");
}

static void perfilRedeCasos(void) {
  PerfilDados d;
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
  OK("perfil: HTTP/HTML falham e relink no meio da consulta nao mistura contas");
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
  maisMenos();
  perfilStats();
  perfilFalhas();
  perfilAnimeRanking();
  perfilRedeCasos();
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
