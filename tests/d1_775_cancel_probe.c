// #775: prova dirigida com rede e resolvedor REAIS (sem rede falsa).
// Servidor HTTP local em fio proprio; o transporte e a libcurl via dlopen.
//
//   normal   — /settings demora ~1,5 s a responder, resolve OK (P2P_OK),
//              GET seguinte no mesmo fio devolve corpo "ok".
//   cancel   — cancela p2p_resolver enquanto o servidor segura /settings:
//              P2P_ERR_CANCELADO em < 900 ms, GET seguinte com corpo.
//   warm     — aquece a conexao (GET /warm) e cancela no caminho de reuso.
//   retry    — 1a /settings cai sem resposta (retry em conexao nova); um
//              socket de outro consumidor ja ocupa um numero baixo. O
//              cancelamento tem de voltar em < 900 ms e o socket alheio NAO
//              pode receber EOF (a vigia antiga dava shutdown() nele).
//   cost     — 20 GETs rapidos com o parar instalado sem disparar: a media
//              nao pode superar a do GET sem parar em mais de 2 ms.
#include "p2p.h"
#include "rede.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

// ----------------------------------------------------------- servidor HTTP
static int srvFd;
static int srvPorta;
static _Atomic int srvFim;
static _Atomic int gotSettings;
static _Atomic int holdSettings;   // segura a resposta ate soltar
static _Atomic int dropFirst;      // proxima /settings: fecha sem responder
static _Atomic int sawDrop;
static _Atomic int slowSettings;   // responde depois de ~1,5 s
static FILE *logConns;
static pthread_mutex_t logTrava = PTHREAD_MUTEX_INITIALIZER;

static void responder(int fd, const char *corpo) {
  char resp[512];
  int m = snprintf(resp, sizeof resp,
    "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s",
    (int)strlen(corpo), corpo);
  (void)send(fd, resp, m, 0);
}

static void *srvConn(void *u);
static void *srvFio(void *u) {
  (void)u;
  while (!srvFim) {
    struct sockaddr_in a; socklen_t al = sizeof a;
    int fd = accept(srvFd, (struct sockaddr *)&a, &al);
    if (fd < 0) { usleep(1000); continue; }
    pthread_t t;
    int *ctx = malloc(2 * sizeof(int));
    ctx[0] = fd; ctx[1] = (int)ntohs(a.sin_port);
    if (pthread_create(&t, NULL, srvConn, ctx) == 0) pthread_detach(t);
    else { free(ctx); close(fd); }
  }
  return NULL;
}
static void *srvConn(void *u) {
  int fd = ((int *)u)[0], connPort = ((int *)u)[1];
  free(u);
  struct timeval tv = { 40, 0 };
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  while (!srvFim) {
    char req[2048];
    ssize_t n = recv(fd, req, sizeof req - 1, 0);
    if (n <= 0) break;
    req[n] = 0;
    char path[256] = "?";
    if (sscanf(req, "%*7s %255s", path) != 1) strcpy(path, "?");
    pthread_mutex_lock(&logTrava);
    fprintf(logConns, "('%s', %d), ", path, connPort); fflush(logConns);
    pthread_mutex_unlock(&logTrava);
    if (!strncmp(req, "POST", 4)) { responder(fd, "{\"files\":[{\"name\":\"v.mp4\",\"length\":10}]}"); continue; }
    if (strstr(path, "/settings")) {
      gotSettings = 1;
      if (dropFirst && !sawDrop) { sawDrop = 1; break; }      /* curl 52 -> retry */
      if (slowSettings) { struct timespec d = {1, 500*1000*1000}; nanosleep(&d, NULL); }
      while (holdSettings && !srvFim) usleep(10000);
      responder(fd, "{\"serverVersion\":\"4.21.2\"}");
      continue;
    }
    responder(fd, "ok");
  }
  close(fd);
  return NULL;
}

static int srvIniciar(void) {
  struct sockaddr_in a;
  srvFd = socket(AF_INET, SOCK_STREAM, 0);
  int um = 1; setsockopt(srvFd, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
  if (bind(srvFd, (struct sockaddr *)&a, sizeof a) || listen(srvFd, 16)) return 0;
  socklen_t al = sizeof a;
  getsockname(srvFd, (struct sockaddr *)&a, &al);
  srvPorta = (int)ntohs(a.sin_port);
  pthread_t t; if (pthread_create(&t, NULL, srvFio, NULL)) return 0;
  pthread_detach(t);
  return 1;
}

// ----------------------------------------------------------- stubs do app
static char srvBase[80];
static int ligado = 1;
int ajustes_p2p_ligado(void) { return ligado; }
const char *ajustes_p2p_url(void) { return srvBase; }
int ajustes_p2p_prazo_metadados_s(void) { return 30; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
uint64_t badges_bit(const char *id) { (void)id; return 0; }
int p2pmotor_disponivel(void) { return 0; }
const char *p2pmotor_motivo_indisponivel(void) { return "sem motor"; }
int p2pmotor_resumo(char *d, unsigned n) { (void)d; (void)n; return 0; }
int p2pmotor_resolver(const char *h, int i, const char *f, int t, int e,
                      char *u, unsigned n) {
  (void)h; (void)i; (void)f; (void)t; (void)e; (void)u; (void)n; return -1;
}
static int pararNunca(void *u) { (void)u; return 0; }

// ------------------------------------------------------------ cancelador
static _Atomic int cancelarArmado;
static void *cancelFio(void *u) {
  (void)u;
  while (!cancelarArmado) usleep(1000);
  while (!gotSettings) usleep(1000);
  usleep(20000);                       // deixa o pedido entrar no socket
  p2p_cancelar();
  return NULL;
}

static unsigned long agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long)t.tv_sec * 1000u + (unsigned)t.tv_nsec / 1000000u;
}

#define HASH "0123456789abcdef0123456789abcdef01234567"

static int getSame(int *corpoOk) {
  char u[120]; snprintf(u, sizeof u, "%s/same", srvBase);
  char *g = rede_baixar_st(u, 5, NULL, NULL);
  int ok = g && !strcmp(g, "ok");
  printf("GET /same status corpo=%s\n", g ? g : "(null)");
  free(g);
  if (corpoOk) *corpoOk = ok;
  return ok;
}

// Socket "de outro consumidor": par conectado (cliente/servidor local) cujo
// recv nao pode devolver 0 (EOF causado por shutdown alheio).
static int parAlheioVivo(int fd) {
  char lixo[8];
  ssize_t nr = recv(fd, lixo, sizeof lixo, MSG_DONTWAIT);
  printf("par_alheio recv=%zd errno=%d\n", nr, nr < 0 ? errno : 0);
  return nr != 0;
}
static int parAbrir(int *fd) {
  struct sockaddr_in a;
  *fd = socket(AF_INET, SOCK_STREAM, 0);
  if (*fd < 0) return 0;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons((uint16_t)srvPorta);
  // nao conecta ainda: connect() no fio do teste para segurar o fd baixo.
  return connect(*fd, (struct sockaddr *)&a, sizeof a) == 0;
}

static int cenario(const char *modo) {
  char url[300];
  unsigned long t0, dt;
  int r, ok;
  if (!strcmp(modo, "normal")) {
    slowSettings = 1;
    t0 = agoraMs();
    r = p2p_resolver(HASH, -1, NULL, url, sizeof url);
    dt = agoraMs() - t0;
    printf("modo=normal retorno=%d latencia_ms=%lu\n", r, dt);
    return r == P2P_OK && getSame(&ok);
  }
  if (!strcmp(modo, "cancel") || !strcmp(modo, "warm")) {
    if (!strcmp(modo, "warm")) {
      char u[120]; snprintf(u, sizeof u, "%s/warm", srvBase);
      char *g = rede_baixar_st(u, 5, NULL, NULL);
      printf("GET /warm corpo=%s\n", g ? g : "(null)");
      free(g);
    }
    gotSettings = 0; holdSettings = 1; cancelarArmado = 1;
    pthread_t t; pthread_create(&t, NULL, cancelFio, NULL); pthread_detach(t);
    t0 = agoraMs();
    r = p2p_resolver(HASH, -1, NULL, url, sizeof url);
    dt = agoraMs() - t0;
    holdSettings = 0;
    printf("modo=%s retorno=%d latencia_ms=%lu\n", modo, r, dt);
    if (r != P2P_ERR_CANCELADO || dt >= 900) return 0;
    return getSame(&ok);
  }
  if (!strcmp(modo, "retry")) {
    int par = -1;
    if (!parAbrir(&par)) printf("(sem par alheio)\n");
    dropFirst = 1; sawDrop = 0; gotSettings = 0; holdSettings = 1;
    cancelarArmado = 1;
    pthread_t t; pthread_create(&t, NULL, cancelFio, NULL); pthread_detach(t);
    t0 = agoraMs();
    r = p2p_resolver(HASH, -1, NULL, url, sizeof url);
    dt = agoraMs() - t0;
    holdSettings = 0;
    int vivo = par < 0 || parAlheioVivo(par);
    if (par >= 0) close(par);
    printf("modo=retry retorno=%d latencia_ms=%lu par_vivo=%d\n", r, dt, vivo);
    if (r != P2P_ERR_CANCELADO || dt >= 900 || !vivo) return 0;
    return getSame(&ok);
  }
  if (!strcmp(modo, "cost")) {
    char u[120]; snprintf(u, sizeof u, "%s/same", srvBase);
    int i; unsigned long d0 = 0, d1 = 0;
    for (i = 0; i < 20; i++) {
      t0 = agoraMs();
      char *g = rede_baixar_st(u, 5, NULL, NULL); free(g);
      d0 += agoraMs() - t0;
    }
    for (i = 0; i < 20; i++) {
      t0 = agoraMs();
      RedePararEstado ant = rede_parar_trocar(pararNunca, NULL);
      char *g = rede_baixar_st(u, 5, NULL, NULL); free(g);
      if (ant.instalado) rede_parar_trocar(ant.parar, ant.usuario);
      d1 += agoraMs() - t0;
    }
    printf("cost sem_parar=%.3f ms com_parar=%.3f ms\n", d0 / 20.0, d1 / 20.0);
    return d1 <= d0 + 40;   // media: +2 ms por GET
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *modo = argc > 1 ? argv[1] : "normal";
  const char *logp = getenv("PROBE_LOG");
  logConns = logp ? fopen(logp, "w") : stdout;
  if (!logConns) logConns = stdout;
  assert(srvIniciar());
  snprintf(srvBase, sizeof srvBase, "http://127.0.0.1:%d", srvPorta);
  rede_preparar();
  int rc = !cenario(modo);
  srvFim = 1;
  usleep(50000);
  close(srvFd);
  fprintf(logConns, " rc=%d\n", rc);
  fclose(logConns);
  return rc;
}
