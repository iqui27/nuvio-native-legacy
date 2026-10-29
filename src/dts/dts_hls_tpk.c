// DTS / TrueHD -> Dolby Digital no .tpk da Samsung: o servidor HTTP local de
// onde o player da TV le o fluxo convertido. Ver dts_hls_tpk.h e
// docs/features/dts/README.md. O andaime e o do src/proxyts.c (127.0.0.1,
// porta livre, um fio por conexao, sessao por geracao); o conversor e o
// dts_remux.c.
//
// ENTREGA: HLS sob demanda (playlist VOD montada do indice de quadros-chave;
// cada segmento sai do conversor quando o player pede; seek, posicao e duracao
// ficam com o player). Sem indice ate o fim (MKV sem Cues) o app troca sozinho
// para um TS continuo, reaberto a cada seek (?t=). Corpo sempre chunked: o
// tamanho de um segmento so existe depois de gera-lo (Range e ignorado).
//
// REGISTRO: toda requisicao do player sai no log com o Range e o
// Connection dela, e a resposta com bytes, primeiro byte, tempo total e o
// gerador usado (continuou ou buscou). Nada de URL da fonte nem do token.
#include "dts_hls_tpk.h"
#include "dts_remux.h"
#include <stdio.h>
#include <string.h>

#ifndef NV_TPK
int dts_tpk_disponivel(void) { return 0; }
const char *dts_tpk_modo_nome(int m) { (void)m; return ""; }
int dts_tpk_abrir(const char *u, const char *h, int t, int s, int m, double i, char *o, size_t n) {
  (void)u; (void)h; (void)t; (void)s; (void)m; (void)i; if (o && n) o[0] = 0; return 0;
}
int dts_tpk_url_em(double s, char *o, size_t n) { (void)s; if (o && n) o[0] = 0; return 0; }
void dts_tpk_fechar(void) {}
int dts_tpk_estado(void) { return DTSTPK_NADA; }
const char *dts_tpk_erro(void) { return ""; }
double dts_tpk_duracao(void) { return 0; }
int dts_tpk_modo(void) { return -1; }
double dts_tpk_inicio_stream(void) { return -1; }
const char *dts_tpk_caminho(void) { return ""; }
int dts_tpk_sem_indice(void) { return 0; }
int dts_tpk_e_url(const char *u) { (void)u; return 0; }
#else

#include "dts_engine.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// Picked on the S95C (Tizen 9, Oct 2026): 2 s segments with a 1 s player
// prebuffer seek in about 2 s on a fast source; 6 s segments took 10-21 s.
#define SEG_ALVO_S       2.0           /* HLS: keyframes grouped to >= 2 s */
#define PREBUFFER_MS     1000          /* player prebuffer before it starts */
#define ESPERA_SONDA_MS  30000
#define ESPERA_GERADOR_MS 30000
#define GERADORES        2             /* each one is one more connection to the source */

static const char *const MODO_NOME[DTSTPK_MODOS] = { "HLS", "continuous TS", "off" };
const char *dts_tpk_modo_nome(int m) { return m >= 0 && m < DTSTPK_MODOS ? MODO_NOME[m] : "?"; }

static uint64_t agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}
static void prazoEm(struct timespec *ts, int ms) {
  clock_gettime(CLOCK_REALTIME, ts);
  ts->tv_sec += ms / 1000; ts->tv_nsec += (long)(ms % 1000) * 1000000L;
  if (ts->tv_nsec >= 1000000000L) { ts->tv_nsec -= 1000000000L; ts->tv_sec++; }
}

// --- token e IP -------------------------------------------------------------
static void gerarToken(char *t, int n) {
  static const char A[] = "abcdefghjkmnpqrstuvwxyz23456789";
  unsigned char b[32];
  int f = open("/dev/urandom", O_RDONLY), k = 0;
  ssize_t r = f >= 0 ? read(f, b, sizeof b) : -1;
  if (f >= 0) close(f);
  if (r < (ssize_t)sizeof b) {   // sem urandom: relogio (o token so separa sessoes)
    uint64_t x = agoraMs() * 6364136223846793005ULL + (uint64_t)getpid();
    for (int i = 0; i < (int)sizeof b; i++) { x = x * 6364136223846793005ULL + 1442695040888963407ULL; b[i] = (unsigned char)(x >> 33); }
  }
  for (int i = 0; i < (int)sizeof b && k < n; i++) if (b[i] < 248) t[k++] = A[b[i] % 31];
  t[k] = 0;
}

// --- sockets ------------------------------------------------------------------
static int enviarTudo(int s, const void *b, size_t n, volatile int *parar1, volatile int *parar2) {
  const char *p = b;
  while (n) {
    ssize_t r = send(s, p, n, MSG_NOSIGNAL);
    if (r < 0 && (errno == EINTR)) continue;
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // Player parado (pausa, buffer cheio): espera sem prazo, so o cancelamento solta.
      if ((parar1 && *parar1) || (parar2 && *parar2)) return 0;
      continue;
    }
    if (r <= 0) return 0;
    p += r; n -= (size_t)r;
  }
  return 1;
}

static int ouvir(in_addr_t onde, int *porta) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  int um = 1, s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return -1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(onde);
  if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }   // porta 0: livre
  if (listen(s, 16) < 0 || getsockname(s, (struct sockaddr *)&a, &al) < 0) { close(s); return -1; }
  *porta = ntohs(a.sin_port);
  return s;
}

// Fim do cabecalho (depois da linha vazia), 0 = ainda nao chegou.
static size_t fimCabecalho(const char *b) {
  const char *a = strstr(b, "\r\n\r\n"), *z = strstr(b, "\n\n");
  if (a && (!z || a < z)) return (size_t)(a - b) + 4;
  return z ? (size_t)(z - b) + 2 : 0;
}

// Le o cabecalho do pedido (ate a linha vazia); b ja traz `ja` bytes (o que o
// porteiro leu, ou o que sobrou do pedido anterior). 0 = conexao fechada/prazo.
static int lerPedido(int s, char *b, size_t n, int prazoMs, size_t ja) {
  size_t tot = ja;
  uint64_t fim = agoraMs() + (uint64_t)prazoMs;
  b[tot] = 0;
  if (fimCabecalho(b)) return (int)tot;
  while (tot < n - 1) {
    struct pollfd p = { s, POLLIN, 0 };
    int restam = (int)(fim > agoraMs() ? fim - agoraMs() : 0);
    if (restam <= 0 || poll(&p, 1, restam) <= 0) return 0;
    ssize_t r = recv(s, b + tot, n - 1 - tot, 0);
    if (r <= 0) return 0;
    tot += (size_t)r; b[tot] = 0;
    if (strstr(b, "\r\n\r\n") || strstr(b, "\n\n")) return (int)tot;
  }
  return 0;
}

typedef struct {
  char metodo[8], caminho[400], query[128], range[64], conexao[32], agente[96];
  int http11, head;
} Pedido;

static void campo(const char *cab, const char *nome, char *dst, size_t n) {
  const char *p = cab;
  size_t k = strlen(nome);
  dst[0] = 0;
  while ((p = strchr(p, '\n')) != NULL) {
    p++;
    if (!strncasecmp(p, nome, k) && p[k] == ':') {
      const char *v = p + k + 1, *f;
      while (*v == ' ' || *v == '\t') v++;
      f = v + strcspn(v, "\r\n");
      snprintf(dst, n, "%.*s", (int)(f - v), v);
      return;
    }
  }
}

static int lerLinha(const char *cab, Pedido *p) {
  char alvo[400];
  memset(p, 0, sizeof *p);
  if (sscanf(cab, "%7s %399s", p->metodo, alvo) != 2) return 0;
  p->http11 = strstr(cab, "HTTP/1.1\r") != NULL || strstr(cab, "HTTP/1.1\n") != NULL;
  p->head = !strcmp(p->metodo, "HEAD");
  { char *q = strchr(alvo, '?');
    if (q) { snprintf(p->query, sizeof p->query, "%s", q + 1); *q = 0; }
    snprintf(p->caminho, sizeof p->caminho, "%s", alvo); }
  campo(cab, "Range", p->range, sizeof p->range);
  campo(cab, "Connection", p->conexao, sizeof p->conexao);
  campo(cab, "User-Agent", p->agente, sizeof p->agente);
  return !strcmp(p->metodo, "GET") || p->head;
}

static void responderSimples(int s, int cod, const char *msg, const char *tipo, const char *corpo, int manter);

// --- porteiro ---------------------------------------------------------------
// Review blocker 3: no thread per connection before its token is checked. One
// thread per listener holds the new connections (up to PORT_PEND, the oldest
// dropped when full, PORT_PRAZO_MS each to send the request head), reads the
// head without blocking, and only a head with the live token gets a thread,
// up to maxVivos at once. Anything else is answered (404/503) and closed here.
#define PORT_VIVOS 8
#define PORT_PEND 16
#define PORT_PRAZO_MS 5000
#define CAB_MAX 8192
typedef struct Porteiro Porteiro;
typedef struct { Porteiro *pt; int sock; size_t n; char cab[CAB_MAX]; } Conexao;
struct Porteiro {
  int ls, maxVivos;
  int (*valido)(const char *cab);   // the head carries a live token
  void *(*fio)(void *);             // takes the Conexao, frees it, calls conexaoFim
  atomic_int vivos;
  pthread_mutex_t m;
  Conexao *conexoes[PORT_VIVOS];
  unsigned recusados;
};
static void conexaoFim(Conexao *c) {
  Porteiro *pt = c->pt;
  pthread_mutex_lock(&pt->m);
  for (int i = 0; i < pt->maxVivos; i++) if (pt->conexoes[i] == c) {
    pt->conexoes[i] = NULL;
    atomic_fetch_sub(&pt->vivos, 1);
    break;
  }
  close(c->sock);
  pthread_mutex_unlock(&pt->m);
  free(c);
}
static int tokenNoCaminho(const char *cab, const char *tok) {   // "GET /<tok>/..." or "/<tok>"
  const char *p = strchr(cab, ' ');
  size_t k = strlen(tok);
  if (!p || !k) return 0;
  while (*p == ' ') p++;
  if (*p++ != '/') return 0;
  return !strncmp(p, tok, k) && (p[k] == '/' || p[k] == ' ' || p[k] == '?');
}
static void entregar(Porteiro *pt, int s, char *b, size_t n) {
  pthread_t t;
  Conexao *c;
  pthread_mutex_lock(&pt->m);
  if (!pt->valido(b)) {
    pthread_mutex_unlock(&pt->m);
    if (!(pt->recusados++ % 64)) { printf("[dts-tpk] pedido sem token valido: 404, sem fio (%u)\n", pt->recusados); fflush(stdout); }
    responderSimples(s, 404, "Not Found", NULL, "", 0);
    close(s); return;
  }
  if (atomic_load(&pt->vivos) >= pt->maxVivos || !(c = malloc(sizeof *c))) {
    pthread_mutex_unlock(&pt->m);
    printf("[dts-tpk] %d conexoes abertas: 503\n", pt->maxVivos);
    fflush(stdout);
    responderSimples(s, 503, "Service Unavailable", NULL, "busy\n", 0);
    close(s); return;
  }
  c->pt = pt; c->sock = s; c->n = n;
  memcpy(c->cab, b, n + 1);
  for (int i = 0; i < pt->maxVivos; i++) if (!pt->conexoes[i]) { pt->conexoes[i] = c; break; }
  atomic_fetch_add(&pt->vivos, 1);
  pthread_mutex_unlock(&pt->m);
  if (pthread_create(&t, NULL, pt->fio, c) == 0) pthread_detach(t);
  else conexaoFim(c);
}
static void *porteiro(void *u) {
  Porteiro *pt = u;
  struct { int s; size_t n; uint64_t fim; char *b; } pd[PORT_PEND];
  int np = 0;
  for (;;) {
    struct pollfd f[PORT_PEND + 1];
    uint64_t t = agoraMs();
    int espera = 1000, i;
    f[0].fd = pt->ls; f[0].events = POLLIN; f[0].revents = 0;
    for (i = 0; i < np; i++) {
      int r = pd[i].fim > t ? (int)(pd[i].fim - t) : 0;
      f[i + 1].fd = pd[i].s; f[i + 1].events = POLLIN; f[i + 1].revents = 0;
      if (r < espera) espera = r;
    }
    if (poll(f, (nfds_t)np + 1, espera) < 0 && errno != EINTR) { usleep(200 * 1000); continue; }
    t = agoraMs();
    for (i = np - 1; i >= 0; i--) {   // from the end: removal swaps in one already seen
      int fim = 0, pronto = 0;
      if (f[i + 1].revents) {
        ssize_t k = recv(pd[i].s, pd[i].b + pd[i].n, CAB_MAX - 1 - pd[i].n, MSG_DONTWAIT);
        if (k > 0) {
          pd[i].n += (size_t)k; pd[i].b[pd[i].n] = 0;
          if (fimCabecalho(pd[i].b)) pronto = 1; else if (pd[i].n >= CAB_MAX - 1) fim = 1;
        } else if (k == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) fim = 1;
      }
      if (!pronto && !fim && t >= pd[i].fim) fim = 1;
      if (pronto) entregar(pt, pd[i].s, pd[i].b, pd[i].n);
      else if (fim) close(pd[i].s);
      if (pronto || fim) { free(pd[i].b); pd[i] = pd[--np]; }
    }
    if (f[0].revents & POLLIN) {
      int s = accept(pt->ls, NULL, NULL);
      if (s < 0) continue;
      if (np == PORT_PEND) { close(pd[0].s); free(pd[0].b); pd[0] = pd[--np]; }   // the oldest waits no more
      { struct timeval tv = { 1, 0 };   // envio em fatias de 1 s: o cancelamento e checado entre elas
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv); }
#ifdef SO_NOSIGPIPE
      { int um = 1; setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &um, sizeof um); }
#endif
      if (!(pd[np].b = malloc(CAB_MAX))) { close(s); continue; }
      pd[np].s = s; pd[np].n = 0; pd[np].b[0] = 0; pd[np].fim = t + PORT_PRAZO_MS;
      np++;
    }
  }
  return NULL;
}
static int porteiroSubir(Porteiro *pt, int ls, int maxVivos, int (*valido)(const char *), void *(*fio)(void *)) {
  pthread_t t;
  pt->ls = ls; pt->maxVivos = maxVivos; pt->valido = valido; pt->fio = fio;
  pthread_mutex_init(&pt->m, NULL);
  atomic_store(&pt->vivos, 0);
  if (pthread_create(&t, NULL, porteiro, pt) != 0) return 0;
  pthread_detach(t);
  return 1;
}

static void responderSimples(int s, int cod, const char *msg, const char *tipo, const char *corpo, int manter) {
  char cab[512];
  size_t n = corpo ? strlen(corpo) : 0;
  snprintf(cab, sizeof cab, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
           "Cache-Control: no-cache\r\nConnection: %s\r\n\r\n", cod, msg, tipo ? tipo : "text/plain", n,
           manter ? "keep-alive" : "close");
  enviarTudo(s, cab, strlen(cab), NULL, NULL);
  if (n) enviarTudo(s, corpo, n, NULL, NULL);
}

// --- sessao -----------------------------------------------------------------
typedef struct {
  DtsRemuxGen *g;
  int ocupado, seg;      // seg em producao (-2 = progressivo), quando ocupado
  volatile int parar;
  unsigned uso;
} Gerador;

typedef struct Sessao {
  unsigned id;
  char token[12];
  char *url, *cabs;
  int track, stereo, modo;
  volatile int cancelar;
  pthread_mutex_t m;
  pthread_cond_t c;
  int refs;                       // sob a trava global
  int estado, semIndice;
  char erro[160], caminho[40];
  double duracao;
  DtsRemux *rx;
  DtsRemux *rxSonda;              // while the probe runs: what a close cancels
  Gerador ger[GERADORES];
  unsigned uso;
  double inicioStream;            // progressivo: keyframe do fluxo em curso
  // resumo
  unsigned nPedidos, nSegs, nContinuou, nBuscou, nErros;
  int64_t bytes;
  uint64_t criadaMs, prontaMs, primeiroPedidoMs;
  char agente[96];
  // memory line
  long rssIni, rssPico, livreIni, livreMin;   // kB; -1 unknown
  int hwmZerado;
  uint64_t memLogMs;
} Sessao;

static pthread_mutex_t G = PTHREAD_MUTEX_INITIALIZER;
static Sessao *atual;
static unsigned contador;
static int sockLo = -1, portaLo;

static void sessaoDestruir(Sessao *s) {
  for (int i = 0; i < GERADORES; i++) dts_remux_gen_free(s->ger[i].g);
  dts_remux_close(s->rx);
  free(s->url); free(s->cabs);
  pthread_cond_destroy(&s->c); pthread_mutex_destroy(&s->m);
  free(s);
}
// Fechar geradores junta fios de rede (curl cancelado responde rapido, mas um
// DNS preso nao): sempre fora do fio de quem soltou a ultima referencia.
static void *fioDestruir(void *u) { sessaoDestruir(u); return NULL; }
static void sessaoSoltar(Sessao *s) {
  int fim;
  pthread_mutex_lock(&G);
  fim = --s->refs == 0;
  pthread_mutex_unlock(&G);
  if (fim) {
    pthread_t t;
    if (pthread_create(&t, NULL, fioDestruir, s) == 0) pthread_detach(t);
    else sessaoDestruir(s);
  }
}
static Sessao *sessaoPegar(const char *token) {
  Sessao *s = NULL;
  pthread_mutex_lock(&G);
  if (atual && token && !strcmp(atual->token, token)) { s = atual; s->refs++; }
  pthread_mutex_unlock(&G);
  return s;
}

static void sessaoResumo(Sessao *s) {
  printf("[dts-tpk] sessao %u fim: %u pedido(s), %u segmento(s) (%u continuaram, %u buscaram), "
         "%u erro(s), %.1f MB enviados%s%s\n", s->id, s->nPedidos, s->nSegs, s->nContinuou, s->nBuscou,
         s->nErros, s->bytes / 1048576.0, s->agente[0] ? "; player: " : "", s->agente);
  fflush(stdout);
}

static void *fioSonda(void *u) {
  Sessao *s = u;
  char err[160] = "";
  // Two steps so a close during the probe (stop/back while the source is still
  // answering) reaches its reader: encerrarAtual cancels rxSonda.
  DtsRemux *rx = dts_remux_new(&s->cancelar);
  pthread_mutex_lock(&s->m);
  s->rxSonda = rx;
  if (rx && s->cancelar) dts_remux_cancel(rx);
  pthread_mutex_unlock(&s->m);
  if (rx && dts_remux_probe(rx, s->url, s->cabs, s->track, s->stereo, SEG_ALVO_S, err, sizeof err) < 0) {
    pthread_mutex_lock(&s->m); s->rxSonda = NULL; pthread_mutex_unlock(&s->m);
    dts_remux_close(rx);
    rx = NULL;
  }
  if (!rx && !err[0]) snprintf(err, sizeof err, "Out of memory");
  const DtsRemuxInfo *in = rx ? dts_remux_info(rx) : NULL;
  pthread_mutex_lock(&s->m);
  s->rxSonda = NULL;
  if (s->cancelar) { if (rx) dts_remux_close(rx); rx = NULL; s->estado = DTSTPK_FALHOU; snprintf(s->erro, sizeof s->erro, "cancelled"); }
  else if (!rx) { s->estado = DTSTPK_FALHOU; snprintf(s->erro, sizeof s->erro, "%s", err[0] ? err : "probe failed"); }
  else if (s->modo == DTSTPK_HLS && (in->n_segments < 1 || !in->index_complete)) {
    // HLS precisa do indice inteiro (a playlist sai dele): sem Cues nao ha HLS.
    // video_tpk.c refaz no TS continuo, que busca por tempo.
    s->estado = DTSTPK_FALHOU; s->semIndice = 1;
    snprintf(s->erro, sizeof s->erro, "no keyframe index to the end (no Cues): HLS impossible");
  } else {
    s->rx = rx; s->estado = DTSTPK_PRONTO; s->duracao = in->duration_s;
    snprintf(s->caminho, sizeof s->caminho, "%s", in->decoder_path);
  }
  s->prontaMs = agoraMs();
  pthread_cond_broadcast(&s->c);
  pthread_mutex_unlock(&s->m);
  if (rx) {
    char fpsTxt[24] = "?";
    if (in->fps > 0) { long f = lround(in->fps * 1000.0); snprintf(fpsTxt, sizeof fpsTxt, "%ld.%03ld", f / 1000, f % 1000); }
    char dvTxt[40] = "nao";
    if (in->dv_profile >= 0) snprintf(dvTxt, sizeof dvTxt, "perfil %d.%d%s", in->dv_profile, in->dv_compat, in->dv_el ? " com camada EL" : "");
    printf("[dts-tpk] sessao %u fonte: %s %dx%d %s fps, dv=%s hdr(container)=%s, audio %s (audio %d do arquivo) %d ch %d Hz -> %s %d ch %d kb/s "
           "(%s); %d quadro(s)-chave, %d segmento(s) de ate %.1f s, %.0f s, %.0f MB; sonda %u ms\n",
           s->id, in->video_codec, in->width, in->height, fpsTxt, dvTxt, in->hdr, in->audio_codec, in->audio_ordinal, in->in_channels,
           in->in_rate, in->out_codec, in->out_channels, in->out_bitrate / 1000, in->decoder_path, in->n_keyframes,
           in->n_segments, in->max_segment_s, in->duration_s, in->size_bytes > 0 ? in->size_bytes / 1048576.0 : -1.0,
           (unsigned)(s->prontaMs - s->criadaMs));
    if (s->semIndice) {
      printf("[dts-tpk] sessao %u: %s: o app troca para o TS continuo\n", s->id, s->erro);
      dts_remux_close(rx);             // a sessao nao ficou com ele
    }
  } else if (!s->cancelar)
    printf("[dts-tpk] sessao %u: a fonte nao abriu no conversor: %s (%u ms)\n", s->id, s->erro,
           (unsigned)(s->prontaMs - s->criadaMs));
  fflush(stdout);
  sessaoSoltar(s);
  return NULL;
}

// Espera a sonda. 1 = pronta; 0 = falhou/cancelada (com resposta enviada).
static int esperarPronta(Sessao *s, int sock, int manter) {
  struct timespec ts;
  prazoEm(&ts, ESPERA_SONDA_MS);
  pthread_mutex_lock(&s->m);
  while (s->estado == DTSTPK_SONDANDO && !s->cancelar)
    if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) break;
  // HLS sem indice: o app refaz no TS continuo e fecha esta sessao. O player
  // fica sem resposta ate la (um 503 viraria erro do player, que chega depois
  // da troca e derrubaria o fluxo novo).
  if (s->semIndice) {
    prazoEm(&ts, 15000);
    while (!s->cancelar)
      if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) break;
  }
  int ok = s->estado == DTSTPK_PRONTO && !s->cancelar, calar = s->semIndice && s->cancelar;
  pthread_mutex_unlock(&s->m);
  if (!ok && !calar) responderSimples(sock, 503, "Service Unavailable", NULL, "converter not ready\n", manter);
  return ok;
}

// --- geradores ----------------------------------------------------------------
// Ordem (design 4.1): (1) um ocioso que CONTINUA em `seg`; (2) se outro esta
// fazendo seg-1, espera por ele (o player pediu dois em paralelo: continuar
// custa nada, buscar baixaria o mesmo trecho duas vezes); (3) um ocioso
// qualquer (busca); (4) um novo, ate GERADORES; (5) para o mais antigo.
static Gerador *pegarGerador(Sessao *s, int seg, int *continua, char *err, size_t errn) {
  struct timespec ts;
  Gerador *esc = NULL;
  int i;
  *continua = 0;
  prazoEm(&ts, ESPERA_GERADOR_MS);
  pthread_mutex_lock(&s->m);
  while (!esc && !s->cancelar) {
    int esperar = 0, vaga = -1;
    for (i = 0; i < GERADORES && seg >= 0; i++)
      if (s->ger[i].g && !s->ger[i].ocupado && dts_remux_gen_next(s->ger[i].g) == seg) { esc = &s->ger[i]; *continua = 1; break; }
    if (esc) break;
    for (i = 0; i < GERADORES && seg > 0; i++)
      if (s->ger[i].ocupado && s->ger[i].seg == seg - 1 && !s->ger[i].parar) esperar = 1;
    if (esperar) {
      if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) { snprintf(err, errn, "timed out waiting for segment %d", seg - 1); break; }
      continue;
    }
    for (i = 0; i < GERADORES; i++)
      if (s->ger[i].g && !s->ger[i].ocupado && (!esc || s->ger[i].uso < esc->uso)) esc = &s->ger[i];
    if (esc) break;
    // Progressivo: o player le UM fluxo por vez. Pedido novo (seek, ou o player
    // reconectando) com o anterior ainda ocupado: o anterior para e este fica
    // com o leitor dele, em vez de abrir outro (cada leitor novo e outra
    // conexao na fonte e reler o cabecalho; foi isso que o TorBox recusou).
    if (seg < 0) {
      Gerador *prog = NULL;
      for (i = 0; i < GERADORES; i++) if (s->ger[i].g && s->ger[i].ocupado && s->ger[i].seg == -2) prog = &s->ger[i];
      if (prog) {
        if (!prog->parar) {
          prog->parar = 1;
          dts_remux_gen_interrupt(prog->g);
          printf("[dts-tpk] sessao %u: fluxo anterior para; o pedido novo usa o mesmo leitor\n", s->id);
          fflush(stdout);
        }
        if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) { snprintf(err, errn, "previous stream did not stop"); break; }
        continue;
      }
    }
    for (i = 0; i < GERADORES; i++) if (!s->ger[i].g && !s->ger[i].ocupado) { vaga = i; break; }
    if (vaga >= 0) {
      Gerador *nv = &s->ger[vaga];
      DtsRemuxGen *g;
      nv->ocupado = 1; nv->seg = seg;      // reserva a vaga enquanto abre a fonte
      pthread_mutex_unlock(&s->m);
      g = dts_remux_gen_new(s->rx, err, errn);
      pthread_mutex_lock(&s->m);
      nv->g = g; nv->ocupado = 0;
      pthread_cond_broadcast(&s->c);
      if (!g) break;
      printf("[dts-tpk] sessao %u: gerador %d pronto\n", s->id, vaga);
      fflush(stdout);
      esc = nv;
      break;
    }
    // Todos ocupados: o mais antigo larga o que faz (o player desistiu dele).
    { Gerador *velho = NULL;
      for (i = 0; i < GERADORES; i++)
        if (s->ger[i].ocupado && s->ger[i].g && (!velho || s->ger[i].uso < velho->uso)) velho = &s->ger[i];
      if (velho && !velho->parar) {
        velho->parar = 1;
        dts_remux_gen_interrupt(velho->g);   // preso na rede: solta ja, nao no proximo pacote
        printf("[dts-tpk] sessao %u: %d geradores ocupados, o mais antigo (seg %d) para\n", s->id, GERADORES, velho->seg);
        fflush(stdout);
      } }
    if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) { snprintf(err, errn, "no free generator"); break; }
  }
  if (esc && !s->cancelar) { esc->ocupado = 1; esc->seg = seg; esc->parar = 0; esc->uso = ++s->uso; }
  else esc = NULL;
  pthread_mutex_unlock(&s->m);
  if (!esc && !err[0]) snprintf(err, errn, "session closed");
  return esc;
}

static void soltarGerador(Sessao *s, Gerador *g) {
  pthread_mutex_lock(&s->m);
  g->ocupado = 0; g->seg = -1; g->parar = 0;
  pthread_cond_broadcast(&s->c);
  pthread_mutex_unlock(&s->m);
}

// --- saida de um run ------------------------------------------------------------
typedef struct {
  int sock, manter, cabEnviado, status;
  const char *tipo;
  Sessao *s;
  Gerador *ger;
  int64_t bytes;
  uint64_t t0, primeiroByte;
} Saida;

static int enviarCab(Saida *o) {
  char cab[400];
  snprintf(cab, sizeof cab, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\n"
           "Cache-Control: no-cache\r\nConnection: %s\r\n\r\n", o->tipo, o->manter ? "keep-alive" : "close");
  o->cabEnviado = 1; o->status = 200;
  return enviarTudo(o->sock, cab, strlen(cab), &o->s->cancelar, &o->ger->parar);
}

static int saidaEscrever(void *u, const unsigned char *d, int n) {
  Saida *o = u;
  char tam[16];
  int k;
  if (!o->primeiroByte) o->primeiroByte = agoraMs();
  if (!o->cabEnviado && !enviarCab(o)) return -1;
  k = snprintf(tam, sizeof tam, "%x\r\n", (unsigned)n);
  if (!enviarTudo(o->sock, tam, (size_t)k, &o->s->cancelar, &o->ger->parar) ||
      !enviarTudo(o->sock, d, (size_t)n, &o->s->cancelar, &o->ger->parar) ||
      !enviarTudo(o->sock, "\r\n", 2, &o->s->cancelar, &o->ger->parar)) return -1;
  o->bytes += n;
  return n;
}


// Um segmento (seg >= 0) ou o fluxo progressivo (seg < 0, a partir de `t`).
// Devolve se a conexao pode seguir aberta.
static int servirFluxo(int sock, Sessao *s, const Pedido *pd, unsigned nr, int seg, double t, const char *fmt,
                       const char *tipo, int manter) {
  char err[160] = "";
  int continua = 0, rr;
  uint64_t t0 = agoraMs();
  DtsRemuxStats st;
  Saida o;
  memset(&st, 0, sizeof st);
  if (pd->head) {
    // HEAD: so o cabecalho; o tamanho de um segmento nao existe antes de gera-lo.
    char cab[300];
    snprintf(cab, sizeof cab, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nCache-Control: no-cache\r\nConnection: %s\r\n\r\n",
             tipo, manter ? "keep-alive" : "close");
    enviarTudo(sock, cab, strlen(cab), &s->cancelar, NULL);
    printf("[dts-tpk] #%u <- 200 HEAD (sem corpo)\n", nr);
    fflush(stdout);
    return manter;
  }
  Gerador *ger = pegarGerador(s, seg < 0 ? -2 : seg, &continua, err, sizeof err);
  if (!ger) {
    responderSimples(sock, 503, "Service Unavailable", NULL, "busy\n", 0);
    printf("[dts-tpk] #%u <- 503: %s\n", nr, err);
    fflush(stdout);
    pthread_mutex_lock(&s->m); s->nErros++; pthread_mutex_unlock(&s->m);
    return 0;
  }
  if (seg < 0) {   // o player conta a posicao a partir do quadro-chave onde o fluxo comeca
    pthread_mutex_lock(&s->m); s->inicioStream = dts_remux_keyframe_before(s->rx, t); pthread_mutex_unlock(&s->m);
  }
  memset(&o, 0, sizeof o);
  o.sock = sock; o.manter = manter; o.tipo = tipo; o.s = s; o.ger = ger; o.t0 = t0;
  if (pd->range[0] && strncmp(pd->range, "bytes=0-", 8))
    printf("[dts-tpk] #%u: Range ignorado (gera do inicio, 200 inteiro)\n", nr);
  rr = dts_remux_gen_run(ger->g, seg, t, fmt, saidaEscrever, &o, &ger->parar, &st, err, sizeof err);
  soltarGerador(s, ger);
  int status = o.status, ok = rr == 0;
  int64_t enviados = o.bytes;
  if (ok) {
    if (!o.cabEnviado) enviarCab(&o);
    if (!enviarTudo(sock, "0\r\n\r\n", 5, &s->cancelar, NULL)) manter = 0;
  } else {
    if (!o.cabEnviado && rr != -2) {
      responderSimples(sock, 500, "Internal Server Error", NULL, "conversion failed\n", 0);
      status = 500;
    }
    manter = 0;
  }
  pthread_mutex_lock(&s->m);
  s->bytes += enviados;
  if (seg >= 0 && ok) { s->nSegs++; if (!st.fresh) s->nContinuou++; else s->nBuscou++; }
  if (!ok && rr != -2) s->nErros++;
  pthread_mutex_unlock(&s->m);
  uint64_t fim = agoraMs();
  char stTxt[8];                           // "-": nada saiu (o player fechou antes)
  if (status) snprintf(stTxt, sizeof stTxt, "%d", status); else snprintf(stTxt, sizeof stTxt, "-");
  if (seg >= 0)
    printf("[dts-tpk] #%u <- %s seg%d (%.3f-%.3f s): %lld B, 1o byte %u ms, total %u ms, gerador %d %s, cpu %.0f ms "
           "(audio %.0f), rede %.0f ms%s%s\n", nr, stTxt, seg, st.first_pts_s, st.last_pts_s, (long long)enviados,
           (unsigned)((o.primeiroByte ? o.primeiroByte : fim) - t0), (unsigned)(fim - t0), (int)(ger - s->ger),
           st.fresh ? "buscou" : "continuou", st.cpu_s * 1000.0, st.audio_cpu_s * 1000.0, st.range_wait_s * 1000.0,
           ok ? "" : rr == -2 ? ", player fechou" : ", ERRO: ", ok || rr == -2 ? "" : err);
  else
    printf("[dts-tpk] #%u <- %s %s desde %.3f s: %lld B em %u ms, %s%s\n", nr, stTxt, fmt, dts_remux_gen_start_s(ger->g),
           (long long)enviados, (unsigned)(fim - t0), ok ? "fim do arquivo" : rr == -2 ? "player fechou" : "ERRO: ",
           ok || rr == -2 ? "" : err);
  fflush(stdout);
  return ok && manter;
}

static void playlist(Sessao *s, char *b, size_t n) {
  const DtsRemuxInfo *in = dts_remux_info(s->rx);
  int k, alvo = (int)ceil(in->max_segment_s);
  size_t p = 0;
  if (alvo < 1) alvo = 1;
  p += (size_t)snprintf(b + p, n - p, "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:0\n"
                        "#EXT-X-PLAYLIST-TYPE:VOD\n#EXT-X-INDEPENDENT-SEGMENTS\n", alvo);
  for (k = 0; k < in->n_segments && p < n - 64; k++) {
    // Milissegundos inteiros: o LC_NUMERIC da TV nunca vira virgula aqui.
    long ms = lround(dts_remux_segment_duration(s->rx, k) * 1000.0);
    p += (size_t)snprintf(b + p, n - p, "#EXTINF:%ld.%03ld,\nseg%d.ts\n", ms / 1000, ms % 1000, k);
  }
  snprintf(b + p, n - p, "#EXT-X-ENDLIST\n");
}

// MASTER PLAYLIST: a single variant, but it tells the player the frame rate,
// size and HDR range before the first segment (the media playlist cannot). No
// CODECS: a wrong string is worse than none. Integers only: the TV's
// LC_NUMERIC never turns the decimal point into a comma here.
static void playlistMestre(Sessao *s, char *b, size_t n) {
  const DtsRemuxInfo *in = dts_remux_info(s->rx);
  long bw = in->size_bytes > 0 && in->duration_s > 1 ? (long)(in->size_bytes * 8.0 / in->duration_s) : 20000000L;
  long fps = lround(in->fps * 1000.0);
  size_t p = 0;
  if (bw < 1000000L) bw = 1000000L;
  p += (size_t)snprintf(b + p, n - p, "#EXTM3U\n#EXT-X-VERSION:7\n#EXT-X-INDEPENDENT-SEGMENTS\n");
  p += (size_t)snprintf(b + p, n - p, "#EXT-X-STREAM-INF:BANDWIDTH=%ld", bw);
  if (in->width > 0 && in->height > 0) p += (size_t)snprintf(b + p, n - p, ",RESOLUTION=%dx%d", in->width, in->height);
  if (fps > 0) p += (size_t)snprintf(b + p, n - p, ",FRAME-RATE=%ld.%03ld", fps / 1000, fps % 1000);
  if (!strncmp(in->hdr, "PQ", 2) || !strcmp(in->hdr, "HLG"))
    p += (size_t)snprintf(b + p, n - p, ",VIDEO-RANGE=%s", strncmp(in->hdr, "PQ", 2) ? "HLG" : "PQ");
  snprintf(b + p, n - p, "\nv.m3u8\n");
}

static int atenderPedido(int sock, const Pedido *pd, int manter) {
  char tok[16] = "", resto[200] = "";
  const char *c = pd->caminho;
  if (*c == '/') c++;
  { const char *b = strchr(c, '/');
    if (!b || b - c >= (int)sizeof tok) { responderSimples(sock, 404, "Not Found", NULL, "", 0); return 0; }
    snprintf(tok, sizeof tok, "%.*s", (int)(b - c), c);
    snprintf(resto, sizeof resto, "%s", b + 1); }
  Sessao *s = sessaoPegar(tok);
  unsigned nr;
  if (!s) {
    responderSimples(sock, 404, "Not Found", NULL, "", 0);
    printf("[dts-tpk] pedido de sessao antiga/desconhecida: 404\n");
    fflush(stdout);
    return 0;
  }
  pthread_mutex_lock(&s->m);
  nr = ++s->nPedidos;
  if (!s->primeiroPedidoMs) s->primeiroPedidoMs = agoraMs();
  int logAgente = !s->agente[0] && pd->agente[0];
  if (logAgente) snprintf(s->agente, sizeof s->agente, "%s", pd->agente);
  pthread_mutex_unlock(&s->m);
  printf("[dts-tpk] #%u -> %s %s%s%s Range=%s Connection=%s (+%u ms desde a abertura)%s%s\n", nr, pd->metodo, resto,
         pd->query[0] ? "?" : "", pd->query, pd->range[0] ? pd->range : "-", pd->conexao[0] ? pd->conexao : "-",
         (unsigned)(agoraMs() - s->criadaMs), logAgente ? " UA=" : "", logAgente ? pd->agente : "");
  fflush(stdout);
  int r = 0;
  if (!esperarPronta(s, sock, manter)) {
    if (s->semIndice) printf("[dts-tpk] #%u <- sem resposta: o app troca para o TS continuo\n", nr);
    else printf("[dts-tpk] #%u <- 503: %s\n", nr, s->erro[0] ? s->erro : "not ready");
    fflush(stdout);
  } else if (!strcmp(resto, "v.m3u8") || !strcmp(resto, "m.m3u8")) {
    int mestre = resto[0] == 'm';
    if (dts_remux_info(s->rx)->n_segments < 1) {
      responderSimples(sock, 404, "Not Found", NULL, "no keyframe index\n", manter);
      printf("[dts-tpk] #%u <- 404: fonte sem indice, nao ha playlist\n", nr);
    } else {
      size_t cap = (size_t)dts_remux_info(s->rx)->n_segments * 32 + 512;
      char *b = malloc(cap);
      if (b) {
        if (mestre) playlistMestre(s, b, cap); else playlist(s, b, cap);
        if (pd->head) {
          char cab[300];
          snprintf(cab, sizeof cab, "HTTP/1.1 200 OK\r\nContent-Type: application/vnd.apple.mpegurl\r\n"
                   "Content-Length: %zu\r\nCache-Control: no-cache\r\nConnection: %s\r\n\r\n", strlen(b), manter ? "keep-alive" : "close");
          enviarTudo(sock, cab, strlen(cab), NULL, NULL);
        } else responderSimples(sock, 200, "OK", "application/vnd.apple.mpegurl", b, manter);
        if (mestre) { const char *v = strstr(b, "BANDWIDTH"); v = v ? v : "";
                      printf("[dts-tpk] #%u <- 200 playlist mestre: %.*s\n", nr, (int)strcspn(v, "\n"), v); }
        else printf("[dts-tpk] #%u <- 200 playlist: %d segmento(s), %zu B\n", nr, dts_remux_info(s->rx)->n_segments, strlen(b));
        free(b);
        r = manter;
      }
    }
    fflush(stdout);
  } else if (!strncmp(resto, "seg", 3)) {
    int k = atoi(resto + 3);
    if (k < 0 || k >= dts_remux_info(s->rx)->n_segments) {
      responderSimples(sock, 404, "Not Found", NULL, "", manter);
      printf("[dts-tpk] #%u <- 404 segmento %d fora da playlist\n", nr, k);
      fflush(stdout);
      r = manter;
    } else r = servirFluxo(sock, s, pd, nr, k, 0, "mpegts", "video/mp2t", manter);
  } else if (!strcmp(resto, "prog.ts")) {
    const char *q = strstr(pd->query, "t=");
    double t = q ? atof(q + 2) : 0.0;
    r = servirFluxo(sock, s, pd, nr, -1, t, "mpegts", "video/mp2t", manter);
  } else {
    responderSimples(sock, 404, "Not Found", NULL, "", manter);
    r = manter;
  }
  sessaoSoltar(s);
  return r;
}

// One connection of the player (its first request head already read and its
// token checked by the porteiro); keep-alive requests follow on the same socket.
static void *atender(void *u) {
  Conexao *c = u;
  int sock = c->sock;
  size_t ja = c->n;
  char cab[CAB_MAX], resto[CAB_MAX];
  memcpy(cab, c->cab, ja + 1);
  for (int n = 0;; n++) {
    Pedido pd;
    int tot = lerPedido(sock, cab, sizeof cab, n ? 60000 : 15000, ja);
    size_t h;
    if (!tot) break;
    h = fimCabecalho(cab);
    ja = (size_t)tot - h;                 // a pipelined next request, if any
    memcpy(resto, cab + h, ja);
    cab[h] = 0;
    if (!lerLinha(cab, &pd)) { responderSimples(sock, 405, "Method Not Allowed", NULL, "", 0); break; }
    int manter = pd.http11 ? strcasecmp(pd.conexao, "close") != 0 : strcasecmp(pd.conexao, "keep-alive") == 0;
    if (!atenderPedido(sock, &pd, manter)) break;
    memcpy(cab, resto, ja);
  }
  conexaoFim(c);
  return NULL;
}

// A head for the live session's token (old sessions' and unknown ones: 404).
static int tokenAtual(const char *cab) {   // under porteiroHls.m
  return atual && tokenNoCaminho(cab, atual->token);
}
static Porteiro porteiroHls;

// Sob G. Only 127.0.0.1 (review blocker 3): the TV's own player is the client.
static int subir(void) {
  if (sockLo >= 0) return 1;
  signal(SIGPIPE, SIG_IGN);
  sockLo = ouvir(INADDR_LOOPBACK, &portaLo);
  if (sockLo < 0) return 0;
  // 8 connections at once: the player uses 1-3; the rest wait in the porteiro.
  if (!porteiroSubir(&porteiroHls, sockLo, PORT_VIVOS, tokenAtual, atender)) { close(sockLo); sockLo = -1; return 0; }
  printf("[dts-tpk] servidor ouvindo em 127.0.0.1:%d\n", portaLo);
  fflush(stdout);
  return 1;
}

// Versao maior da Tizen (system-info por dlopen, como gpunivel.c). -1 = nao leu.
static int tizenMaior(void) {
  static int v = -2;
  if (v == -2) {
    static const char *const libs[] = { "libcapi-system-info.so.0", "libcapi-system-info.so", NULL };
    int (*get)(const char *, char **) = NULL;
    void *h = NULL;
    char *t = NULL;
    v = -1;
    for (int i = 0; libs[i] && !h; i++) h = dlopen(libs[i], RTLD_NOW);
    if (h) *(void **)&get = dlsym(h, "system_info_get_platform_string");
    if (get && get("http://tizen.org/feature/platform.version", &t) == 0 && t) { v = atoi(t); free(t); }
  }
  return v;
}

// Anel do leitor da fonte, MiB: abaixo do 8 (menos RAM) cai de 32 para 16.
static int anelMib(void) {
  int t = tizenMaior();
  return t >= 6 && t < 8 ? 16 : 32;
}

// Tizen 6 em diante: os tres pacotes 6+ (NuvioTpk60/65/NuvioTpk) carregam a
// mesma .so, e as TVs de 2021/2022 (6.0/6.5) tambem nao tocam DTS (especificacao
// de video da Samsung de cada ano).
int dts_tpk_disponivel(void) {
  static int c = -1;   // os Ajustes perguntam a cada quadro: decide uma vez
  if (c < 0) {
    int t = tizenMaior();
    c = (t < 0 || t >= 6) && dts_remux_available();
    printf("[dts-tpk] conversao DTS/TrueHD %s (tizen %d, FFmpeg %s, anel %d MiB)\n", c ? "disponivel" : "indisponivel", t,
           dts_remux_available() ? "ligado" : "ausente", anelMib());
    fflush(stdout);
  }
  return c;
}

// --- memory line (one per playback) --------------------------------------------
// Peak resident memory of the app and the lowest MemAvailable of the TV while a
// session lives, sampled every 500 ms (and VmHWM, reset at the session start
// when the kernel allows it), with the reader rings alive. Kilobytes; -1 = not
// readable. Stdout only (the app log).
static long lerKb(const char *arq, const char *chave) {
  char l[160];
  size_t k = strlen(chave);
  long v = -1;
  FILE *f = fopen(arq, "r");
  if (!f) return -1;
  while (fgets(l, sizeof l, f))
    if (!strncmp(l, chave, k) && l[k] == ':') { v = atol(l + k + 1); break; }
  fclose(f);
  return v;
}
// Package id from where libnuvio.so was loaded (.../<package>/lib/libnuvio.so).
static const char *pacote(void) {
  static char p[64];
  if (!p[0]) {
    char l[512];
    FILE *f = fopen("/proc/self/maps", "r");
    snprintf(p, sizeof p, "?");
    while (f && fgets(l, sizeof l, f)) {
      char *a = strstr(l, "/lib/libnuvio"), *b;
      if (!a) continue;
      *a = 0;
      b = strrchr(l, '/');
      if (b && b[1]) snprintf(p, sizeof p, "%s", b + 1);
      break;
    }
    if (f) fclose(f);
  }
  return p;
}
static void memAmostra(Sessao *s, long rss, long livre) {   // sob s->m
  if (rss > s->rssPico) s->rssPico = rss;
  if (livre >= 0 && (s->livreMin < 0 || livre < s->livreMin)) s->livreMin = livre;
}
static void memLinha(Sessao *s, int fim) {
  DtsEngineRings rg;
  long hwm = lerKb("/proc/self/status", "VmHWM"), total = lerKb("/proc/meminfo", "MemTotal");
  dts_engine_rings(&rg, 0);
  pthread_mutex_lock(&s->m);
  memAmostra(s, lerKb("/proc/self/status", "VmRSS"), lerKb("/proc/meminfo", "MemAvailable"));
  printf("[dts-tpk] mem%s: sessao %u, %s, tizen %d, RAM %ld MB; app RSS peak %ld MB (start %ld, VmHWM %ld %s), "
         "MemAvailable low %ld MB (start %ld); reader rings peak %d = %lld MiB, alive now %d = %lld MiB; %.0f s\n",
         fim ? "" : " (running)", s->id, pacote(), tizenMaior(), total / 1024, s->rssPico / 1024, s->rssIni / 1024,
         hwm / 1024, s->hwmZerado ? "since session start" : "since app start", s->livreMin / 1024, s->livreIni / 1024,
         rg.peak, rg.peak_bytes >> 20, rg.alive, rg.bytes >> 20, (agoraMs() - s->criadaMs) / 1000.0);
  pthread_mutex_unlock(&s->m);
  fflush(stdout);
}
static void memInicio(Sessao *s) {
  FILE *f = fopen("/proc/self/clear_refs", "w");   // "5": VmHWM back to the RSS of now
  s->hwmZerado = f && fputs("5", f) >= 0;
  if (f && fclose(f)) s->hwmZerado = 0;
  s->rssIni = s->rssPico = lerKb("/proc/self/status", "VmRSS");
  s->livreIni = s->livreMin = lerKb("/proc/meminfo", "MemAvailable");
  s->memLogMs = s->criadaMs;
  dts_engine_rings(NULL, 1);
}
static void *fioMemoria(void *u) {
  (void)u;
  for (;;) {
    long rss, livre;
    Sessao *s;
    int linha = 0;
    usleep(500 * 1000);
    rss = lerKb("/proc/self/status", "VmRSS");
    livre = lerKb("/proc/meminfo", "MemAvailable");
    pthread_mutex_lock(&G);
    if ((s = atual) != NULL) {
      pthread_mutex_lock(&s->m);
      memAmostra(s, rss, livre);
      if (agoraMs() - s->memLogMs >= 120000) { s->memLogMs = agoraMs(); linha = 1; }
      pthread_mutex_unlock(&s->m);
      if (linha) memLinha(s, 0);   // a running line every 2 min: an OOM kill leaves no final one
    }
    pthread_mutex_unlock(&G);
  }
  return NULL;
}

static void encerrarAtual(void) {   // sob G
  Sessao *s = atual;
  if (!s) return;
  pthread_mutex_lock(&porteiroHls.m);
  atual = NULL;
  for (int i = 0; i < porteiroHls.maxVivos; i++) if (porteiroHls.conexoes[i]) {
    shutdown(porteiroHls.conexoes[i]->sock, SHUT_RDWR);
    porteiroHls.conexoes[i] = NULL;
  }
  atomic_store(&porteiroHls.vivos, 0);
  pthread_mutex_unlock(&porteiroHls.m);
  pthread_mutex_lock(&s->m);
  s->cancelar = 1;
  for (int i = 0; i < GERADORES; i++) s->ger[i].parar = 1;
  if (s->rxSonda) dts_remux_cancel(s->rxSonda);   // the probe hangs up on the source now
  pthread_cond_broadcast(&s->c);
  pthread_mutex_unlock(&s->m);
  if (s->rx) dts_remux_cancel(s->rx);
  sessaoResumo(s);
  memLinha(s, 1);
  if (--s->refs == 0) {
    pthread_t t;
    if (pthread_create(&t, NULL, fioDestruir, s) == 0) pthread_detach(t);
    else sessaoDestruir(s);
  }
}

static int urlDe(Sessao *s, double t, char *out, size_t n) {
  const char *host = "127.0.0.1";
  int porta = portaLo;
  switch (s->modo) {
    case DTSTPK_HLS:
      snprintf(out, n, "http://%s:%d/%s/m.m3u8", host, porta, s->token); break;
    default: {
      long ms = lround((t > 0 ? t : 0) * 1000.0);
      snprintf(out, n, "http://%s:%d/%s/prog.ts?t=%ld.%03ld", host, porta, s->token, ms / 1000, ms % 1000);
    }
  }
  return 1;
}

int dts_tpk_abrir(const char *url, const char *headers, int track, int stereo, int modo,
                  double inicio, char *out, size_t n) {
  Sessao *s;
  pthread_t t;
  unsigned id;
  if (out && n) out[0] = 0;
  if (!url || !*url || !out || n < 64 || modo < 0 || modo >= DTSTPK_DESLIGADO || !dts_remux_available()) return 0;
  pthread_mutex_lock(&G);
  encerrarAtual();
  if (!subir()) {
    pthread_mutex_unlock(&G);
    printf("[dts-tpk] o servidor nao subiu em 127.0.0.1 (%s)\n", strerror(errno));
    fflush(stdout);
    return 0;
  }
  dts_remux_set_queue_budget_mib(anelMib() == 16 ? 128 : 192);
  dts_engine_set_ring_mib(anelMib());   // ring of the readers this session opens
  { static int monitor;
    pthread_t tm;
    if (!monitor && pthread_create(&tm, NULL, fioMemoria, NULL) == 0) { pthread_detach(tm); monitor = 1; } }
  s = calloc(1, sizeof *s);
  if (!s) { pthread_mutex_unlock(&G); return 0; }
  pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->c, NULL);
  s->id = ++contador;
  gerarToken(s->token, 8);
  s->url = strdup(url); s->cabs = strdup(headers ? headers : "");
  s->track = track; s->stereo = stereo; s->modo = modo;
  s->estado = DTSTPK_SONDANDO; s->inicioStream = -1;
  s->criadaMs = agoraMs();
  memInicio(s);
  for (int i = 0; i < GERADORES; i++) s->ger[i].seg = -1;
  if (!s->url || !s->cabs) { pthread_mutex_unlock(&G); sessaoDestruir(s); return 0; }
  s->refs = 2;   // a sessao atual e o fio da sonda
  pthread_mutex_lock(&porteiroHls.m);
  atual = s;
  pthread_mutex_unlock(&porteiroHls.m);
  if (pthread_create(&t, NULL, fioSonda, s) != 0) {
    pthread_mutex_lock(&porteiroHls.m); atual = NULL; pthread_mutex_unlock(&porteiroHls.m); s->refs = 0; pthread_mutex_unlock(&G); sessaoDestruir(s); return 0;
  }
  pthread_detach(t);
  urlDe(s, inicio, out, n);
  id = s->id;   // s may go once G is released (a close from another thread)
  pthread_mutex_unlock(&G);
  printf("[dts-tpk] sessao %u: %s em 127.0.0.1, audio %d do arquivo, saida %s; player abre %s; segmentos de %.0f s\n",
         id, MODO_NOME[modo], track, stereo ? "AAC 2.0" : "AC3",
         strstr(out, ".m3u8") ? "a playlist HLS" : "o fluxo progressivo", SEG_ALVO_S);
  fflush(stdout);
  return 1;
}

int dts_tpk_prebuffer_ms(void) { return PREBUFFER_MS; }

int dts_tpk_url_em(double t, char *out, size_t n) {
  int ok = 0;
  if (out && n) out[0] = 0;
  pthread_mutex_lock(&G);
  if (atual && out && n >= 64) {
    ok = urlDe(atual, t, out, n);
    pthread_mutex_lock(&atual->m); atual->inicioStream = -1; pthread_mutex_unlock(&atual->m);
  }
  pthread_mutex_unlock(&G);
  return ok;
}

void dts_tpk_fechar(void) {
  pthread_mutex_lock(&G);
  encerrarAtual();
  pthread_mutex_unlock(&G);
}

#define LER_ATUAL(tipo, campo, padrao) \
  tipo v = padrao; pthread_mutex_lock(&G); \
  if (atual) { pthread_mutex_lock(&atual->m); v = atual->campo; pthread_mutex_unlock(&atual->m); } \
  pthread_mutex_unlock(&G); return v;
int dts_tpk_estado(void) { LER_ATUAL(int, estado, DTSTPK_NADA) }
double dts_tpk_duracao(void) { LER_ATUAL(double, duracao, 0.0) }
int dts_tpk_modo(void) { LER_ATUAL(int, modo, -1) }
double dts_tpk_inicio_stream(void) { LER_ATUAL(double, inicioStream, -1.0) }
const char *dts_tpk_erro(void) {
  static char b[160];
  b[0] = 0;
  pthread_mutex_lock(&G);
  if (atual) { pthread_mutex_lock(&atual->m); snprintf(b, sizeof b, "%s", atual->erro); pthread_mutex_unlock(&atual->m); }
  pthread_mutex_unlock(&G);
  return b;
}
int dts_tpk_sem_indice(void) { LER_ATUAL(int, semIndice, 0) }
const char *dts_tpk_caminho(void) {
  static char b[40];
  b[0] = 0;
  pthread_mutex_lock(&G);
  if (atual) { pthread_mutex_lock(&atual->m); snprintf(b, sizeof b, "%s", atual->caminho); pthread_mutex_unlock(&atual->m); }
  pthread_mutex_unlock(&G);
  return b;
}
int dts_tpk_e_url(const char *u) {
  char a[80] = "";
  if (!u) return 0;
  pthread_mutex_lock(&G);
  if (portaLo) snprintf(a, sizeof a, "http://127.0.0.1:%d/", portaLo);
  pthread_mutex_unlock(&G);
  return a[0] && !strncmp(u, a, strlen(a));
}

#endif
