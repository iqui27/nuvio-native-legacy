// #707 r3: recuo IPv4 com libcurl e sockets REAIS (dlopen, sem rede falsa).
// O recuo so vale quando a conexao TCP NAO abriu — nunca por limiar de tempo.
//
//   tls     — listener aceita TCP e trava o TLS: 1 conexao, sem recuo, volta
//             no prazo da base (GET e rede_fluxo64).
//   reuso   — conexao reusada que estoura (a 8.x zera CONNECT_TIME): sem recuo.
//   7 / 6   — connect recusado e nome que nao resolve: recuam.
//   28      — fila de accept cheia (SYN descartado, TCP nunca abre): recua, e o
//             cancelamento DURANTE a tentativa IPv4 volta em < 1 s.
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>

static const char *logPath;
static _Atomic int aceitas, modoOk, parar;

static unsigned long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long)t.tv_sec * 1000UL + (unsigned long)(t.tv_nsec / 1000000L);
}
static int ouvir(int fila, int *porta) {
  struct sockaddr_in a;
  socklen_t l = sizeof a;
  int fd = socket(AF_INET, SOCK_STREAM, 0), um = 1;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  assert(fd >= 0 && !bind(fd, (struct sockaddr *)&a, sizeof a) && !listen(fd, fila));
  assert(!getsockname(fd, (struct sockaddr *)&a, &l));
  *porta = ntohs(a.sin_port);
  return fd;
}
// Cada conexao: com modoOk responde ao 1o pedido e cala nos seguintes; sem
// ele nunca responde (o TLS do cliente fica parado no ClientHello).
static void *conexao(void *u) {
  int fd = (int)(intptr_t)u, n = 0;
  char b[4096];
  while (recv(fd, b, sizeof b, 0) > 0)
    if (modoOk && n++ == 0) {
      const char *r = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
      (void)send(fd, r, strlen(r), 0);
    }
  close(fd);
  return NULL;
}
static void *servidor(void *u) {
  int fd = (int)(intptr_t)u;
  for (;;) {
    pthread_t t;
    int c = accept(fd, NULL, NULL);
    if (c < 0) continue;
    aceitas++;
    if (pthread_create(&t, NULL, conexao, (void *)(intptr_t)c)) close(c);
    else pthread_detach(t);
  }
  return NULL;
}
// Quantas vezes o recuo foi anunciado no log do modulo (stdout -> arquivo).
static int recuos(void) {
  char l[512];
  int n = 0;
  FILE *f;
  fflush(stdout);
  f = fopen(logPath, "r");
  if (!f) return 0;
  while (fgets(l, sizeof l, f)) if (strstr(l, "de novo so em IPv4")) n++;
  fclose(f);
  return n;
}
static int pararSe(void *u) { (void)u; return parar; }
static int engole(void *o, const char *d, size_t n) { (void)o; (void)d; (void)n; return 1; }
static void *vigiaRecuo(void *u) {
  unsigned long *quando = u;
  int antes = recuos();
  while (recuos() == antes) usleep(10000);
  *quando = agoraMs();
  parar = 1;
  return NULL;
}

int main(int argc, char **argv) {
  char url[128], *r;
  int porta, st, fd, antes;
  unsigned long t0, ms;
  pthread_t srv;
  assert(argc == 2);
  logPath = argv[1];
  assert(freopen(logPath, "w", stdout));
  setvbuf(stdout, NULL, _IOLBF, 0);

  fd = ouvir(16, &porta);
  assert(!pthread_create(&srv, NULL, servidor, (void *)(intptr_t)fd));

  // TLS travado depois do TCP (loopback: connect em dezenas de us).
  snprintf(url, sizeof url, "https://127.0.0.1:%d/meta", porta);
  aceitas = 0; antes = recuos(); t0 = agoraMs();
  /* Prazo 2 s = connect 2 s (conexaoMs): o GET nao tem folga para repetir,
     entao a unica 2a conexao possivel seria o recuo. */
  r = rede_baixar_st(url, 2, NULL, &st);
  ms = agoraMs() - t0;
  fprintf(stderr, "tls GET: %lu ms, %d conexoes, recuos %d\n", ms, (int)aceitas, recuos() - antes);
  assert(!r && aceitas == 1 && recuos() == antes);
  assert(ms >= 1500 && ms < 3000);   /* base: connect+TLS no prazo de 2 s */

  { int64_t total, bytes; int cross; char fim[8192];
    aceitas = 0; antes = recuos(); t0 = agoraMs();
    st = rede_fluxo64(url, NULL, 0, engole, NULL, &total, &st, NULL, fim, &cross, 0, &bytes);
    ms = agoraMs() - t0;
    fprintf(stderr, "tls fluxo64: %lu ms, %d conexoes, recuos %d\n", ms, (int)aceitas, recuos() - antes);
    assert(st == -1 && aceitas == 1 && recuos() == antes);
    assert(ms >= 6000 && ms < 9500); }   /* base: 7,5 s */

  // Conexao reusada que estoura: a familia ja deu certo, nao e recuo.
  modoOk = 1;
  snprintf(url, sizeof url, "http://127.0.0.1:%d/a", porta);
  r = rede_baixar_st(url, 3, NULL, &st);
  assert(r && !strcmp(r, "ok"));
  free(r);
  antes = recuos();
  snprintf(url, sizeof url, "http://127.0.0.1:%d/b", porta);
  r = rede_baixar_st(url, 3, NULL, &st);
  fprintf(stderr, "reuso parado: recuos %d\n", recuos() - antes);
  /* O GET pode repetir em conexao NOVA (comportamento da base) e receber o
     corpo; o que importa aqui e nao ter recuado para IPv4. */
  assert(recuos() == antes);
  free(r);
  modoOk = 0;

  // Connect recusado (7) e nome que nao resolve (6): recuam.
  { int f2 = ouvir(1, &porta); close(f2); }
  snprintf(url, sizeof url, "http://127.0.0.1:%d/", porta);
  antes = recuos();
  r = rede_baixar_st(url, 3, NULL, &st);
  assert(!r && recuos() > antes);
  antes = recuos();
  r = rede_baixar_st("http://nuvio-707.invalid/", 3, NULL, &st);
  assert(!r && recuos() > antes);

  // 28 sem TCP: fila de accept cheia descarta o SYN. Recua, e o cancelamento
  // na tentativa IPv4 tem de voltar em < 1 s.
  { int cheia = ouvir(0, &porta), k;
    struct sockaddr_in a;
    pthread_t vg;
    unsigned long quando = 0;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(porta);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    for (k = 0; k < 3; k++) {
      int s = socket(AF_INET, SOCK_STREAM, 0);
      (void)fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
      (void)connect(s, (struct sockaddr *)&a, sizeof a);
    }
    usleep(100000);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/", porta);
    rede_parar_quando(pararSe, NULL);
    assert(!pthread_create(&vg, NULL, vigiaRecuo, &quando));
    r = rede_baixar_st(url, 8, NULL, &st);
    ms = agoraMs();
    pthread_join(vg, NULL);
    rede_parar_quando(NULL, NULL);
    fprintf(stderr, "28 sem TCP: recuou e cancelou em %lu ms\n", ms - quando);
    assert(!r && quando && ms - quando < 1000);
    close(cheia); }

  fprintf(stderr, "PASS: rede_ipv4_real (TLS travado sem recuo, reuso, 6/7/28 recuam, cancela < 1 s)\n");
  return 0;
}
