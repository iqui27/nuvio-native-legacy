// Scrobble do Simkl a partir do player (issue #459): start / pause / stop com
// porcentagem, contra uma rede FALSA que responde no formato da documentacao
// (api.simkl.org/api-reference/simkl/scrobble-*). Sem SDL, sem rede.
//
// O que a prova cobre: rota e corpo de cada chamada, conta de porcentagem,
// nenhuma chamada sem vinculo ou com guarda recusada, 401 desfaz o vinculo,
// 429/5xx/400-RATE_LIMIT abrem janela de espera (sem tempestade de retry), e
// o stop que falha cai no historico em vez de perder o "visto".
//
// Compilado com -DSMK_SCR_ESPACO_MS=0 -DSMK_SCR_ESPERA_MS=150 para a janela de
// espera caber em milissegundos. STDOUT e capturado e o token nao pode aparecer.
#include "../src/simkl.h"
#include "nvtmp.h"
#include "../src/traktscrobble.h"
#include "../src/trakt.h"
#include "stub_perfisessao.h"
#include "../src/ajustes.h"
#include "../src/rede.h"
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

static char disco[256];
char *dados_ler(const char *nome) {
  char caminho[512], buf[8192];
  FILE *f;
  size_t n;
  snprintf(caminho, sizeof caminho, "%s/%s", disco, nome);
  f = fopen(caminho, "r");
  if (!f) return NULL;
  n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f); buf[n] = 0;
  return strdup(buf);
}
int dados_gravar(const char *nome, const char *buf) {
  char caminho[512];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/%s", disco, nome);
  f = fopen(caminho, "w");
  if (!f) return 0;
  fputs(buf, f);
  return fclose(f) == 0;
}
int dados_apagar(const char *nome) {
  char caminho[512];
  snprintf(caminho, sizeof caminho, "%s/%s", disco, nome);
  return unlink(caminho) == 0;
}

#define TOKEN "TOKEN-SIMKL-SEGREDO-459"
#define OK(s) fprintf(stderr, "ok  %s\n", s)

// ---------------------------------------------------------------- stubs

int ajustes_cw_proximo(void) { return 1; }
int ajustes_cw_minimo(void) { return 1; }
int ajustes_cw_concluido(void) { return 90; }
static const char *tokenAtual = "";
static int nInvalidar;
static unsigned long long credGeracao = 1;
static pthread_t principal;
static int tokenLidoNoFio;   // leituras por ponteiro fora do fio principal
const char *simklauth_token(void) {
  if (!pthread_equal(pthread_self(), principal)) tokenLidoNoFio++;
  return tokenAtual;
}
unsigned long long simklauth_credencial_copiar(char *dst, size_t tam) {
  if (dst && tam) snprintf(dst, tam, "%s", tokenAtual);
  return credGeracao;
}
// O real apaga o arquivo do perfil, e so se a credencial ainda e a recusada.
int simklauth_invalidar_se(unsigned long long cred) {
  if (cred != credGeracao) return 0;
  tokenAtual = ""; nInvalidar++; credGeracao++;
  return 1;
}
const char *nuvem_simkl_cliente(void) { return "cid-teste"; }
const char *nuvem_simkl_app(void) { return "nuvio"; }
void nuvem_url_escapar(const char *v, char *dst, unsigned tam) { snprintf(dst, tam, "%s", v); }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
int trakt_enfeitar_lote(CatItem *saida, int n) { (void)saida; return n; }
int trakt_enfeitar_lote_sessao(CatItem *saida, int n, uint64_t sessao) {
  (void)saida; (void)n; (void)sessao;
  abort(); // Scrobble/history scenarios never request catalog enrichment.
}

// ---------------------------------------------------------------- rede falsa

static int posts;                       // POSTs de qualquer rota
static int nScr, nHist;                 // /scrobble/* e /sync/history
static char ultimaRota[64], ultimoCorpo[400], ultimaUrl[700];
static int semBearer;
static int stScr = 200;                 // status devolvido por /scrobble/*
static int stHist = 200;
static const char *corpoScr = "{\"id\":1}";
static void (*duranteScr)(void);        // roda dentro do POST /scrobble/*
static volatile int segurarHist, histSegurado, histOutroBearer;
static int logoutNaEspera;

// So simkl.c e compilado com usleep redirecionado: o logout ocorre exatamente
// na espera entre POSTs, sem depender da velocidade do escalonador.
static int esperarMicros(useconds_t us) {
  struct timespec restante = { us / 1000000u, (long)(us % 1000000u) * 1000L };
  while (nanosleep(&restante, &restante)) {
    if (errno != EINTR) return -1;
  }
  return 0;
}
int simkl_teste_usleep(useconds_t us) {
  if (logoutNaEspera && us > 1000000) {
    if (logoutNaEspera != 2) { tokenAtual = ""; credGeracao++; }
    if (logoutNaEspera != 3) simkl_esquecer();
    logoutNaEspera = 0;
    return 0;
  }
  return esperarMicros(us);
}

char *rede_postar_st(const char *url, int s, const char *const *cab,
                     const char *corpo, int *st) {
  const char *p = strstr(url, "api.simkl.com");
  size_t n;
  int k, bearer = 0;
  (void)s;
  assert(p);
  p += strlen("api.simkl.com");
  n = strcspn(p, "?");
  assert(n < sizeof ultimaRota);
  memcpy(ultimaRota, p, n); ultimaRota[n] = 0;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo ? corpo : "");
  assert(strstr(url, "client_id=cid-teste"));
  assert(strstr(url, "app-name=nuvio"));
  assert(strstr(url, "app-version="));
  for (k = 0; cab && cab[k]; k++)
    if (!strcmp(cab[k], "Authorization: Bearer " TOKEN)) bearer = 1;
  if (!bearer) semBearer = 1;
  posts++;
  if (!strncmp(ultimaRota, "/scrobble/", 10)) {
    nScr++;
    if (duranteScr) duranteScr();
    *st = stScr;
    return strdup(corpoScr);
  }
  if (!strcmp(ultimaRota, "/sync/history")) {
    nHist++;
    if (!bearer) histOutroBearer++;
    if (segurarHist) {   // outro envio segura travaCorpo ate o teste soltar
      segurarHist = 0; histSegurado = 1;
      while (histSegurado) usleep(1000);
    }
  }
  *st = stHist;
  return strdup("{\"added\":{},\"not_found\":{\"movies\":[],\"shows\":[]}}");
}

char *rede_baixar_st(const char *url, int s, const char *const *cab, int *st) {
  (void)url; (void)s; (void)cab;
  *st = 200;
  return strdup("{}");
}
char *rede_apagar(const char *url, int s, const char *const *cab, int *st) {
  (void)url; (void)s; (void)cab;
  *st = 204;
  return strdup("");
}

// ---------------------------------------------------------------- ajudantes

static void esperar(void) {
  int i;
  for (i = 0; i < 400 && !simkl_scrobble_ocioso(); i++) usleep(5000);
  assert(simkl_scrobble_ocioso());
}

// Uma chamada e a espera do fio. Devolve a acao decidida; zera os contadores.
static int mandar(int ev, const char *id, double pos, double dur, double ass, int assistiu) {
  int a;
  nScr = nHist = 0;
  a = simkl_scrobble(ev, id, pos, dur, ass, assistiu);
  esperar();
  return a;
}

static void esperarJanela(void) { usleep(700 * 1000); }   // > 2 x SMK_SCR_ESPERA_MS

// ---------------------------------------------------------------- cenarios

static void semVinculo(void) {
  tokenAtual = "";
  posts = 0;
  assert(mandar(SCR_EV_TOCANDO, "tt1375666", 300, 3000, 300, 0) == SCR_NADA);
  assert(mandar(SCR_EV_SAIU, "tt1375666", 3000, 3000, 2700, 1) == SCR_NADA);
  assert(posts == 0);
  OK("sem vinculo nao ha chamada de rede");
  tokenAtual = TOKEN;
}

static void filme(void) {
  assert(mandar(SCR_EV_TOCANDO, "tt1375666", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 1 && !strcmp(ultimaRota, "/scrobble/start"));
  assert(!strcmp(ultimoCorpo, "{\"movie\":{\"ids\":{\"imdb\":\"tt1375666\"}},\"progress\":10.00}"));
  // Mesmo titulo tocando de novo nao repete o start.
  assert(mandar(SCR_EV_TOCANDO, "tt1375666", 330, 3000, 330, 0) == SCR_NADA && nScr == 0);
  assert(mandar(SCR_EV_PAUSOU, "tt1375666", 1000, 3000, 1000, 0) == SCR_PAUSE);
  assert(nScr == 1 && !strcmp(ultimaRota, "/scrobble/pause"));
  // 1000/3000 = 33,33...%: duas casas, o maximo que o Simkl aceita.
  assert(!strcmp(ultimoCorpo, "{\"movie\":{\"ids\":{\"imdb\":\"tt1375666\"}},\"progress\":33.33}"));
  assert(!semBearer);
  OK("filme: start/pause com rota, corpo e porcentagem certos");
}

static void episodio(void) {
  assert(mandar(SCR_EV_TOCANDO, "tt0903747:1:5", 60, 2400, 60, 0) == SCR_START);
  assert(!strcmp(ultimaRota, "/scrobble/start"));
  assert(!strcmp(ultimoCorpo,
    "{\"show\":{\"ids\":{\"imdb\":\"tt0903747\"}},\"episode\":{\"season\":1,\"number\":5},\"progress\":2.50}"));
  assert(mandar(SCR_EV_SAIU, "tt0903747:1:5", 2400, 2400, 2000, 1) == SCR_STOP);
  assert(nScr == 1 && !strcmp(ultimaRota, "/scrobble/stop"));
  assert(!strcmp(ultimoCorpo,
    "{\"show\":{\"ids\":{\"imdb\":\"tt0903747\"}},\"episode\":{\"season\":1,\"number\":5},\"progress\":100.00}"));
  assert(nHist == 0);   // stop aceito: o historico NAO e mandado de novo
  // Saiu antes do fim: pause com o ponto, nunca stop (nao marca visto).
  assert(mandar(SCR_EV_TOCANDO, "tt0903747:1:6", 60, 2400, 60, 0) == SCR_START);
  assert(mandar(SCR_EV_SAIU, "tt0903747:1:6", 1200, 2400, 1200, 0) == SCR_PAUSE);
  assert(!strcmp(ultimaRota, "/scrobble/pause"));
  assert(strstr(ultimoCorpo, "\"progress\":50.00"));
  OK("episodio: show+season/number, stop a 100% e pause ao sair no meio");
}

static void especial(void) {
  simkl_esquecer(); tokenAtual = TOKEN; stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "tt12409194:0:1", 60, 2400, 60, 0) == SCR_START);
  assert(strstr(ultimoCorpo, "\"show\"") && strstr(ultimoCorpo, "\"season\":0"));
  assert(!strstr(ultimoCorpo, "\"movie\""));
  stScr = 500;
  assert(mandar(SCR_EV_SAIU, "tt12409194:0:1", 2400, 2400, 2000, 1) == SCR_STOP);
  assert(nHist == 1 && !strcmp(ultimaRota, "/sync/history"));
  assert(strstr(ultimoCorpo, "\"shows\"") && strstr(ultimoCorpo, "\"number\":0"));
  assert(!strstr(ultimoCorpo, "\"movies\""));
  stScr = 200; simkl_esquecer();
  assert(mandar(SCR_EV_TOCANDO, "tt12409194:0:0", 60, 2400, 60, 0) == SCR_NADA);
  assert(mandar(SCR_EV_TOCANDO, "tt12409194:-1:1", 60, 2400, 60, 0) == SCR_NADA);
  OK("#556: S00 e episodio no scrobble e no historico, nunca filme");
}

// Anime add-on ids, as NuvioTV sends them (SimklMutationBodies.kt): with a
// season a "show" in TVDB-style coordinates, without one an "anime" with the
// absolute number. They were refused before: Kitsu anime never reached Simkl.
static void anime(void) {
  char c[200];
  simkl_esquecer(); tokenAtual = TOKEN; stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "kitsu:41370:1:2", 60, 2400, 60, 0) == SCR_START);
  assert(!strcmp(ultimaRota, "/scrobble/start"));
  assert(!strcmp(ultimoCorpo,
    "{\"show\":{\"ids\":{\"kitsu\":41370}},\"episode\":{\"season\":1,\"number\":2},\"progress\":2.50}"));
  assert(mandar(SCR_EV_TOCANDO, "mal:5114:25", 60, 2400, 60, 0) == SCR_START);
  assert(!strcmp(ultimoCorpo,
    "{\"anime\":{\"ids\":{\"mal\":5114}},\"episode\":{\"number\":25},\"progress\":2.50}"));
  // A stop Simkl rejects for good leaves the journal; the IMDb history
  // fallback doesn't apply to these ids.
  stScr = 404;
  assert(mandar(SCR_EV_SAIU, "mal:5114:25", 2400, 2400, 2000, 1) == SCR_STOP);
  assert(nScr == 1 && nHist == 0 && !strcmp(ultimaRota, "/scrobble/stop"));
  stScr = 200; simkl_esquecer();
  // IMDb bodies are unchanged, and the builder refuses what it can't name.
  assert(simkl_corpo_scrobble(c, sizeof c, "tt0903747:1:5", 50.0) &&
         !strcmp(c, "{\"show\":{\"ids\":{\"imdb\":\"tt0903747\"}},\"episode\":{\"season\":1,\"number\":5},\"progress\":50.00}"));
  assert(!simkl_corpo_scrobble(c, sizeof c, "tvdb:123:1:2", 50.0) && !c[0]);
  assert(!simkl_corpo_scrobble(c, sizeof c, "kitsu:0:1", 50.0));
  OK("anime: kitsu/mal com temporada viram show, sem temporada viram anime");
}

static void guardas(void) {
  posts = 0;
  assert(mandar(SCR_EV_TOCANDO, "tt2000001", 10, 90, 10, 0) == SCR_NADA);      // < 120 s
  assert(mandar(SCR_EV_TOCANDO, "tt2000002", 10, 3000, 1, 0) == SCR_NADA);     // assistiu < 5 s
  assert(mandar(SCR_EV_TOCANDO, "tmdb:m123", 100, 3000, 100, 0) == SCR_NADA);  // nao e IMDb
  // Anime add-on episodes now go (see `anime`); the id alone, with no
  // episode, or a non-numeric one still doesn't.
  assert(mandar(SCR_EV_TOCANDO, "kitsu:41370", 100, 3000, 100, 0) == SCR_NADA);
  assert(mandar(SCR_EV_TOCANDO, "kitsu:abc:1:2", 100, 3000, 100, 0) == SCR_NADA);
  assert(mandar(SCR_EV_TOCANDO, "", 100, 3000, 100, 0) == SCR_NADA);
  assert(mandar(SCR_EV_SAIU, "tt2000003", 2900, 3000, 200, 0) == SCR_NADA);    // 96% sem ter assistido
  assert(posts == 0);
  OK("guardas do Trakt valem aqui: clipe curto, id sem IMDb, saida >=90% sem assistir");
}

static void indevido(void) {
  // Pausa abaixo de 1% nao e ponto de retomada (mesma regra do Trakt).
  assert(mandar(SCR_EV_TOCANDO, "tt2000004", 10, 3000, 10, 0) == SCR_START);
  assert(mandar(SCR_EV_PAUSOU, "tt2000004", 10, 3000, 10, 0) == SCR_NADA && nScr == 0);
  OK("pause abaixo de 1% nao sai");
}

static void falhaNaoAutorizada(void) {
  int antes = nInvalidar;
  stScr = 401;
  corpoScr = "{\"error\":\"user_token_failed\"}";
  assert(mandar(SCR_EV_TOCANDO, "tt3000001", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 1);
  assert(nInvalidar == antes + 1);
  assert(!simkl_ativo());
  // Desconectado: nada mais sai, e a fila nao fica repetindo.
  posts = 0;
  assert(mandar(SCR_EV_SAIU, "tt3000001", 1500, 3000, 1500, 0) == SCR_NADA);
  assert(mandar(SCR_EV_TOCANDO, "tt3000002", 300, 3000, 300, 0) == SCR_NADA);
  assert(posts == 0);
  OK("401: vinculo desfeito uma vez e nenhuma chamada depois");
  tokenAtual = TOKEN;
  stScr = 200;
  corpoScr = "{\"id\":1}";
}

static void espera(void) {
  stScr = 429;
  assert(mandar(SCR_EV_TOCANDO, "tt4000001", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 1);
  // Dentro da janela: nada sai, nem para outro titulo.
  stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "tt4000002", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 0);
  assert(simkl_ativo());   // 429 nao desvincula
  esperarJanela();
  assert(mandar(SCR_EV_TOCANDO, "tt4000003", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 1 && !strcmp(ultimaRota, "/scrobble/start"));
  OK("429: janela de espera sem retry, e volta depois dela");

  // 5xx e o 400 RATE_LIMIT do lock de 20 s tambem esperam; 404 nao.
  stScr = 503;
  assert(mandar(SCR_EV_TOCANDO, "tt4000004", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 1);
  stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "tt4000005", 300, 3000, 300, 0) == SCR_START && nScr == 0);
  esperarJanela();
  stScr = 400;
  corpoScr = "{\"error\":\"RATE_LIMIT\"}";
  assert(mandar(SCR_EV_TOCANDO, "tt4000006", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  stScr = 200;
  corpoScr = "{\"id\":1}";
  assert(mandar(SCR_EV_TOCANDO, "tt4000007", 300, 3000, 300, 0) == SCR_START && nScr == 0);
  esperarJanela();
  stScr = 404;
  corpoScr = "{\"error\":\"id_err\"}";
  assert(mandar(SCR_EV_TOCANDO, "tt4000008", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  stScr = 200;
  corpoScr = "{\"id\":1}";
  assert(mandar(SCR_EV_TOCANDO, "tt4000009", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  OK("5xx e 400 RATE_LIMIT esperam; 404 nao abre janela");
}

static void stopQueFalha(void) {
  esperarJanela();
  stScr = 500;
  assert(mandar(SCR_EV_TOCANDO, "tt5000001:2:3", 60, 2400, 60, 0) == SCR_START);
  esperarJanela();
  stScr = 500;
  assert(mandar(SCR_EV_SAIU, "tt5000001:2:3", 2400, 2400, 2000, 1) == SCR_STOP);
  assert(nScr == 1 && nHist == 1);
  assert(!strcmp(ultimaRota, "/sync/history"));
  assert(strstr(ultimoCorpo, "\"number\":2") && strstr(ultimoCorpo, "\"number\":3"));
  OK("stop que falha cai no historico (o visto nao se perde)");
  // 409: ja registrado, e sucesso. Sem fallback.
  esperarJanela();
  stScr = 409;
  assert(mandar(SCR_EV_SAIU, "tt5000002", 3000, 3000, 2800, 1) == SCR_STOP);
  assert(nScr == 1 && nHist == 0);
  stScr = 200;
  OK("stop 409 conta como registrado");
}

static void trocaDePerfil(void) { credGeracao++; }
static void logoutNoVoo(void) { simkl_esquecer(); }

// 401 de um POST em voo que saiu com o token do perfil ANTERIOR nao pode
// desvincular o perfil que esta ligado agora.
static void naoRespondeAoPerfilErrado(void) {
  int antes = nInvalidar;
  stScr = 401;
  duranteScr = trocaDePerfil;
  assert(mandar(SCR_EV_TOCANDO, "tt6000001", 300, 3000, 300, 0) == SCR_START);
  assert(nScr == 1);
  duranteScr = NULL;
  assert(nInvalidar == antes && simkl_ativo());
  // Nem janela de espera: o proximo titulo sai na hora.
  stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "tt6000002", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  OK("401 de credencial antiga nao desvincula o perfil atual");
}

// O Authorization sai da COPIA da credencial, nunca de simklauth_token().
static void tokenPorCopia(void) {
  tokenLidoNoFio = 0;
  assert(mandar(SCR_EV_TOCANDO, "tt6000003", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  assert(!semBearer && tokenLidoNoFio == 0);
  OK("o fio de envio le o token por copia");
}

// Logout / troca de perfil zeram o "start em pe" e a janela de espera.
static void esquecerZeraEstado(void) {
  assert(mandar(SCR_EV_TOCANDO, "tt6000004", 300, 3000, 300, 0) == SCR_START);
  assert(mandar(SCR_EV_TOCANDO, "tt6000004", 330, 3000, 330, 0) == SCR_NADA);
  simkl_esquecer();
  assert(mandar(SCR_EV_TOCANDO, "tt6000004", 360, 3000, 360, 0) == SCR_START && nScr == 1);
  // Janela aberta por 429 some com o esquecer.
  stScr = 429;
  assert(mandar(SCR_EV_TOCANDO, "tt6000005", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "tt6000006", 300, 3000, 300, 0) == SCR_START && nScr == 0);
  simkl_esquecer();
  assert(mandar(SCR_EV_TOCANDO, "tt6000007", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  // Esquecer durante o POST: a resposta 429 em voo nao reabre a janela.
  stScr = 429;
  duranteScr = logoutNoVoo;
  assert(mandar(SCR_EV_TOCANDO, "tt6000008", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  duranteScr = NULL;
  stScr = 200;
  assert(mandar(SCR_EV_TOCANDO, "tt6000009", 300, 3000, 300, 0) == SCR_START && nScr == 1);
  OK("esquecer zera o start em pe e a janela de espera");
}

static void pausaAntesDeConcluir(void) {
  const double pontos[] = { 80, 85, 89.99 };
  unsigned i;
  tokenAtual = TOKEN;
  for (i = 0; i < sizeof pontos / sizeof pontos[0]; i++) {
    assert(mandar(SCR_EV_TOCANDO, "tt7000001", 300, 3000, 300, 0) == SCR_START);
    assert(mandar(SCR_EV_PAUSOU, "tt7000001", pontos[i] * 30, 3000, 2400, 0) == SCR_PAUSE);
    assert(nScr == 1 && nHist == 0 && !strcmp(ultimaRota, "/scrobble/pause"));
    assert(mandar(SCR_EV_SAIU, "tt7000001", pontos[i] * 30, 3000, 2400, 0) == SCR_PAUSE);
    assert(nScr == 1 && nHist == 0 && !strcmp(ultimaRota, "/scrobble/pause"));
  }
  OK("P2: pause entre 80 e 90% salva retomada sem marcar visto");
}

static void logoutDuranteEspera(void) {
  int modo;
  for (modo = 1; modo <= 3; modo++) {
    tokenAtual = TOKEN;
    assert(mandar(SCR_EV_TOCANDO, "tt7000002", 300, 3000, 300, 0) == SCR_START);
    logoutNaEspera = modo;   // ambos, so geracao, so credencial
    assert(mandar(SCR_EV_PAUSOU, "tt7000002", 600, 3000, 600, 0) == SCR_PAUSE);
    assert(!logoutNaEspera);
    assert(nScr == 0 && nHist == 0);
  }
  OK("P2: logout, geracao e credencial revalidados depois da espera");
}

static pthread_mutex_t redeLentaTrava = PTHREAD_MUTEX_INITIALIZER;
static struct timespec saidaDesde;
static void saidaDiagnostico(const char *etapa) {
  struct timespec agora;
  assert(!clock_gettime(CLOCK_MONOTONIC, &agora));
  double ms = (agora.tv_sec - saidaDesde.tv_sec) * 1000.0 +
              (agora.tv_nsec - saidaDesde.tv_nsec) / 1000000.0;
  fprintf(stderr, "shutdown child: %s pid=%ld elapsed_ms=%.0f\n",
          etapa, (long)getpid(), ms);
  fflush(stderr);
}
static void redeLenta(void) {
  saidaDiagnostico("callback pending");
  pthread_mutex_lock(&redeLentaTrava);
  pthread_mutex_unlock(&redeLentaTrava);
}
static void vistoSobreviveSaida(void) {
  pid_t filho = fork();
  int status, achou = 0;
  DIR *d;
  struct dirent *ent;
  assert(filho >= 0);
  if (!filho) {
    // A resposta fica pendente ate _exit, sem disputar um prazo de sono.
    // Se drenar deixar de ser limitado, o alarme faz a guarda do pai falhar.
    assert(!clock_gettime(CLOCK_MONOTONIC, &saidaDesde));
    saidaDiagnostico("started");
    alarm(10);
    pthread_mutex_lock(&redeLentaTrava);
    saidaDiagnostico("gate held");
    tokenAtual = TOKEN;
    duranteScr = redeLenta;
    assert(simkl_scrobble(SCR_EV_SAIU, "tt7000003:2:4", 3000, 3000, 2800, 1) == SCR_STOP);
    saidaDiagnostico("stop enqueued; drain starting");
    // A rede fica bloqueada ate _exit: uma drenagem curta prova a mesma saida
    // pendente sem depender de 150 despertares dentro do prazo do alarme.
    simkl_scrobble_drenar(100);
    saidaDiagnostico("drain returned");
    assert(!simkl_scrobble_ocioso());
    saidaDiagnostico("pending verified; exiting");
    _exit(0);   // o fio e morto ANTES da resposta e do fallback
  }
  pid_t esperado;
  do { esperado = waitpid(filho, &status, 0); } while (esperado < 0 && errno == EINTR);
  int erro = esperado < 0 ? errno : 0;
  fprintf(stderr, "shutdown wait: pid=%ld returned=%ld errno=%d status=0x%x exit=%d signal=%d\n",
          (long)filho, (long)esperado, erro, esperado == filho ? status : 0,
          esperado == filho && WIFEXITED(status) ? WEXITSTATUS(status) : -1,
          esperado == filho && WIFSIGNALED(status) ? WTERMSIG(status) : 0);
  assert(esperado == filho && WIFEXITED(status) && !WEXITSTATUS(status));
  d = opendir(disco); assert(d);
  while ((ent = readdir(d))) {
    char *buf;
    if (strncmp(ent->d_name, "simkl-vistos-", 13)) continue;
    buf = dados_ler(ent->d_name);
    assert(!buf || !strstr(buf, TOKEN));
    if (buf && strstr(buf, "tt7000003:2:4")) achou = 1;
    free(buf);
  }
  closedir(d);
  assert(achou);
  tokenAtual = "OUTRO-VINCULO"; credGeracao++;
  simkl_esquecer();
  posts = 0;
  simkl_scrobble_retentar(); esperar();
  assert(posts == 0);   // outra credencial nao consome o jornal
  tokenAtual = TOKEN; credGeracao++;
  simkl_esquecer();
  stScr = stHist = 500;
  simkl_scrobble_retentar(); esperar();
  assert(nScr == 1 && nHist == 1);
  posts = 0;
  simkl_scrobble_retentar(); esperar();
  assert(posts == 0);   // falhou: sem tempestade de retry
  simkl_esquecer();
  stScr = stHist = 200;
  nScr = nHist = 0;
  simkl_scrobble_retentar(); esperar();
  assert(nScr == 1 && nHist == 0 && strstr(ultimoCorpo, "tt7000003"));
  simkl_esquecer();
  posts = 0;
  simkl_scrobble_retentar(); esperar();
  assert(posts == 0);   // confirmado: nao retoma de novo
  OK("P2: stop sobrevive saida lenta, retoma so no vinculo certo e sai do disco apos sucesso");
}

static int jornalTem(const char *id) {
  DIR *d = opendir(disco);
  struct dirent *ent;
  int achou = 0;
  assert(d);
  while ((ent = readdir(d))) {
    char *buf;
    if (strncmp(ent->d_name, "simkl-vistos-", 13)) continue;
    buf = dados_ler(ent->d_name);
    if (buf && strstr(buf, id)) achou = 1;
    free(buf);
  }
  closedir(d);
  return achou;
}

static void *segurarCorpo(void *u) {
  VistoPar par = { 1, 1 };
  (void)u;
  simkl_episodios_marcar("tt8000001", &par, 1, 1);
  return NULL;
}

// Review 3: o fallback do stop de A espera travaCorpo, o perfil vira B, e o
// historico nao pode sair com o Bearer de B nem apagar a pendencia de A.
static void fallbackNaoTrocaDePerfil(void) {
  pthread_t t;
  int i;
  tokenAtual = TOKEN;
  stScr = 500;
  segurarHist = 1;
  assert(!pthread_create(&t, NULL, segurarCorpo, NULL));
  for (i = 0; i < 400 && !histSegurado; i++) usleep(5000);
  assert(histSegurado);
  nScr = 0;
  assert(simkl_scrobble(SCR_EV_SAIU, "tt8000002:1:1", 2400, 2400, 2000, 1) == SCR_STOP);
  for (i = 0; i < 400 && !nScr; i++) usleep(5000);
  assert(nScr == 1);
  usleep(200 * 1000);          // fallback de A ja esta na fila da trava
  tokenAtual = "TOKEN-PERFIL-B"; credGeracao++;
  histSegurado = 0;
  pthread_join(t, NULL);
  esperar();
  assert(histOutroBearer == 0);
  assert(jornalTem("tt8000002:1:1"));
  // De volta em A, a pendencia sai pela conta certa.
  tokenAtual = TOKEN; credGeracao++;
  simkl_esquecer();
  stScr = 200;
  nScr = 0;
  simkl_scrobble_retentar(); esperar();
  assert(nScr == 1 && strstr(ultimoCorpo, "tt8000002") && !semBearer);
  assert(!jornalTem("tt8000002:1:1"));
  OK("P2: fallback do stop nao usa o perfil trocado e fica pendente para o original");
}

int main(int argc, char **argv) {
  char caminho[NVT_MAX]; nvt_set(caminho, sizeof caminho, "/nuvio-simkl-scr-XXXXXX");
  int fd = mkstemp(caminho), salvo = dup(1);
  FILE *f;
  char linha[512];
  int vazou = 0;
  assert(fd >= 0 && salvo >= 0);
  fflush(stdout);
  dup2(fd, 1);
  principal = pthread_self();
  snprintf(disco, sizeof disco, "%s", getenv("SIMKL_TEST_DIR"));
  if (argc > 1) {
    if (!strcmp(argv[1], "pause")) pausaAntesDeConcluir();
    else if (!strcmp(argv[1], "logout")) logoutDuranteEspera();
    else if (!strcmp(argv[1], "shutdown")) vistoSobreviveSaida();
    else if (!strcmp(argv[1], "perfil")) fallbackNaoTrocaDePerfil();
    else abort();
  } else {
    semVinculo();
    filme();
    episodio();
    especial();
    anime();
    guardas();
    indevido();
    falhaNaoAutorizada();
    espera();
    stopQueFalha();
    naoRespondeAoPerfilErrado();
    tokenPorCopia();
    esquecerZeraEstado();
  }

  fflush(stdout);
  dup2(salvo, 1);
  f = fopen(caminho, "r");
  assert(f);
  while (fgets(linha, sizeof linha, f)) if (strstr(linha, TOKEN)) vazou = 1;
  fclose(f);
  unlink(caminho);
  assert(!vazou);
  OK("o token nunca aparece no stdout do modulo");
  printf("simkl_scrobble: tudo ok\n");
  return 0;
}
