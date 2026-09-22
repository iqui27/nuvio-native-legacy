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
#include "../src/ajustes.h"
#include "../src/rede.h"
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

static const char *tokenAtual = "";
const char *simklauth_token(void) { return tokenAtual; }
const char *nuvem_simkl_cliente(void) { return "cid-teste"; }
const char *nuvem_simkl_app(void) { return "nuvio"; }
void nuvem_url_escapar(const char *v, char *dst, unsigned tam) { snprintf(dst, tam, "%s", v); }
const char *i18n(const char *s) { return s; }
// O enfeite real vai ao Cinemeta; aqui devolve o lote como veio.
int trakt_enfeitar_lote(CatItem *saida, int n) { (void)saida; return n; }

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

static const char *PTW_FILMES =
  "{\"movies\":[{\"added_to_watchlist_at\":\"2024-01-01T00:00:00Z\",\"status\":\"plantowatch\","
  "\"movie\":{\"title\":\"Dune\",\"year\":2021,\"ids\":{\"simkl\":1,\"imdb\":\"tt1160419\"}}}]}";
static const char *PTW_SERIES =
  "{\"shows\":[{\"status\":\"plantowatch\",\"next_to_watch\":\"S01E01\","
  "\"show\":{\"title\":\"Shogun\",\"year\":2024,\"ids\":{\"imdb\":\"tt2788316\"}}}]}";

static char atividades[200] = "{\"all\":\"2024-01-20T21:00:00Z\"}";
static int batidas[8];   // activities, playback, watching, ptw filmes, ptw series, post, delete, outro
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

char *rede_baixar_st(const char *url, int s, const char *const *cab, int *st) {
  char r[64];
  (void)s;
  conferirCabecalhos(url, cab);
  rota(url, r, sizeof r);
  snprintf(ultimaRota, sizeof ultimaRota, "%s", r);
  *st = 200;
  if (!strcmp(r, "/sync/activities"))                   { batidas[0]++; return strdup(atividades); }
  if (!strcmp(r, "/sync/playback"))                     { batidas[1]++; return strdup(PLAYBACK); }
  if (!strcmp(r, "/sync/all-items/shows/watching"))     { batidas[2]++; return strdup(WATCHING); }
  if (!strcmp(r, "/sync/all-items/movies/plantowatch")) { batidas[3]++; return strdup(PTW_FILMES); }
  if (!strcmp(r, "/sync/all-items/shows/plantowatch"))  { batidas[4]++; return strdup(PTW_SERIES); }
  batidas[7]++;
  *st = 404;
  return strdup("{\"error\":\"id_err\",\"code\":404}");
}

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
  for (i = 0; i < 8; i++) t += batidas[i];
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

  n = simkl_ler_assistindo(WATCHING, v, 12);
  assert(n == 2);   // "em dia" (null) e sem imdb ficam fora
  assert(!strcmp(v[0].imdb, "tt0903747:1:6") && v[0].progresso == 0);
  assert(!strcmp(v[1].imdb, "tt11280740:2:3") && v[1].temporada == 2 && v[1].episodio == 3);
  assert(!strcmp(v[1].titulo, "Severance") && v[1].retomadoMs > v[0].retomadoMs);
  assert(simkl_ler_assistindo("null", v, 12) == 0);
  assert(simkl_ler_assistindo("{}", v, 12) == 0);
  OK("watching: next_to_watch \"S02E03\" vira o episodio seguinte com 0%");

  n = simkl_ler_plantowatch(PTW_FILMES, 0, v, 12);
  assert(n == 1 && !strcmp(v[0].imdb, "tt1160419") && v[0].naLista && !strcmp(v[0].tipo, "movie"));
  assert(v[0].poster[0]);   // arte do metahub, sem GET
  n = simkl_ler_plantowatch(PTW_SERIES, 1, v, 12);
  assert(n == 1 && !strcmp(v[0].imdb, "tt2788316") && !strcmp(v[0].tipo, "series"));
  OK("plantowatch: filmes e series com naLista");
}

static void retomada(void) {
  CatItem v[12];
  int n, i;
  memset(batidas, 0, sizeof batidas);
  n = simkl_continuar(v, 12);
  // 3 pausados + Severance (a seguir). Breaking Bad do watching NAO entra de
  // novo: a serie ja esta pausada no S01E05.
  assert(n == 4);
  for (i = 0; i < n; i++) assert(strncmp(v[i].imdb, "tt0903747:1:6", 13));
  // Mais recente primeiro: Severance (20/01) antes de Inception (15/01 11:15).
  assert(!strcmp(v[0].imdb, "tt11280740:2:3"));
  assert(!strcmp(v[1].imdb, "tt1375666"));
  assert(simkl_e_a_seguir("tt11280740:2:3"));
  assert(!simkl_e_a_seguir("tt1375666"));
  assert(batidas[0] == 1 && batidas[1] == 1 && batidas[2] == 1);
  assert(!semBearer && !comApiKey);
  OK("retomada: pausados + a seguir, sem repetir serie, mais recente primeiro");

  // Atividades iguais: nada de playback nem all-items de novo.
  n = simkl_continuar(v, 12);
  assert(n == 4 && batidas[0] == 2 && batidas[1] == 1 && batidas[2] == 1);
  // Atividade nova: rebaixa.
  snprintf(atividades, sizeof atividades, "{\"all\":\"2024-01-21T00:00:00Z\"}");
  n = simkl_continuar(v, 12);
  assert(n == 4 && batidas[0] == 3 && batidas[1] == 2 && batidas[2] == 2);
  OK("retomada: /sync/activities igual nao baixa all-items de novo");

  // Lista cheia: o "a seguir" mais novo toma o lugar do pausado mais velho.
  n = simkl_continuar(v, 3);
  assert(n == 3 && !strcmp(v[0].imdb, "tt11280740:2:3"));
  for (i = 0; i < n; i++) assert(strcmp(v[i].imdb, "tt2560140:2:3"));   // o de 14/01 saiu
  OK("retomada: com a fileira cheia, o a seguir mais novo substitui o mais velho");

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
  assert(simkl_plantowatch(v, 12) == 2);
  assert(batidas[3] == 1 && batidas[4] == 1);
  assert(v[0].naLista && v[1].naLista);
  assert(simkl_na_plantowatch("tt2788316") && simkl_na_plantowatch("tt1160419"));
  assert(simkl_lista_tipo("tt2788316", "series", 0) == 1);
  assert(esperarEscrita() == SMK_OP_CONFIRMADA);
  assert(!strcmp(ultimoCorpo, "{\"shows\":[{\"ids\":{\"imdb\":\"tt2788316\"}}]}"));
  OK("plantowatch da rede: naLista e a guarda do - conhece o que veio de la");
}

int main(void) {
  char caminho[] = "/tmp/nuvio-simkl-cw-XXXXXX";
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
