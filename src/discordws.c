// Ver discordws.h.
#include "discordws.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- navegador
#include <emscripten/websocket.h>

typedef struct Msg { struct Msg *prox; char *texto; } Msg;

struct DiscordWs {
  EMSCRIPTEN_WEBSOCKET_T s;
  int estado, codigo, morto;
  Msg *ini, *fim;
};

static EM_BOOL aoAbrir(int t, const EmscriptenWebSocketOpenEvent *e, void *u) {
  DiscordWs *w = (DiscordWs *)u;
  (void)t; (void)e;
  w->estado = DWS_ABERTO;
  return EM_TRUE;
}
static EM_BOOL aoMensagem(int t, const EmscriptenWebSocketMessageEvent *e, void *u) {
  DiscordWs *w = (DiscordWs *)u;
  Msg *m;
  (void)t;
  if (!e->isText) return EM_TRUE;   // o gateway com encoding=json so manda texto
  m = (Msg *)calloc(1, sizeof *m);
  if (!m) return EM_TRUE;
  m->texto = (char *)malloc(e->numBytes + 1);
  if (!m->texto) { free(m); return EM_TRUE; }
  memcpy(m->texto, e->data, e->numBytes);
  m->texto[e->numBytes] = 0;
  if (w->fim) w->fim->prox = m; else w->ini = m;
  w->fim = m;
  return EM_TRUE;
}
static EM_BOOL aoFechar(int t, const EmscriptenWebSocketCloseEvent *e, void *u) {
  DiscordWs *w = (DiscordWs *)u;
  (void)t;
  w->codigo = e->code;
  w->estado = DWS_CAIU;
  return EM_TRUE;
}
static EM_BOOL aoErro(int t, const EmscriptenWebSocketErrorEvent *e, void *u) {
  DiscordWs *w = (DiscordWs *)u;
  (void)t; (void)e;
  w->estado = DWS_CAIU;
  return EM_TRUE;
}

DiscordWs *dws_abrir(const char *url) {
  EmscriptenWebSocketCreateAttributes a;
  DiscordWs *w = (DiscordWs *)calloc(1, sizeof *w);
  if (!w) return NULL;
  emscripten_websocket_init_create_attributes(&a);
  a.url = url;
  // Callbacks no fio que criou — o principal, o mesmo que chama dws_receber.
  a.createOnMainThread = EM_FALSE;
  w->s = emscripten_websocket_new(&a);
  if (w->s <= 0) { w->estado = DWS_CAIU; return w; }
  emscripten_websocket_set_onopen_callback(w->s, w, aoAbrir);
  emscripten_websocket_set_onmessage_callback(w->s, w, aoMensagem);
  emscripten_websocket_set_onclose_callback(w->s, w, aoFechar);
  emscripten_websocket_set_onerror_callback(w->s, w, aoErro);
  return w;
}

int dws_estado(DiscordWs *w) { return w ? w->estado : DWS_CAIU; }
int dws_codigo_fechamento(DiscordWs *w) { return w ? w->codigo : 0; }

int dws_enviar(DiscordWs *w, const char *texto) {
  if (!w || w->estado != DWS_ABERTO) return -1;
  return emscripten_websocket_send_utf8_text(w->s, texto) == EMSCRIPTEN_RESULT_SUCCESS ? 0 : -1;
}

char *dws_receber(DiscordWs *w) {
  Msg *m;
  char *t;
  if (!w || !w->ini) return NULL;
  m = w->ini;
  w->ini = m->prox;
  if (!w->ini) w->fim = NULL;
  t = m->texto;
  free(m);
  return t;
}

void dws_fechar(DiscordWs *w) {
  char *t;
  if (!w) return;
  if (w->s > 0) {
    emscripten_websocket_close(w->s, 1000, "");
    emscripten_websocket_delete(w->s);
  }
  while ((t = dws_receber(w))) free(t);
  free(w);
}

#else
// ---------------------------------------------------------------- nativo
#include <pthread.h>
#include <stdatomic.h>
#include "rede.h"

struct DiscordWs {
  pthread_mutex_t trava;
  RedeTls *tls;
  char host[128], caminho[2048];   // o bilhete do Watch Together vai na query
  int claro;                       // ws:// (sem TLS): so teste local
  atomic_int estado;
  int abandonado;          // dws_fechar chegou com o fio ainda conectando
  int codigo;
  unsigned char *rx;       // bytes crus ainda nao consumidos
  size_t nRx, capRx;
  unsigned char *msg;      // mensagem fragmentada em montagem
  size_t nMsg, capMsg;
  unsigned char *tx;
  size_t nTx, offTx, capTx;
  unsigned txDesde;
};

static int crescer(unsigned char **p, size_t *cap, size_t precisa) {
  unsigned char *n;
  size_t c = *cap ? *cap : 4096;
  if (precisa <= *cap) return 0;
  while (c < precisa) c *= 2;
  n = (unsigned char *)realloc(*p, c);
  if (!n) return -1;
  *p = n;
  *cap = c;
  return 0;
}

static unsigned aleatorio(void) {
  unsigned n = 0;
  FILE *f = fopen("/dev/urandom", "rb");
  if (f) { if (fread(&n, sizeof n, 1, f) != 1) n = 0; fclose(f); }
  return n;
}

static unsigned agoraWs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

// The render thread never waits for socket writability. Retain partial frames
// in order, cap memory and abandon a stalled connection after ten seconds.
static int descarregar(DiscordWs *w) {
  int i;
  if (w->offTx == w->nTx) return 0;
  if (agoraWs() - w->txDesde >= 10000u) { w->estado = DWS_CAIU; return -1; }
  for (i = 0; i < 4 && w->offTx < w->nTx; i++) {
    size_t foi = 0;
    if (rede_tls_tentar_enviar(w->tls, w->tx + w->offTx,
                              w->nTx - w->offTx, &foi) != 0) {
      w->estado = DWS_CAIU; return -1;
    }
    if (!foi) break;
    w->offTx += foi;
  }
  if (w->offTx == w->nTx) w->offTx = w->nTx = 0;
  return 0;
}

static void base64(const unsigned char *in, int n, char *out) {
  static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int i, o = 0;
  for (i = 0; i + 2 < n; i += 3) {
    out[o++] = t[in[i] >> 2];
    out[o++] = t[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
    out[o++] = t[((in[i + 1] & 15) << 2) | (in[i + 2] >> 6)];
    out[o++] = t[in[i + 2] & 63];
  }
  if (i < n) {
    out[o++] = t[in[i] >> 2];
    if (i + 1 < n) {
      out[o++] = t[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
      out[o++] = t[(in[i + 1] & 15) << 2];
    } else {
      out[o++] = t[(in[i] & 3) << 4];
      out[o++] = '=';
    }
    out[o++] = '=';
  }
  out[o] = 0;
}

static void liberar(DiscordWs *w) {
  if (w->tls) rede_tls_fechar(w->tls);
  free(w->rx);
  free(w->msg);
  free(w->tx);
  pthread_mutex_destroy(&w->trava);
  free(w);
}

// Aperto de mao HTTP/1.1 -> 101. Roda no fio de conexao.
static int apertarMao(DiscordWs *w) {
  unsigned char chave[16];
  char k64[32], pedido[2600];
  int i, n;
  for (i = 0; i < 16; i++) chave[i] = (unsigned char)aleatorio();
  base64(chave, 16, k64);
  n = snprintf(pedido, sizeof pedido,
               "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\n"
               "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
               "Sec-WebSocket-Version: 13\r\nUser-Agent: Nuvio/1.0\r\n\r\n",
               w->caminho, w->host, k64);
  if (n <= 0 || n >= (int)sizeof pedido) return -1;
  if (rede_tls_enviar(w->tls, pedido, (size_t)n) != 0) return -1;
  // Le ate o fim dos cabecalhos. O que vier depois ja e websocket e fica no rx.
  for (i = 0; i < 100; i++) {
    unsigned char *fim;
    int r;
    if (crescer(&w->rx, &w->capRx, w->nRx + 2048 + 1) != 0) return -1;
    r = rede_tls_receber(w->tls, w->rx + w->nRx, 2048, 100);
    if (r < 0) return -1;
    w->nRx += (size_t)r;
    w->rx[w->nRx] = 0;
    fim = (unsigned char *)strstr((char *)w->rx, "\r\n\r\n");
    if (fim) {
      size_t cab = (size_t)(fim - w->rx) + 4;
      // Sec-WebSocket-Accept nao e conferido: o servidor e fixo, a conexao e
      // TLS e o pior caso de um 101 falso seria o IDENTIFY ser recusado.
      if (strncmp((char *)w->rx, "HTTP/1.1 101", 12) != 0) {
        printf("[discord] ws: handshake rejected: %.40s\n", (char *)w->rx);
        return -1;
      }
      memmove(w->rx, w->rx + cab, w->nRx - cab);
      w->nRx -= cab;
      return 0;
    }
  }
  return -1;
}

static void *fioConectar(void *u) {
  DiscordWs *w = (DiscordWs *)u;
  char url[200];
  RedeTls *t;
  int ok;
  // wss:// e TLS na 443 (ou na porta da URL); ws:// so existe para o teste
  // local do Watch Together contra o `wrangler dev` (TCP puro, mesma libcurl).
  if (strchr(w->host, ':')) snprintf(url, sizeof url, "%s://%s/", w->claro ? "http" : "https", w->host);
  else snprintf(url, sizeof url, "%s://%s:%d/", w->claro ? "http" : "https", w->host, w->claro ? 80 : 443);
  t = rede_tls_abrir(url, 15);
  pthread_mutex_lock(&w->trava);
  w->tls = t;
  pthread_mutex_unlock(&w->trava);
  ok = t && apertarMao(w) == 0;
  pthread_mutex_lock(&w->trava);
  if (w->abandonado) { pthread_mutex_unlock(&w->trava); liberar(w); return NULL; }
  w->estado = ok ? DWS_ABERTO : DWS_CAIU;
  pthread_mutex_unlock(&w->trava);
  return NULL;
}

DiscordWs *dws_abrir(const char *url) {
  DiscordWs *w;
  const char *h, *barra;
  pthread_t f;
  size_t nh;
  w = (DiscordWs *)calloc(1, sizeof *w);
  if (!w) return NULL;
  pthread_mutex_init(&w->trava, NULL);
  atomic_init(&w->estado, DWS_CONECTANDO);
  if (url && !strncmp(url, "ws://", 5)) { w->claro = 1; h = url + 5; }
  else if (url && !strncmp(url, "wss://", 6)) h = url + 6;
  else { w->estado = DWS_CAIU; return w; }
  barra = h + strcspn(h, "/?");
  nh = (size_t)(barra - h);
  if (nh == 0 || nh >= sizeof w->host) { w->estado = DWS_CAIU; return w; }
  memcpy(w->host, h, nh);
  w->host[nh] = 0;
  snprintf(w->caminho, sizeof w->caminho, "%s%s", *barra == '/' ? "" : "/", barra);
  w->estado = DWS_CONECTANDO;
  if (pthread_create(&f, NULL, fioConectar, w) != 0) { w->estado = DWS_CAIU; return w; }
  pthread_detach(f);
  return w;
}

int dws_estado(DiscordWs *w) { return w ? w->estado : DWS_CAIU; }
int dws_codigo_fechamento(DiscordWs *w) { return w ? w->codigo : 0; }

// Quadro do cliente: sempre mascarado (a RFC exige).
static int mandarQuadro(DiscordWs *w, int opcode, const unsigned char *p, size_t n) {
  unsigned char cab[14], *q;
  size_t h = 0, i;
  unsigned m = aleatorio();
  int r;
  cab[h++] = (unsigned char)(0x80 | opcode);
  if (n < 126) cab[h++] = (unsigned char)(0x80 | n);
  else if (n < 65536) { cab[h++] = 0x80 | 126; cab[h++] = (unsigned char)(n >> 8); cab[h++] = (unsigned char)n; }
  else {
    cab[h++] = 0x80 | 127;
    for (i = 0; i < 8; i++) cab[h++] = (unsigned char)((unsigned long long)n >> (56 - 8 * i));
  }
  cab[h++] = (unsigned char)(m >> 24); cab[h++] = (unsigned char)(m >> 16);
  cab[h++] = (unsigned char)(m >> 8);  cab[h++] = (unsigned char)m;
  if (h + n > 65536u) { w->estado = DWS_CAIU; return -1; }
  q = (unsigned char *)malloc(h + n);
  if (!q) return -1;
  memcpy(q, cab, h);
  for (i = 0; i < n; i++) q[h + i] = p[i] ^ cab[h - 4 + (i & 3)];
  if (w->offTx) {
    memmove(w->tx, w->tx + w->offTx, w->nTx - w->offTx);
    w->nTx -= w->offTx; w->offTx = 0;
  }
  if (h + n > 65536u || w->nTx > 65536u - (h + n) ||
      crescer(&w->tx, &w->capTx, w->nTx + h + n) != 0) {
    free(q); w->estado = DWS_CAIU; return -1;
  }
  if (!w->nTx) w->txDesde = agoraWs();
  memcpy(w->tx + w->nTx, q, h + n);
  w->nTx += h + n;
  free(q);
  r = descarregar(w);
  return r;
}

int dws_enviar(DiscordWs *w, const char *texto) {
  if (!w || w->estado != DWS_ABERTO || !texto) return -1;
  return mandarQuadro(w, 1, (const unsigned char *)texto, strlen(texto));
}

// Tira UM quadro completo do rx. 1 = tirou, 0 = falta byte, -1 = protocolo.
static int tirarQuadro(DiscordWs *w, int *fin, int *opcode, unsigned char **dados, size_t *n) {
  size_t h = 2, len, i;
  unsigned char *p = w->rx;
  if (w->nRx < 2) return 0;
  *fin = (p[0] & 0x80) != 0;
  *opcode = p[0] & 0x0F;
  if (p[1] & 0x80) return -1;            // servidor nunca mascara
  len = p[1] & 0x7F;
  if (len == 126) {
    if (w->nRx < 4) return 0;
    len = ((size_t)p[2] << 8) | p[3];
    h = 4;
  } else if (len == 127) {
    unsigned long long l = 0;
    if (w->nRx < 10) return 0;
    for (i = 0; i < 8; i++) l = (l << 8) | p[2 + i];
    if (l > 1024u * 1024u) return -1;  // READY tem dezenas de KB; 1 MB is the connection limit
    len = (size_t)l;
    h = 10;
  }
  if (w->nRx < h + len) return 0;
  *dados = (unsigned char *)malloc(len + 1);
  if (!*dados) return -1;
  memcpy(*dados, p + h, len);
  (*dados)[len] = 0;
  *n = len;
  memmove(w->rx, w->rx + h + len, w->nRx - h - len);
  w->nRx -= h + len;
  return 1;
}

char *dws_receber(DiscordWs *w) {
  int voltas;
  if (!w || w->estado != DWS_ABERTO) return NULL;
  if (descarregar(w) != 0) return NULL;
  // Puxa o que ja chegou, sem esperar. Teto de voltas para um fluxo grande nao
  // segurar o quadro do app.
  for (voltas = 0; voltas < 16; voltas++) {
    int r;
    if (w->nRx > 1024u * 1024u - 16384u ||
        crescer(&w->rx, &w->capRx, w->nRx + 16384 + 1) != 0) {
      w->estado = DWS_CAIU; return NULL;
    }
    r = rede_tls_receber(w->tls, w->rx + w->nRx, 16384, 0);
    if (r == 0) break;
    if (r < 0) {
      // Nao derruba ainda: o que ja esta no rx (um quadro de fechamento com o
      // codigo, por exemplo) ainda e lido abaixo.
      w->estado = DWS_CAIU;
      break;
    }
    w->nRx += (size_t)r;
  }
  for (voltas = 0; voltas < 32; voltas++) {
    int fin, op, t;
    unsigned char *d = NULL;
    size_t n = 0;
    t = tirarQuadro(w, &fin, &op, &d, &n);
    if (t == 0) return NULL;
    if (t < 0) { w->estado = DWS_CAIU; return NULL; }
    if (op == 9) {                 // ping -> pong com o mesmo conteudo
      if (w->estado == DWS_ABERTO) mandarQuadro(w, 10, d, n);
      free(d);
      if (w->estado != DWS_ABERTO) return NULL;
      continue;
    }
    if (op == 10) { free(d); continue; }
    if (op == 8) {                 // fechamento: codigo nos 2 primeiros bytes
      w->codigo = n >= 2 ? (d[0] << 8) | d[1] : 0;
      printf("[discord] ws: server closed (%d)\n", w->codigo);
      free(d);
      w->estado = DWS_CAIU;
      return NULL;
    }
    if (op == 1 || op == 2 || op == 0) {
      if (op != 0) w->nMsg = 0;
      if (n > 1024u * 1024u || w->nMsg > 1024u * 1024u - n ||
          crescer(&w->msg, &w->capMsg, w->nMsg + n + 1) != 0) { free(d); w->estado = DWS_CAIU; return NULL; }
      memcpy(w->msg + w->nMsg, d, n);
      w->nMsg += n;
      free(d);
      if (fin) {
        char *s = (char *)malloc(w->nMsg + 1);
        if (!s) return NULL;
        memcpy(s, w->msg, w->nMsg);
        s[w->nMsg] = 0;
        w->nMsg = 0;
        return s;
      }
      continue;
    }
    free(d);
  }
  return NULL; // Remaining control/fragment frames continue next app tick.
}

void dws_fechar(DiscordWs *w) {
  if (!w) return;
  pthread_mutex_lock(&w->trava);
  if (w->estado == DWS_CONECTANDO) {
    // O fio de conexao ainda e dono da struct: ele libera ao terminar.
    w->abandonado = 1;
    pthread_mutex_unlock(&w->trava);
    return;
  }
  pthread_mutex_unlock(&w->trava);
  // Closing the TLS transport clears presence without waiting to drain writes.
  liberar(w);
}

#endif
