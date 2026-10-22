// watch_state v2 push, passo 3: o fio de entrega e o filtro por addon, com
// rede, disco, addons e perfil falsos. O relogio e o real, entao o backoff em
// si (30 s .. 6 h) fica em tests/watchstate_fila.c; aqui vale o que o fio faz.
//
//   bash tests/watchstate_envio.sh
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "addons.h"
#include "dados.h"
#include "perfis.h"
#include "rede.h"
#include "watchstate_envio.h"

static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %d\n", __LINE__); falhas++; } } while (0)

// ---- Rede falsa ------------------------------------------------------------
static pthread_mutex_t tr = PTHREAD_MUTEX_INITIALIZER;
static unsigned caps;
static int nA, nC, nD, nB, prazoOk = 1, postOk = 1, jsonOk = 1;
static int trocarDuranteEnvio;
static const char *usuario = "conta-a";
static char corpoA[1024], urlA[NV_ADDON_PEDIDO_MAX];

unsigned rede_pedido_capacidades(void) { return caps; }
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  int st = 204;
  memset(r, 0, sizeof *r);
  pthread_mutex_lock(&tr);
  if (!p->metodo || strcmp(p->metodo, "POST")) postOk = 0;
  if (p->prazo_ms != 15000) prazoOk = 0;
  if (!p->cabecalhos || !p->cabecalhos[0] || !strstr(p->cabecalhos[0], "application/json")) jsonOk = 0;
  if (strstr(p->url, "a.test") || strstr(p->url, "addon.test")) { nA++; snprintf(urlA, sizeof urlA, "%s", p->url);
                                  snprintf(corpoA, sizeof corpoA, "%.*s", (int)p->n_corpo, (const char *)p->corpo); }
  else if (strstr(p->url, "b.test")) nB++;
  else if (strstr(p->url, "c.test")) { nC++; st = 401; }
  else if (strstr(p->url, "d.test")) { nD++; st = 503; }
  pthread_mutex_unlock(&tr);
  if (st == 204 && (strstr(p->url, "a.test") || strstr(p->url, "addon.test"))) usleep(300000);   // addon lento
  if (trocarDuranteEnvio) usuario = "conta-b";
  r->erro = REDE_OK; r->status = st;
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

// ---- Disco, perfil e addons falsos ------------------------------------------
static char *disco;
int dados_gravar(const char *n, const char *c) { (void)n; pthread_mutex_lock(&tr); free(disco); disco = strdup(c); pthread_mutex_unlock(&tr); return 1; }
char *dados_ler(const char *n) { char *s; (void)n; pthread_mutex_lock(&tr); s = disco ? strdup(disco) : NULL; pthread_mutex_unlock(&tr); return s; }
int dados_apagar(const char *n) { (void)n; pthread_mutex_lock(&tr); free(disco); disco = NULL; pthread_mutex_unlock(&tr); return 1; }
static int perfil = 1, perfilAddons = 1;
int perfis_ativo(void) { return perfil; }
int perfis_ativo_addons(void) { return perfilAddons; }
const char *sessao_usuario(void) { return usuario; }

static const char *bases[] = {"http://a.test/x", "http://b.test/x", "http://c.test/x", "http://d.test/x"};
static int declWs[] = {1, 0, 1, 1};   // so quem declara watch_state (b.test nao)
static int nAddons = 4;
static int pacoteVazio;
static pthread_mutex_t listaTrava = PTHREAD_MUTEX_INITIALIZER;
void addons_lista_travar(void) { pthread_mutex_lock(&listaTrava); }
void addons_lista_destravar(void) { pthread_mutex_unlock(&listaTrava); }
// A lista da CONTA (o que o addonsRestaurar do sync aplica): [A], que declara.
static void restaurarConta(void) {
  bases[0] = "http://a.test/x"; declWs[0] = 1; nAddons = 1;
}
// O duble faz o que o addons_carregar DE VERDADE faz no arranque: a lista do
// pacote (art/addons.txt, a de exemplo) SUBSTITUI a que estava valendo. Era
// isto que o teste antigo nao reproduzia — e sem a troca o arranque nao perdia
// o diario de ninguem.
int addons_carregar(const char *dir) {
  (void)dir;
  nAddons = pacoteVazio ? 0 : 1;
  if (!pacoteVazio) { bases[0] = "http://b.test/x"; declWs[0] = 0; }
  return nAddons;
}
int addons_n(void) { return nAddons; }
int addons_ativo(int i) { return i >= 0 && i < nAddons; }
int addons_sondado(int i) { (void)i; return 1; }
const char *addons_base(int i) { return i >= 0 && i < nAddons ? bases[i] : ""; }
int addons_watchstate(int i, WsDecl *d) {
  memset(d, 0, sizeof *d);
  if (i < 0 || i >= nAddons || !declWs[i]) return 0;    // nao declara watch_state
  d->push = 1; d->eventos = WS_TODOS;
  return 1;
}

static WsEvento filme(int ev, long long pos) {
  WsEvento e;
  memset(&e, 0, sizeof e);
  e.ev = ev; strcpy(e.tipo, "movie");
  strcpy(e.metaId, "tt0133093"); strcpy(e.videoId, "tt0133093");
  e.atMs = 1757441718000LL; e.iniMs = 1757438900000LL; e.posMs = pos; e.durMs = 8160000;
  e.season = e.episode = -1; strcpy(e.imdb, "tt0133093");
  return e;
}

static void espera(int *n, int quer) {
  int i;
  for (i = 0; i < 200 && *n < quer; i++) usleep(10000);
}
static long long agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

// Gerado pelo runner a partir do trecho de main.c que carrega os addons.
void arranqueAddons(const char *dirArte);

static int testarArranque(int recusa) {
  WsEvento e = filme(WS_STOP, 3000);
  WsFila f = {0};
  char url[512], destino[20], id[WS_ID_MAX + 64], corpo[1024];
  long long agora = (long long)time(NULL);
  int i, preservado = 0;
  caps = REDE_CAP_CORPO;
  alarm(30);
  restaurarConta();   // o sync restaura a lista da conta ANTES do pacote
  OK(ws_url(bases[0], &e, url, sizeof url));
  OK(ws_id(&e, id, sizeof id));
  OK(ws_corpo(&e, corpo, sizeof corpo));
  ws_destino(bases[0], usuario, perfil, destino);
  OK(ws_fila_empurrar(&f, destino, url + strlen(bases[0]), id, corpo, agora - 60));
  f.it[0].proxS = agora - 1; f.it[0].tentativas = 1;
  disco = ws_fila_serializar(&f);
  ws_fila_limpar(&f);

  if (recusa == 1) pacoteVazio = 1;       // arranque sem lista nenhuma
  if (recusa == 2) usuario = "conta-b";  // diario de outra conta
  if (recusa == 3) perfil = perfilAddons = 2;
  if (recusa == 4) perfil = 2; // herda addons do principal
  arranqueAddons("art");   // o pacote aplica [B]; nenhum wsenvio_emitir
  arranqueAddons("art");   // iniciar de novo nao duplica a entrega
  usleep(500000);          // o fio tenta, nao acha o destino e segura
  pthread_mutex_lock(&tr);
  preservado = disco != NULL;
  OK(preservado);          // boot: o diario sobrevive a lista do pacote
  OK(nA == 0 && nB == 0 && nC == 0 && nD == 0);
  pthread_mutex_unlock(&tr);
  if (!recusa) {
    // A lista da conta volta (o ciclo do sync) e o pendente sai para A
    restaurarConta();
    wsenvio_podar();   // o sync poda contra a lista DELE: o pendente fica
    for (i = 0; i < 700; i++) {
      int vazio;
      pthread_mutex_lock(&tr); vazio = disco == NULL; pthread_mutex_unlock(&tr);
      if (vazio) break;
      usleep(10000);   // WS_AUSENTE_S: a reconferencia da lista
    }
    pthread_mutex_lock(&tr);
    OK(nA == 1 && nB == 0 && nC == 0 && nD == 0);
    OK(!strcmp(urlA, "http://a.test/x/watch_state/push/movie/tt0133093.json"));
    OK(strstr(corpoA, "\"event\":\"stop\"") && strstr(corpoA, "\"positionMs\":3000"));
    OK(disco == NULL);
    pthread_mutex_unlock(&tr);
  } else {
    usleep(200000);
    pthread_mutex_lock(&tr);
    // outra conta/perfil ou lista vazia: segurado, nunca entregue nem apagado
    OK(nA == 0 && nB == 0 && nC == 0 && nD == 0);
    OK(disco != NULL);
    pthread_mutex_unlock(&tr);
  }
  alarm(0);
  if (!falhas) printf("OK watchstate_arranque (pending_preserved=%d)\n", preservado);
  return falhas ? 1 : 0;
}

static int testarUrlLonga(void) {
  static char base[NV_ADDON_URL_MAX];
  static const char caminho[] = "/watch_state/push/movie/tt0133093.json";
  WsEvento e = filme(WS_STOP, 3000);
  size_t prefixo;
  int i;
  snprintf(base, sizeof base, "http://a.test/");
  prefixo = strlen(base);
  memset(base + prefixo, 'x', 1500 - prefixo); base[1500] = 0;
  bases[0] = base;
  caps = REDE_CAP_CORPO;
  alarm(8);
  wsenvio_emitir(&e);
  for (i = 0; i < 200; i++) {
    int entregue;
    pthread_mutex_lock(&tr); entregue = nA == 1; pthread_mutex_unlock(&tr);
    if (entregue) break;
    usleep(10000);
  }
  pthread_mutex_lock(&tr);
  OK(nA == 1 && nB == 0);
  OK(strlen(urlA) == 1500 + strlen(caminho));
  OK(!strncmp(urlA, base, 1500) && !strcmp(urlA + 1500, caminho));
  OK(strstr(corpoA, "\"event\":\"stop\"") && strstr(corpoA, "\"positionMs\":3000"));
  pthread_mutex_unlock(&tr);
  alarm(0);
  if (!falhas) printf("OK watchstate_url_longa\n");
  return falhas ? 1 : 0;
}

// Base com query: o caminho vai ANTES da query, senao a config do addon chega
// corrompida (https://addon.test/cfg?apikey=fixture/watch_state/...).
static int testarQuery(void) {
  static const char base[] = "https://addon.test/cfg?apikey=fixture";
  WsEvento e = filme(WS_STOP, 3000);
  int i;
  bases[0] = base;
  caps = REDE_CAP_CORPO;
  alarm(8);
  wsenvio_emitir(&e);
  for (i = 0; i < 200; i++) {
    int entregue;
    pthread_mutex_lock(&tr); entregue = nA == 1; pthread_mutex_unlock(&tr);
    if (entregue) break;
    usleep(10000);
  }
  pthread_mutex_lock(&tr);
  OK(nA == 1);
  OK(!strcmp(urlA, "https://addon.test/cfg/watch_state/push/movie/tt0133093.json?apikey=fixture"));
  pthread_mutex_unlock(&tr);
  alarm(0);
  if (!falhas) printf("OK watchstate_query\n");
  return falhas ? 1 : 0;
}

// Diario pendente de addon que a lista atual nao tem (o --sem-addon e o
// arranque com pacote vazio, la em testarArranque): nada sai e o diario
// espera — ausencia da lista de agora nao e motivo de descarte.

int main(int argc, char **argv) {
  WsEvento e;
  long long t0, dt;
  int a, c, d;
  char destinoC[20], destinoD[20];
  ws_destino(bases[2], usuario, perfil, destinoC);
  ws_destino(bases[3], usuario, perfil, destinoD);
  if (argc > 1 && !strcmp(argv[1], "--arranque")) return testarArranque(argc > 2 ? 1 : 0);
  if (argc > 1 && !strcmp(argv[1], "--outra-conta")) return testarArranque(2);
  if (argc > 1 && !strcmp(argv[1], "--outro-perfil")) return testarArranque(3);
  if (argc > 1 && !strcmp(argv[1], "--herdado")) return testarArranque(4);
  if (argc > 1 && !strcmp(argv[1], "--durante-envio")) {
    char destinoA[20];
    ws_destino(bases[0], usuario, perfil, destinoA);
    caps = REDE_CAP_CORPO; trocarDuranteEnvio = 1;
    e = filme(WS_START, 1000); wsenvio_emitir(&e);
    usleep(800000);
    pthread_mutex_lock(&tr);
    OK(nA == 1 && nB + nC + nD == 0);
    // O de A saiu (204). O que sobra e de destino da conta antiga: fica
    // segurado, sem POST — quem troca de conta o poda (definir_lista/esquecer).
    OK(!disco || !strstr(disco, destinoA));
    pthread_mutex_unlock(&tr);
    return falhas ? 1 : 0;
  }
  if (argc > 1 && !strcmp(argv[1], "--url-longa")) return testarUrlLonga();
  if (argc > 1 && !strcmp(argv[1], "--query")) return testarQuery();
  if (argc > 1 && !strcmp(argv[1], "--sem-addon")) return testarArranque(1);
  alarm(8);   // start -> pause -> stop deve terminar mesmo com 401/503

  // VIDAA (so GET): nada sai, nem o fio nasce
  caps = 0; e = filme(WS_START, 1000);
  wsenvio_emitir(&e);
  usleep(200000);
  OK(nA + nB + nC + nD == 0);

  // perfil que usa os addons de outro: nada sai
  caps = REDE_CAP_CORPO; perfil = 2; perfilAddons = 1;
  wsenvio_emitir(&e);
  usleep(200000);
  OK(nA + nB + nC + nD == 0);
  perfil = perfilAddons = 1;

  // evento invalido (sem videoId): nada sai
  e.videoId[0] = 0; wsenvio_emitir(&e); usleep(200000);
  OK(nA + nB + nC + nD == 0);

  // emitir nunca espera pela rede (a.test demora 300 ms por pedido)
  e = filme(WS_START, 1000);
  t0 = agoraMs(); wsenvio_emitir(&e); dt = agoraMs() - t0;
  OK(dt < 100);

  // corpo e URL certos so para quem declara; 401 e 503 tentam uma vez so
  espera(&nA, 1); espera(&nC, 1); espera(&nD, 1);
  usleep(600000);
  pthread_mutex_lock(&tr); a = nA; c = nC; d = nD;
  OK(nA == 1 && nB == 0 && nC == 1 && nD == 1);
  OK(!strcmp(urlA, "http://a.test/x/watch_state/push/movie/tt0133093.json"));
  OK(strstr(corpoA, "\"event\":\"start\"") && strstr(corpoA, "\"videoId\":\"tt0133093\"") &&
     strstr(corpoA, "\"positionMs\":1000") && strstr(corpoA, "\"durationMs\":8160000") &&
     strstr(corpoA, "\"scope\":\"movie\"") && strstr(corpoA, "\"at\":1757441718"));
  OK(prazoOk && postOk && jsonOk);
  // 401 (c) e 503 (d) ficam no diario para depois; o entregue (a) sai dele
  OK(disco && strstr(disco, destinoC) && strstr(disco, destinoD) && !strstr(disco, "http"));
  pthread_mutex_unlock(&tr);

  // segundo evento: a entrega continua; c (401) segue parado, d (backoff) espera
  e = filme(WS_PAUSE, 2000); wsenvio_emitir(&e);
  espera(&nA, 2);
  usleep(600000);
  pthread_mutex_lock(&tr);
  OK(nA == a + 1 && nC == c && nD == d && nB == 0);
  pthread_mutex_unlock(&tr);

  // terceiro evento: o backoff nao pode prender a trava de quem encerra
  e = filme(WS_STOP, 3000);
  t0 = agoraMs(); wsenvio_emitir(&e); dt = agoraMs() - t0;
  OK(dt < 100);
  espera(&nA, 3);
  usleep(600000);
  pthread_mutex_lock(&tr);
  OK(nA == a + 2 && nC == c && nD == d && nB == 0);
  OK(disco && strstr(disco, "|pause|2000|") && strstr(disco, "|stop|3000|"));
  pthread_mutex_unlock(&tr);

  alarm(0);
  if (falhas) { printf("%d FALHAS\n", falhas); return 1; }
  printf("OK watchstate_envio\n");
  return 0;
}
