// Ver pecaserv.h.
#include "pecaserv.h"
#include "peca.h"
#include <string.h>

#if defined(__EMSCRIPTEN__) || defined(_WIN32)
int  pecaserv_disponivel(void) { return 0; }
int  pecaserv_gerar_token(char *d, size_t n) { if (d && n) d[0] = 0; return 0; }
int  pecaserv_iniciar(const char *t, const char *h) { (void)t; (void)h; return 0; }
void pecaserv_parar(void) {}
int  pecaserv_no_ar(void) { return 0; }
const char *pecaserv_url(void) { return ""; }
int  pecaserv_visto_ha(void) { return -1; }
void pecaserv_estado(const char *j) { (void)j; }
int  pecaserv_pegar_pedido(char *d, size_t n) { (void)d; (void)n; return 0; }
int  pecaserv_pegar_chave(char *d, size_t n) { (void)d; (void)n; return 0; }
int  pecaserv_pegar_abrir(int *i) { (void)i; return 0; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define REQ_MAX (4096 + PECA_CORPO_MAX)

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t fio;
static int ouvir = -1, rodando, parar;
static char token[PECA_TOKEN_N + 1], host[48], url[96];
static char *pagina, *estado;
static char pedido[PECA_PEDIDO_MAX + 1], chave[96];
static int temPedido, temChave, abrir = -1;
static time_t visto, castigo;
static int erros;

int pecaserv_disponivel(void) { return 1; }

int pecaserv_gerar_token(char *dst, size_t n) {
  // 31 simbolos sem os que se confundem (0/o, 1/l): ~59 bits em 12.
  static const char ALFA[] = "abcdefghijkmnpqrstuvwxyz23456789";
  unsigned char b[PECA_TOKEN_N];
  FILE *f;
  size_t lidos;
  if (!dst || n < PECA_TOKEN_N + 1) return 0;
  f = fopen("/dev/urandom", "rb");
  if (!f) return 0;
  lidos = fread(b, 1, sizeof b, f);
  fclose(f);
  if (lidos != sizeof b) return 0;
  for (int i = 0; i < PECA_TOKEN_N; i++) dst[i] = ALFA[b[i] & 31];
  dst[PECA_TOKEN_N] = 0;
  memset(b, 0, sizeof b);
  return 1;
}

static int privado(in_addr_t a) {
  unsigned long h = ntohl(a);
  return (h >> 24) == 10 || (h >> 20) == 0xAC1 || (h >> 16) == 0xC0A8;
}
// A rota padrao diz qual interface a TV usa (connect em UDP nao manda pacote);
// sem ela, a primeira IPv4 privada de pe. So endereco privado: nunca publico.
static int ipLan(char *dst, size_t n) {
  struct sockaddr_in alvo, eu;
  socklen_t tam = sizeof eu;
  struct ifaddrs *lista = NULL, *i;
  in_addr_t achou = 0;
  int s;
#ifdef NV_PECA_TEST_LOOPBACK
  snprintf(dst, n, "127.0.0.1"); return 1;
#endif
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s >= 0) {
    memset(&alvo, 0, sizeof alvo);
    alvo.sin_family = AF_INET;
    alvo.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &alvo.sin_addr);
    if (!connect(s, (struct sockaddr *)&alvo, sizeof alvo) &&
        !getsockname(s, (struct sockaddr *)&eu, &tam) && privado(eu.sin_addr.s_addr))
      achou = eu.sin_addr.s_addr;
    close(s);
  }
  if (!achou && !getifaddrs(&lista)) {
    for (i = lista; i && !achou; i = i->ifa_next) {
      in_addr_t a;
      if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
      a = ((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr;
      if (privado(a)) achou = a;
    }
    freeifaddrs(lista);
  }
  if (!achou) return 0;
  eu.sin_addr.s_addr = achou;
  return inet_ntop(AF_INET, &eu.sin_addr, dst, (socklen_t)n) != NULL;
}

static int esperar(int fd, int ms) {
  fd_set r;
  struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
  FD_ZERO(&r); FD_SET(fd, &r);
  return select(fd + 1, &r, NULL, NULL, &tv) > 0;
}
static void enviar(int c, const char *b, size_t n) {
  while (n) {
    ssize_t k = send(c, b, n, MSG_NOSIGNAL);
    if (k <= 0) { if (k < 0 && errno == EINTR) continue; return; }
    b += k; n -= (size_t)k;
  }
}
static void responder(int c, const char *st, const char *tipo, const char *corpo) {
  char cab[320];
  size_t n = corpo ? strlen(corpo) : 0;
  int k = snprintf(cab, sizeof cab,
    "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
    "X-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"
    "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'\r\n"
    "Connection: close\r\n\r\n", st, tipo, n);
  enviar(c, cab, (size_t)k);
  if (n) enviar(c, corpo, n);
}

static void atender(int c) {
  char *req = malloc(REQ_MAX + 1);
  size_t n = 0;
  const char *corpo = NULL, *fim;
  size_t nc = 0;
  PecaRota r;
  char tk[PECA_TOKEN_N + 1], hs[48];
  if (!req) return;
  // Le ate ter cabecalhos e o corpo anunciado (ou estourar o prazo/tamanho).
  req[0] = 0;
  while (n < REQ_MAX && esperar(c, 3000)) {
    ssize_t k = recv(c, req + n, REQ_MAX - n, 0);
    if (k <= 0) break;
    n += (size_t)k; req[n] = 0;
    if (memchr(req, 0, n)) { n = 0; req[0] = 0; break; }   // binario: nao e pedido nosso
    if ((fim = strstr(req, "\r\n\r\n"))) {
      if (strncmp(req, "POST ", 5)) break;
      pthread_mutex_lock(&mtx);
      snprintf(tk, sizeof tk, "%s", token); snprintf(hs, sizeof hs, "%s", host);
      pthread_mutex_unlock(&mtx);
      r = peca_rota(req, tk, hs, &corpo, &nc);
      if (r != PECA_R_INVALIDO) break;        // completo, ou recusado por outro motivo
      if (n - (size_t)(fim + 4 - req) > PECA_CORPO_MAX) break;
    }
  }
  pthread_mutex_lock(&mtx);
  snprintf(tk, sizeof tk, "%s", token); snprintf(hs, sizeof hs, "%s", host);
  pthread_mutex_unlock(&mtx);
  r = peca_rota(req, tk, hs, &corpo, &nc);
  pthread_mutex_lock(&mtx);
  if (r == PECA_R_NADA && ++erros >= PECA_ERROS_MAX) {
    castigo = time(NULL) + PECA_CASTIGO_S; erros = 0;
    puts("[peca] muitos caminhos errados: porta calada por um tempo");
  }
  if (r >= PECA_R_PAGINA) visto = time(NULL);
  switch (r) {
    case PECA_R_PAGINA: {
      char *p = pagina ? strdup(pagina) : NULL;
      pthread_mutex_unlock(&mtx);
      responder(c, "200 OK", "text/html; charset=utf-8", p ? p : "");
      free(p);
      puts("[peca] pagina aberta");
      break; }
    case PECA_R_ESTADO: {
      char *p = estado ? strdup(estado) : NULL;
      pthread_mutex_unlock(&mtx);
      responder(c, "200 OK", "application/json", p ? p : "{}");
      free(p);
      break; }
    case PECA_R_PEDIR: {
      char b[PECA_CORPO_MAX + 1];
      memcpy(b, corpo, nc); b[nc] = 0;
      temPedido = peca_pedido_limpar(b, pedido, sizeof pedido) > 0;
      pthread_mutex_unlock(&mtx);
      printf("[peca] pedido recebido (%zu bytes)\n", nc);
      responder(c, temPedido ? "204 No Content" : "400 Bad Request", "text/plain", "");
      break; }
    case PECA_R_CHAVE: {
      char b[PECA_CORPO_MAX + 1];
      memcpy(b, corpo, nc); b[nc] = 0;
      peca_chave_limpar(b, chave, sizeof chave);
      temChave = 1;                         // vazia = esquecer a chave
      memset(b, 0, sizeof b);
      pthread_mutex_unlock(&mtx);
      puts("[peca] chave recebida");
      responder(c, "204 No Content", "text/plain", "");
      break; }
    case PECA_R_ABRIR:
      abrir = nc == 1 && corpo[0] >= '0' && corpo[0] < '0' + PECA_N ? corpo[0] - '0' : -1;
      pthread_mutex_unlock(&mtx);
      responder(c, "204 No Content", "text/plain", "");
      break;
    default:
      pthread_mutex_unlock(&mtx);
      responder(c, r == PECA_R_NEGADO ? "403 Forbidden" : r == PECA_R_GRANDE ? "413 Payload Too Large" :
                   r == PECA_R_NADA ? "404 Not Found" : "400 Bad Request", "text/plain", "");
      break;
  }
  memset(req, 0, REQ_MAX);
  free(req);
}

static void *laco(void *arg) {
  (void)arg;
  for (;;) {
    int c, calado, fd;
    pthread_mutex_lock(&mtx);
    fd = ouvir;
    calado = castigo && time(NULL) < castigo;
    if (parar) { pthread_mutex_unlock(&mtx); break; }
    pthread_mutex_unlock(&mtx);
    if (!esperar(fd, 250)) continue;
    c = accept(fd, NULL, NULL);
    if (c < 0) continue;
    if (calado) { close(c); continue; }
    atender(c);
    shutdown(c, SHUT_RDWR);
    close(c);
  }
  return NULL;
}

int pecaserv_no_ar(void) { return rodando; }
const char *pecaserv_url(void) { return rodando ? url : ""; }

int pecaserv_visto_ha(void) {
  int r;
  pthread_mutex_lock(&mtx);
  r = visto ? (int)(time(NULL) - visto) : -1;
  pthread_mutex_unlock(&mtx);
  return r;
}

void pecaserv_parar(void) {
  if (!rodando) return;
  pthread_mutex_lock(&mtx); parar = 1; pthread_mutex_unlock(&mtx);
  pthread_join(fio, NULL);
  close(ouvir); ouvir = -1;
  pthread_mutex_lock(&mtx);
  rodando = parar = 0;
  free(pagina); pagina = NULL; free(estado); estado = NULL;
  memset(token, 0, sizeof token); memset(pedido, 0, sizeof pedido); memset(chave, 0, sizeof chave);
  temPedido = temChave = 0; abrir = -1; visto = castigo = 0; erros = 0; url[0] = 0;
  pthread_mutex_unlock(&mtx);
  puts("[peca] servidor parado");
}

int pecaserv_iniciar(const char *t, const char *html) {
  struct sockaddr_in a;
  socklen_t tam = sizeof a;
  char ip[32];
  int um = 1, porta, s;
  if (!peca_token_valido(t) || !html) return 0;
  if (rodando) {   // troca de token ou de pagina (idioma): mesma porta
    pthread_mutex_lock(&mtx);
    if (strcmp(token, t)) { visto = 0; erros = 0; castigo = 0; }
    snprintf(token, sizeof token, "%s", t);
    free(pagina); pagina = strdup(html);
    snprintf(url, sizeof url, "http://%s/%s", host, token);
    pthread_mutex_unlock(&mtx);
    return 1;
  }
  if (!ipLan(ip, sizeof ip)) { puts("[peca] sem endereco de rede local"); return 0; }
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return 0;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons(PECASERV_PORTA);
  if (bind(s, (struct sockaddr *)&a, sizeof a)) {
    a.sin_port = 0;   // porta ocupada: a do sistema (o endereco muda ate reiniciar)
    if (bind(s, (struct sockaddr *)&a, sizeof a)) { close(s); return 0; }
  }
  if (listen(s, 8) || getsockname(s, (struct sockaddr *)&a, &tam)) { close(s); return 0; }
  fcntl(s, F_SETFD, FD_CLOEXEC);
  porta = ntohs(a.sin_port);
  pthread_mutex_lock(&mtx);
  ouvir = s; parar = 0;
  snprintf(token, sizeof token, "%s", t);
  snprintf(host, sizeof host, "%s:%d", ip, porta);
  snprintf(url, sizeof url, "http://%s/%s", host, token);
  free(pagina); pagina = strdup(html);
  pthread_mutex_unlock(&mtx);
  if (pthread_create(&fio, NULL, laco, NULL)) { close(s); ouvir = -1; return 0; }
  rodando = 1;
  printf("[peca] servidor no ar (porta %d)\n", porta);
  return 1;
}

void pecaserv_estado(const char *json) {
  pthread_mutex_lock(&mtx);
  free(estado); estado = json ? strdup(json) : NULL;
  pthread_mutex_unlock(&mtx);
}
int pecaserv_pegar_pedido(char *dst, size_t n) {
  int r;
  pthread_mutex_lock(&mtx);
  if ((r = temPedido)) { snprintf(dst, n, "%s", pedido); temPedido = 0; memset(pedido, 0, sizeof pedido); }
  pthread_mutex_unlock(&mtx);
  return r;
}
int pecaserv_pegar_chave(char *dst, size_t n) {
  int r;
  pthread_mutex_lock(&mtx);
  if ((r = temChave)) { snprintf(dst, n, "%s", chave); temChave = 0; memset(chave, 0, sizeof chave); }
  pthread_mutex_unlock(&mtx);
  return r;
}
int pecaserv_pegar_abrir(int *i) {
  int r;
  pthread_mutex_lock(&mtx);
  if ((r = abrir >= 0)) { *i = abrir; abrir = -1; }
  pthread_mutex_unlock(&mtx);
  return r;
}
#endif
