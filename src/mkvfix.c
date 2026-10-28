// Ver mkvfix.h para o porque.
#include "mkvfix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_EBML     0x1A45DFA3u
#define ID_SEGMENT  0x18538067u
#define ID_SEEKHEAD 0x114D9B74u
#define ID_SEEK     0x4DBBu
#define ID_SEEKID   0x53ABu
#define ID_SEEKPOS  0x53ACu
#define ID_VOID     0xECu
#define ID_CUES     0x1C53BB6Bu

// ---- EBML ------------------------------------------------------------------
static int largura(unsigned char b) { int w = 1; unsigned char m = 0x80; while (w <= 8 && !(b & m)) { m >>= 1; w++; } return w; }
static uint32_t lerId(const unsigned char *p, long resta, int *usou) {
  int w; uint32_t v = 0;
  if (resta < 1) return 0;
  w = largura(p[0]);
  if (w > 4 || w > resta) return 0;
  for (int i = 0; i < w; i++) v = (v << 8) | p[i];
  *usou = w; return v;
}
// -1 = tamanho desconhecido, -2 = invalido.
static int64_t lerTam(const unsigned char *p, long resta, int *usou) {
  int w, todos = 1; uint64_t v;
  if (resta < 1) return -2;
  w = largura(p[0]);
  if (w > 8 || w > resta) return -2;
  v = p[0] & (0xFF >> w);
  if (v != (uint64_t)(0xFFu >> w)) todos = 0;
  for (int i = 1; i < w; i++) { v = (v << 8) | p[i]; if (p[i] != 0xFF) todos = 0; }
  *usou = w;
  if (todos) return -1;
  return v > (uint64_t)INT64_MAX ? -2 : (int64_t)v;
}
static uint64_t lerUint(const unsigned char *p, int64_t n) {
  uint64_t v = 0;
  for (int64_t i = 0; i < n && i < 8; i++) v = (v << 8) | p[i];
  return v;
}

int mkvfix_ler_seekhead(const unsigned char *b, long n, uint32_t *id, int64_t *pos, int max) {
  int ui = 0, us = 0, k = 0;
  int64_t s, o;
  if (lerId(b, n, &ui) != ID_SEEKHEAD) return -1;
  s = lerTam(b + ui, n - ui, &us);
  if (s < 0 || ui + us + s > n) return -1;
  b += ui + us; o = 0;
  while (o < s) {
    int a = 0, c = 0;
    uint32_t i2 = lerId(b + o, (long)(s - o), &a);
    int64_t s2;
    if (!i2) break;
    s2 = lerTam(b + o + a, (long)(s - o - a), &c);
    if (s2 < 0 || o + a + c + s2 > s) break;
    if (i2 == ID_SEEK && k < max) {
      const unsigned char *v = b + o + a + c;
      int64_t q = 0, p = -1; uint32_t alvo = 0;
      while (q < s2) {
        int x = 0, y = 0;
        uint32_t i3 = lerId(v + q, (long)(s2 - q), &x);
        int64_t s3;
        if (!i3) break;
        s3 = lerTam(v + q + x, (long)(s2 - q - x), &y);
        if (s3 < 0 || q + x + y + s3 > s2) break;
        if (i3 == ID_SEEKID) alvo = (uint32_t)lerUint(v + q + x + y, s3);
        else if (i3 == ID_SEEKPOS && s3 <= 8) p = (int64_t)lerUint(v + q + x + y, s3);
        q += x + y + s3;
      }
      if (alvo && p >= 0) { id[k] = alvo; pos[k] = p; k++; }
    }
    o += a + c + s2;
  }
  return k;
}

int mkvfix_ler_inicio(const unsigned char *b, long n, MkvfixCab *c) {
  int ui = 0, us = 0;
  int64_t s, o;
  memset(c, 0, sizeof *c);
  c->regiao = -1;
  if (lerId(b, n, &ui) != ID_EBML) return 0;
  s = lerTam(b + ui, n - ui, &us);
  if (s < 0 || ui + us + s > n) return 0;
  o = ui + us + s;
  if (lerId(b + o, (long)(n - o), &ui) != ID_SEGMENT) return 0;
  s = lerTam(b + o + ui, (long)(n - o - ui), &us);
  if (s == -2) return 0;
  c->segIni = o + ui + us;
  o = c->segIni;
  if (lerId(b + o, (long)(n - o), &ui) == ID_SEEKHEAD) {
    s = lerTam(b + o + ui, (long)(n - o - ui), &us);
    if (s < 0 || o + ui + us + s > n) return 1;
    c->n = mkvfix_ler_seekhead(b + o, (long)(n - o), c->id, c->pos, MKVFIX_ENTRADAS);
    if (c->n < 0) { c->n = 0; return 1; }
    c->regiao = o;
    c->regiaoN = (int)(ui + us + s);
    o += ui + us + s;
    // O Void logo atras e o espaco que o muxer reservou para o SeekHead crescer.
    if (lerId(b + o, (long)(n - o), &ui) == ID_VOID) {
      s = lerTam(b + o + ui, (long)(n - o - ui), &us);
      if (s >= 0 && ui + us + s + c->regiaoN <= 65536) c->regiaoN += (int)(ui + us + s);
    }
  }
  return 1;
}

int64_t mkvfix_entrada(const uint32_t *id, const int64_t *pos, int n, uint32_t alvo) {
  for (int i = 0; i < n; i++) if (id[i] == alvo) return pos[i];
  return -1;
}

// Largura minima do tamanho EBML `x` (todos os bits 1 e reservado).
static int larguraTam(uint64_t x) {
  int w = 1;
  while (w < 8 && x >= ((uint64_t)1 << (7 * w)) - 1) w++;
  return w;
}
static void gravarTam(unsigned char *o, uint64_t x, int w) {
  uint64_t v = x | ((uint64_t)1 << (7 * w));
  for (int i = w - 1; i >= 0; i--) { o[i] = (unsigned char)(v & 0xFF); v >>= 8; }
}
static int bytesDe(uint64_t v) { int k = 1; while (k < 8 && (v >> (8 * k))) k++; return k; }

static int entradaMontar(uint32_t id, int64_t pos, unsigned char *e) {
  int ni = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  int np = bytesDe((uint64_t)pos), k = 0, dentro = 3 + ni + 3 + np;
  e[k++] = 0x4D; e[k++] = 0xBB; e[k++] = (unsigned char)(0x80 | dentro);
  e[k++] = 0x53; e[k++] = 0xAB; e[k++] = (unsigned char)(0x80 | ni);
  for (int i = ni - 1; i >= 0; i--) e[k++] = (unsigned char)(id >> (8 * i));
  e[k++] = 0x53; e[k++] = 0xAC; e[k++] = (unsigned char)(0x80 | np);
  for (int i = np - 1; i >= 0; i--) e[k++] = (unsigned char)((uint64_t)pos >> (8 * i));
  return k;
}

// O resto `r` (bytes depois do SeekHead) tem forma: 0, ou um Void de r bytes
// (r >= 2). r == 1 se resolve alargando o tamanho do SeekHead.
static int restoOk(int r, int w) { return r == 0 || r >= 2 || (r == 1 && w < 8); }

int mkvfix_montar(const uint32_t *id, const int64_t *pos, int n, unsigned char *out, int len) {
  unsigned char cont[512], e[32];
  int cn = 0, entrou = 0, w, r, k;
  uint32_t usados[MKVFIX_ENTRADAS * 2]; int nu = 0;
  if (len < 4 + 1 + 21 || len > 65536) return 0;
  for (int i = 0; i < n; i++) {
    int el, dup = 0, t;
    if (pos[i] < 0) continue;
    for (int j = 0; j < nu; j++) if (usados[j] == id[i]) dup = 1;
    if (dup || cn + 32 > (int)sizeof cont) continue;
    el = entradaMontar(id[i], pos[i], e);
    t = cn + el;
    w = larguraTam((uint64_t)t);
    r = len - (4 + w + t);
    if (r < 0 || !restoOk(r, w)) continue;
    memcpy(cont + cn, e, (size_t)el); cn = t; entrou++;
    if (nu < (int)(sizeof usados / sizeof usados[0])) usados[nu++] = id[i];
  }
  if (!entrou) return 0;
  w = larguraTam((uint64_t)cn);
  r = len - (4 + w + cn);
  if (r == 1) { w++; r = 0; }
  k = 0;
  out[k++] = 0x11; out[k++] = 0x4D; out[k++] = 0x9B; out[k++] = 0x74;
  gravarTam(out + k, (uint64_t)cn, w); k += w;
  memcpy(out + k, cont, (size_t)cn); k += cn;
  if (r >= 2) {
    int v = 1, d = r - 2;
    if (d > 126) { for (v = 2; v <= 8; v++) { d = r - 1 - v; if (d >= 0 && larguraTam((uint64_t)d) <= v) break; } }
    out[k++] = 0xEC;
    gravarTam(out + k, (uint64_t)d, v); k += v;
    memset(out + k, 0, (size_t)d); k += d;
  }
  return k == len ? entrou : 0;
}

// ---- legendas de texto escondidas da TV (ver mkvfix.h) ----------------------
#define ID_TRACKS     0x1654AE6Bu
#define ID_TRACKENTRY 0xAEu
#define ID_TRACKTYPE  0x83u
#define ID_CODECID    0x86u

static int codecTexto(const char *c) {
  return !strncmp(c, "S_TEXT/", 7) || !strcmp(c, "S_ASS") || !strcmp(c, "S_SSA");
}
static int codecAss(const char *c) {
  return !strcmp(c, "S_TEXT/ASS") || !strcmp(c, "S_TEXT/SSA") || !strcmp(c, "S_ASS") || !strcmp(c, "S_SSA");
}

int mkvfix_legendas_texto(const unsigned char *b, long n, int64_t *off, int max) {
  int ui = 0, us = 0, k = 0, ass = 0;
  int64_t s, o, ini;
  if (lerId(b, n, &ui) != ID_TRACKS) return -1;
  s = lerTam(b + ui, n - ui, &us);
  if (s < 0 || ui + us + s > n) return -1;
  ini = ui + us; o = 0;
  while (o < s) {
    int a = 0, c = 0;
    uint32_t id = lerId(b + ini + o, (long)(s - o), &a);
    int64_t s2;
    if (!id) return -1;
    s2 = lerTam(b + ini + o + a, (long)(s - o - a), &c);
    if (s2 < 0 || o + a + c + s2 > s) return -1;
    if (id == ID_TRACKENTRY) {
      const unsigned char *v = b + ini + o + a + c;
      int64_t q = 0, tipoEm = -1; uint64_t tipo = 0; char codec[40] = "";
      while (q < s2) {
        int x = 0, y = 0;
        uint32_t i3 = lerId(v + q, (long)(s2 - q), &x);
        int64_t s3;
        if (!i3) break;
        s3 = lerTam(v + q + x, (long)(s2 - q - x), &y);
        if (s3 < 0 || q + x + y + s3 > s2) break;
        if (i3 == ID_TRACKTYPE && s3 <= 8) {
          tipo = lerUint(v + q + x + y, s3);
          tipoEm = s3 == 1 ? (int64_t)(v - b) + q + x + y : -1;   // so o valor de 1 byte se troca
        }
        else if (i3 == ID_CODECID && s3 < (int64_t)sizeof codec) {
          memcpy(codec, v + q + x + y, (size_t)s3); codec[s3] = 0;
        }
        q += x + y + s3;
      }
      // TrackType 0x11 = legenda, num byte: vira 0x20 sem mexer em tamanho.
      if (tipo == MKVFIX_TIPO_LEGENDA && codecTexto(codec)) {
        if (codecAss(codec)) ass = 1;
        if (tipoEm >= 0 && k < max) off[k++] = tipoEm;
      }
    }
    o += a + c + s2;
  }
  return ass ? k : 0;
}

int64_t mkvfix_tracks_pos(const unsigned char *b, long n, const MkvfixCab *c) {
  int64_t p = mkvfix_entrada(c->id, c->pos, c->n, ID_TRACKS), o;
  if (p >= 0) return c->segIni + p;
  // Sem entrada no SeekHead: os elementos do topo, em ordem, dentro do trecho.
  o = c->segIni;
  while (o < n) {
    int ui = 0, us = 0;
    uint32_t id = lerId(b + o, (long)(n - o), &ui);
    int64_t s;
    if (!id) return -1;
    if (id == ID_TRACKS) return o;
    if (id == 0x1F43B675u) return -1;   // Cluster: os dados comecaram, nao ha Tracks antes
    s = lerTam(b + o + ui, (long)(n - o - ui), &us);
    if (s < 0) return -1;
    o += ui + us + s;
  }
  return -1;
}

// ---- servidor ----------------------------------------------------------------
#if defined(__EMSCRIPTEN__) || defined(NV_ANDROID) || defined(NV_MKVFIX_SEM_REDE)   // tests: no network
void mkvfix_sondar(const char *u, const char *c, const char *o) { (void)u; (void)c; (void)o; }
void mkvfix_sondar_relay(const char *u, const char *c, const char *o) { mkvfix_sondar(u, c, o); }
int  mkvfix_relay(void) { return 0; }
void mkvfix_sondar_arquivo(const char *u, const char *c, int usenet, int item, int file) {
  (void)usenet; (void)item; (void)file; mkvfix_sondar(u, c, NULL);
}
int  mkvfix_estado(void) { return MKVFIX_NADA; }
int  mkvfix_ocultas(void) { return 0; }
int  mkvfix_url(char *out, size_t n) { (void)out; (void)n; return 0; }
int  mkvfix_e_url(const char *u) { (void)u; return 0; }
void mkvfix_parar(void) {}
#else

#include "rede.h"
#include "debrid.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MF_JANELA     16384      // primeira leitura: o SeekHead mora nos primeiros bytes
#define MF_VIVOS      8          // conexoes do player ao mesmo tempo
#define MF_PARADO_MS  25000L     // o CDN sem mandar byte por isto: desiste da conexao
#define MF_CAB        4096
#define MF_TRACKS_MAX (4L * 1024 * 1024)   // Tracks maior que isto: nao esconde (nunca visto: 243 KB)

typedef struct {
  unsigned id;
  char token[16];
  char *orig, *cab;              // o que o player tocava e os cabecalhos dele
  char *origem;                  // de onde pedir link novo (pode ser NULL)
  int usenet, item, file;        // selected TorBox file, owned by this session
  int relay;                     // #648: repasse puro de bytes (cabecalhos que a TV nao manda)
  pthread_mutex_t m;
  char up[8192];                 // o link de verdade (final, depois de redirect), sob m
  int upSemCab;                  // o final e de outra origem: sem cabecalhos
  int64_t total;
  int64_t regiao; int regiaoN;
  unsigned char conserto[512];
  int64_t oculta[MKVFIX_OCULTAS];  // byte (absoluto) do valor do TrackType de cada legenda escondida
  int nOculta;                     // publicado antes do estado final
  atomic_int estado;
  volatile int cancelar;
  atomic_int refs;
  atomic_uint pedidos;
} Sess;

static pthread_mutex_t G = PTHREAD_MUTEX_INITIALIZER;
static Sess *atual;
static unsigned proxId;
static int sockOuvir = -1, porta;
static atomic_int vivos;

static uint64_t agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)(t.tv_nsec / 1000000);
}

static void sessSoltar(Sess *s) {
  if (!s || atomic_fetch_sub(&s->refs, 1) != 1) return;
  pthread_mutex_destroy(&s->m);
  free(s->orig); free(s->cab); free(s->origem);
  free(s);
}
static Sess *sessPegarAtual(void) {
  Sess *s;
  pthread_mutex_lock(&G);
  s = atual;
  if (s) atomic_fetch_add(&s->refs, 1);
  pthread_mutex_unlock(&G);
  return s;
}

// O player so le pelo servidor quando ha o que trocar: o SeekHead (seek) ou
// TrackEntry de legenda escondidas (mkvfix.h).
static int servivel(Sess *s) {
  int e = atomic_load(&s->estado);
  return s->total > 0 && (e == MKVFIX_PRECISA || s->relay || (e != MKVFIX_SONDANDO && s->nOculta > 0));
}

static const char *nomeId(uint32_t id) {
  switch (id) {
    case 0x1549A966: return "Info"; case 0x1654AE6B: return "Tracks"; case 0x1043A770: return "Chapters";
    case 0x1941A469: return "Attachments"; case 0x1254C367: return "Tags"; case ID_CUES: return "Cues";
    case ID_SEEKHEAD: return "SeekHead"; case 0x1F43B675: return "Cluster";
    default: return "?";
  }
}
static void mb(char *d, size_t n, int64_t v) {
  if (v < 0) snprintf(d, n, "?");
  else if (v >= 1024LL * 1024 * 1024) snprintf(d, n, "%.2f GB", v / 1073741824.0);
  else if (v >= 1024LL * 1024) snprintf(d, n, "%.1f MB", v / 1048576.0);
  else if (v >= 1024) snprintf(d, n, "%lld KB", (long long)(v / 1024));
  else snprintf(d, n, "%lld B", (long long)v);
}

static void definirEstado(Sess *s, int e, const char *porque) {
  atomic_store(&s->estado, e);
  printf("[mkvfix] check: %s\n", porque);
  fflush(stdout);
}

static unsigned char *ler(Sess *s, int64_t ini, int64_t fim, long *n, int64_t *total, int *st) {
  char up[8192], fin[8192] = "";   // rede.c escreve ate 8192 no final
  int cross = 0, semCab;
  unsigned char *b;
  pthread_mutex_lock(&s->m);
  snprintf(up, sizeof up, "%s", s->up);
  semCab = s->upSemCab;
  pthread_mutex_unlock(&s->m);
  b = (unsigned char *)rede_baixar_trecho64_final(up, semCab ? "" : s->cab, ini, fim, n, total, st,
                                                  &s->cancelar, fin, &cross);
  if (b && fin[0] && strcmp(fin, up)) {
    pthread_mutex_lock(&s->m);
    snprintf(s->up, sizeof s->up, "%s", fin);
    s->upSemCab = s->upSemCab || cross;
    pthread_mutex_unlock(&s->m);
  }
  return b;
}

// A legenda de texto de um arquivo com ASS/SSA: le o Tracks e marca as
// TrackEntry a esconder da TV (mkvfix.h). Antes do estado final.
static void sondaLegendas(Sess *s, const unsigned char *b, long n, const MkvfixCab *c, int64_t total) {
  int64_t tp = mkvfix_tracks_pos(b, n, c), off[MKVFIX_OCULTAS];
  long m = 0; int st = 0, k, ui = 0, us = 0;
  int64_t tam;
  unsigned char *q = NULL;
  const unsigned char *t;
  if (tp < 0 || total <= 0 || tp >= total) {
    printf("[mkvfix] subtitles: no Tracks element found, subtitle tracks left to the TV\n"); fflush(stdout); return;
  }
  // O Tracks quase sempre ja veio no primeiro trecho; senao, um Range so dele.
  if (tp + 12 <= n && lerId(b + tp, n - tp, &ui) == ID_TRACKS &&
      (tam = lerTam(b + tp + ui, (long)(n - tp - ui), &us)) >= 0 && tp + ui + us + tam <= n) {
    t = b + tp; m = (long)(ui + us + tam);
  } else {
    q = ler(s, tp, tp + 15 < total ? tp + 15 : total - 1, &m, NULL, &st);
    if (s->cancelar || !q || lerId(q, m, &ui) != ID_TRACKS || (tam = lerTam(q + ui, m - ui, &us)) < 0 ||
        ui + us + tam > MF_TRACKS_MAX) {
      printf("[mkvfix] subtitles: Tracks not readable (HTTP %d), subtitle tracks left to the TV\n", st);
      fflush(stdout); free(q); return;
    }
    free(q);
    q = ler(s, tp, tp + ui + us + tam - 1, &m, NULL, &st);
    if (s->cancelar || !q) { free(q); return; }
    t = q;
  }
  k = mkvfix_legendas_texto(t, m, off, MKVFIX_OCULTAS);
  if (k > 0) {
    for (int i = 0; i < k; i++) s->oculta[i] = tp + off[i];
    s->nOculta = k;
    printf("[mkvfix] subtitles: %d text subtitle track(s) with ASS/SSA hidden from the TV player (the app draws them); "
           "the file plays through the local server\n", k);
  } else if (k < 0) printf("[mkvfix] subtitles: Tracks element unreadable, subtitle tracks left to the TV\n");
  fflush(stdout);
  free(q);
}

static void *fioSonda(void *u) {
  Sess *s = u;
  MkvfixCab c;
  long n = 0, m = 0;
  int st = 0, nSh2, nTodas = 0, entrou = 0, est = MKVFIX_NORMAL;
  int64_t total = -1, sh2, cues;
  uint32_t id2[MKVFIX_ENTRADAS], ids[MKVFIX_ENTRADAS * 2 + 1];
  int64_t pos2[MKVFIX_ENTRADAS], poss[MKVFIX_ENTRADAS * 2 + 1];
  char a[24], b2[24], d[24], msg[420] = "";
  uint64_t t0 = agoraMs();
  unsigned char *b = ler(s, 0, MF_JANELA - 1, &n, &total, &st), *q = NULL;
  if (s->cancelar) goto fim;
  if (!b || n < 64) {
    char x[96]; snprintf(x, sizeof x, "header not readable (HTTP %d, %ld bytes)", st, n);
    definirEstado(s, MKVFIX_FALHOU, x); goto fim;
  }
  if (!mkvfix_ler_inicio(b, n, &c)) { if (s->relay) s->total = total; definirEstado(s, MKVFIX_NORMAL, "not a Matroska file: nothing to do"); goto fim; }
  // Daqui em diante o indice (abaixo) e as legendas (sondaLegendas) sao
  // independentes: o estado so e publicado no fim, com as duas respostas.
  if (c.regiao < 0) { snprintf(msg, sizeof msg, "no SeekHead at the start of the Segment: left to the player"); goto legendas; }
  if (mkvfix_entrada(c.id, c.pos, c.n, ID_CUES) >= 0) {
    snprintf(msg, sizeof msg, "the first SeekHead lists the Cues (%d entries): nothing to fix (%u ms)",
             c.n, (unsigned)(agoraMs() - t0));
    goto legendas;
  }
  sh2 = mkvfix_entrada(c.id, c.pos, c.n, ID_SEEKHEAD);
  if (sh2 < 0 || total <= 0) {
    snprintf(msg, sizeof msg, "the first SeekHead (%d entries) has no Cues and no second SeekHead: no index to point to", c.n);
    est = MKVFIX_SEM_INDICE; goto legendas;
  }
  // O fim do pedido nunca passa do arquivo (o segundo SeekHead mora no fim).
  q = ler(s, c.segIni + sh2, c.segIni + sh2 + 4095 < total ? c.segIni + sh2 + 4095 : total - 1, &m, NULL, &st);
  if (s->cancelar) goto fim;
  nSh2 = q ? mkvfix_ler_seekhead(q, m, id2, pos2, MKVFIX_ENTRADAS) : -1;
  cues = nSh2 > 0 ? mkvfix_entrada(id2, pos2, nSh2, ID_CUES) : -1;
  free(q); q = NULL;
  if (cues < 0) {
    snprintf(msg, sizeof msg, "second SeekHead %s: no Cues listed", nSh2 < 0 ? "unreadable" : "read");
    est = nSh2 < 0 ? MKVFIX_FALHOU : MKVFIX_SEM_INDICE; goto legendas;
  }
  // Confere que ha um Cues de verdade la (nao se aponta o player para lixo).
  q = ler(s, c.segIni + cues, c.segIni + cues + 15 < total ? c.segIni + cues + 15 : total - 1, &m, NULL, &st);
  if (s->cancelar) goto fim;
  { int ui = 0;
    int achou = q && m >= 4 && lerId(q, m, &ui) == ID_CUES;
    free(q); q = NULL;
    if (!achou) {
      snprintf(msg, sizeof msg, "the second SeekHead points to Cues, but no Cues element is there");
      est = MKVFIX_SEM_INDICE; goto legendas;
    } }
  // Ordem: o Cues primeiro, depois o que o 1o SeekHead ja tinha (o caminho
  // para o segundo), depois o resto do segundo, enquanto couber.
  ids[nTodas] = ID_CUES; poss[nTodas++] = cues;
  for (int i = 0; i < c.n; i++) { ids[nTodas] = c.id[i]; poss[nTodas++] = c.pos[i]; }
  for (int i = 0; i < nSh2; i++) { ids[nTodas] = id2[i]; poss[nTodas++] = pos2[i]; }
  if (c.regiaoN > (int)sizeof s->conserto ||
      !(entrou = mkvfix_montar(ids, poss, nTodas, s->conserto, c.regiaoN))) {
    snprintf(msg, sizeof msg, "the SeekHead region (%d B) has no room for a Cues entry", c.regiaoN);
    est = MKVFIX_SEM_INDICE; goto legendas;
  }
  s->regiao = c.regiao; s->regiaoN = c.regiaoN;
  mb(a, sizeof a, c.segIni + sh2); mb(b2, sizeof b2, c.segIni + cues); mb(d, sizeof d, total);
  snprintf(msg, sizeof msg, "the first SeekHead (region %d B at %lld, %d entr%s) only points to a second SeekHead at %s, "
           "which lists the Cues at %s (file %s). The TV player does not follow it, so seeks go through the "
           "local fix: %d-byte region rewritten with %d entries (%u ms)",
           c.regiaoN, (long long)c.regiao, c.n, c.n == 1 ? "y" : "ies", a, b2, d, c.regiaoN, entrou,
           (unsigned)(agoraMs() - t0));
  est = MKVFIX_PRECISA;
legendas:
  sondaLegendas(s, b, n, &c, total);
  if (s->cancelar) goto fim;
  s->total = total;
  definirEstado(s, est, msg);
  if (est == MKVFIX_PRECISA) {
    char lista[160] = ""; size_t k = 0;
    uint32_t idN[MKVFIX_ENTRADAS]; int64_t posN[MKVFIX_ENTRADAS];
    int nN = mkvfix_ler_seekhead(s->conserto, s->regiaoN, idN, posN, MKVFIX_ENTRADAS);
    for (int i = 0; i < nN && k < sizeof lista - 16; i++)
      k += (size_t)snprintf(lista + k, sizeof lista - k, "%s%s", k ? ", " : "", nomeId(idN[i]));
    printf("[mkvfix] new SeekHead lists: %s\n", lista);
    fflush(stdout);
  }
fim:
  free(b); free(q);
  sessSoltar(s);
  return NULL;
}

static void sondar(const char *url, const char *cabecalhos, const char *origem,
                   int usenet, int item, int file, int relay) {
  Sess *s, *velho;
  pthread_t t;
  if (!url || (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) return;
  s = calloc(1, sizeof *s);
  if (!s) return;
  pthread_mutex_init(&s->m, NULL);
  s->orig = strdup(url); s->cab = strdup(cabecalhos ? cabecalhos : "");
  s->origem = origem && *origem && strcmp(origem, url) ? strdup(origem) : NULL;
  snprintf(s->up, sizeof s->up, "%s", url);
  s->usenet = usenet; s->item = item; s->file = file; s->relay = relay;
  s->total = -1; s->regiao = -1;
  atomic_store(&s->estado, MKVFIX_SONDANDO);
  atomic_store(&s->refs, 2);   // a sessao atual + o fio da sonda
  pthread_mutex_lock(&G);
  s->id = ++proxId;
  snprintf(s->token, sizeof s->token, "%08x%04x", (unsigned)(agoraMs() * 2654435761u) ^ (unsigned)(uintptr_t)s,
           s->id & 0xFFFF);
  velho = atual; atual = s;
  pthread_mutex_unlock(&G);
  if (velho) { velho->cancelar = 1; sessSoltar(velho); }
  if (!s->orig || !s->cab || pthread_create(&t, NULL, fioSonda, s) != 0) {
    atomic_store(&s->estado, MKVFIX_FALHOU);
    sessSoltar(s);
    return;
  }
  pthread_detach(t);
}

void mkvfix_sondar(const char *url, const char *cabecalhos, const char *origem) {
  sondar(url, cabecalhos, origem, 0, 0, 0, 0);
}
void mkvfix_sondar_relay(const char *url, const char *cabecalhos, const char *origem) {
  sondar(url, cabecalhos, origem, 0, 0, 0, 1);
}
void mkvfix_sondar_arquivo(const char *url, const char *cabecalhos, int usenet, int item, int file) {
  sondar(url, cabecalhos, NULL, usenet, item, file, 0);
}

int mkvfix_estado(void) {
  int e;
  pthread_mutex_lock(&G);
  e = atual ? atomic_load(&atual->estado) : MKVFIX_NADA;
  pthread_mutex_unlock(&G);
  return e;
}

int mkvfix_ocultas(void) {
  int k = 0;
  pthread_mutex_lock(&G);
  if (atual && atomic_load(&atual->estado) != MKVFIX_SONDANDO) k = atual->nOculta;
  pthread_mutex_unlock(&G);
  return k;
}

int mkvfix_relay(void) {
  int ok = 0;
  pthread_mutex_lock(&G);
  ok = atual && atual->relay && servivel(atual);
  pthread_mutex_unlock(&G);
  return ok;
}

void mkvfix_parar(void) {
  Sess *velho;
  pthread_mutex_lock(&G);
  velho = atual; atual = NULL;
  pthread_mutex_unlock(&G);
  if (velho) {
    unsigned p = atomic_load(&velho->pedidos);
    if (p) { printf("[mkvfix] session ended after %u request(s) from the player\n", p); fflush(stdout); }
    velho->cancelar = 1;
    sessSoltar(velho);
  }
}

int mkvfix_e_url(const char *url) {
  char pre[40];
  if (!url || porta <= 0) return 0;
  snprintf(pre, sizeof pre, "http://127.0.0.1:%d/", porta);
  return !strncmp(url, pre, strlen(pre));
}

// --- conexoes -------------------------------------------------------------------
static int enviar(int sock, const void *b, size_t n, Sess *s) {
  const char *p = b;
  while (n) {
    ssize_t r = send(sock, p, n, MSG_NOSIGNAL);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // Player parado (pausa, buffer cheio): espera; so o fim da sessao solta.
      if (s->cancelar) return 0;
      continue;
    }
    if (r <= 0) return 0;
    p += r; n -= (size_t)r;
  }
  return 1;
}

typedef struct {
  int sock;
  Sess *s;
  int64_t ini, fim, prox;     // bytes [ini, fim] pedidos; prox = o proximo a mandar
  int cabFeito, head, parcial, falhouEnvio;
  unsigned nr;
} Envio;

static int enviarCab(Envio *e) {
  char c[512];
  int64_t len = e->fim - e->ini + 1;
  const char *tipo = e->s->relay ? "application/octet-stream" : "video/x-matroska";
  if (e->parcial)
    snprintf(c, sizeof c, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\n"
             "Content-Range: bytes %lld-%lld/%lld\r\nContent-Length: %lld\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n",
             tipo, (long long)e->ini, (long long)e->fim, (long long)e->s->total, (long long)len);
  else
    snprintf(c, sizeof c, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\n"
             "Content-Length: %lld\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n", tipo, (long long)len);
  e->cabFeito = 1;
  return enviar(e->sock, c, strlen(c), e->s);
}

// O corpo que chega do CDN: troca os bytes da regiao do SeekHead e manda.
static int ralo(void *u, const char *dados, size_t n) {
  Envio *e = u;
  unsigned char tmp[16384];
  if (e->s->cancelar) return 0;
  if (!e->cabFeito && !enviarCab(e)) { e->falhouEnvio = 1; return 0; }
  while (n && e->prox <= e->fim) {
    size_t k = n > sizeof tmp ? sizeof tmp : n;
    int64_t sobra = e->fim - e->prox + 1, a, z;
    if ((int64_t)k > sobra) k = (size_t)sobra;
    memcpy(tmp, dados, k);
    a = e->prox > e->s->regiao ? e->prox : e->s->regiao;
    z = e->prox + (int64_t)k < e->s->regiao + e->s->regiaoN ? e->prox + (int64_t)k : e->s->regiao + e->s->regiaoN;
    if (a < z) memcpy(tmp + (a - e->prox), e->s->conserto + (a - e->s->regiao), (size_t)(z - a));
    // Legenda escondida: o TrackType dela vira "controle" (mkvfix.h).
    for (int i = 0; i < e->s->nOculta; i++)
      if (e->s->oculta[i] >= e->prox && e->s->oculta[i] < e->prox + (int64_t)k)
        tmp[e->s->oculta[i] - e->prox] = MKVFIX_TIPO_ESCONDIDA;
    if (!enviar(e->sock, tmp, k, e->s)) { e->falhouEnvio = 1; return 0; }
    e->prox += (int64_t)k; dados += k; n -= k;
  }
  return e->prox <= e->fim;
}

// O link do CDN venceu (403/404/410 antes do primeiro byte): pede um novo a
// origem com um Range minimo e troca o `up` da sessao.
static int renovar(Sess *s) {
  char fin[8192] = "";
  long n = 0; int st = 0, cross = 0;
  unsigned char *b;
  if (s->item > 0) {
    if (!debrid_tb_link(s->usenet, s->item, s->file, fin, sizeof fin) || !fin[0]) return 0;
    cross = 1;
  } else {
    if (!s->origem) return 0;
    b = (unsigned char *)rede_baixar_trecho64_final(s->origem, s->cab, 0, 0, &n, NULL, &st,
                                                    &s->cancelar, fin, &cross);
    free(b);
    if (!fin[0] || st != 206) return 0;
  }
  pthread_mutex_lock(&s->m);
  snprintf(s->up, sizeof s->up, "%s", fin);
  s->upSemCab = cross;
  pthread_mutex_unlock(&s->m);
  printf("[mkvfix] the CDN link was refused: a fresh one came from the source link\n");
  fflush(stdout);
  return 1;
}

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

static void responder(int sock, int cod, const char *msg, int64_t total) {
  char c[256];
  if (cod == 416) snprintf(c, sizeof c, "HTTP/1.1 416 %s\r\nContent-Range: bytes */%lld\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", msg, (long long)total);
  else snprintf(c, sizeof c, "HTTP/1.1 %d %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", cod, msg);
  (void)!send(sock, c, strlen(c), MSG_NOSIGNAL);
}

typedef struct { int sock; } Conexao;

static void *atender(void *u) {
  Conexao *cx = u;
  int sock = cx->sock;
  char b[MF_CAB], met[8] = "", alvo[256] = "", range[96] = "";
  size_t tot = 0;
  uint64_t fimPrazo = agoraMs() + 5000, t0;
  Sess *s = NULL;
  Envio e;
  free(cx);
  memset(&e, 0, sizeof e);
  { struct timeval tv = { 1, 0 }; setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv); }
  b[0] = 0;
  while (tot < sizeof b - 1 && !strstr(b, "\r\n\r\n") && !strstr(b, "\n\n")) {
    struct pollfd p = { sock, POLLIN, 0 };
    int resta = (int)(fimPrazo > agoraMs() ? fimPrazo - agoraMs() : 0);
    ssize_t r;
    if (resta <= 0 || poll(&p, 1, resta) <= 0) goto sai;
    r = recv(sock, b + tot, sizeof b - 1 - tot, 0);
    if (r <= 0) goto sai;
    tot += (size_t)r; b[tot] = 0;
  }
  if (sscanf(b, "%7s %255s", met, alvo) != 2) goto sai;
  s = sessPegarAtual();
  // GET /<token>/v.mkv — so a sessao atual e so ja consertada.
  if (!s || !servivel(s) || alvo[0] != '/' ||
      strncmp(alvo + 1, s->token, strlen(s->token)) || alvo[1 + strlen(s->token)] != '/') {
    responder(sock, 404, "Not Found", 0); goto sai;
  }
  if (strcmp(met, "GET") && strcmp(met, "HEAD")) { responder(sock, 405, "Method Not Allowed", 0); goto sai; }
  e.sock = sock; e.s = s; e.head = !strcmp(met, "HEAD");
  e.nr = atomic_fetch_add(&s->pedidos, 1) + 1;
  campo(b, "Range", range, sizeof range);
  e.ini = 0; e.fim = s->total - 1;
  if (range[0]) {
    long long a = -1, z = -1;
    const char *r = strstr(range, "bytes=");
    if (r && sscanf(r + 6, "%lld-%lld", &a, &z) >= 1 && a >= 0) {
      e.parcial = 1; e.ini = a;
      if (z >= a && z < s->total) e.fim = z;
    } else if (r && sscanf(r + 6, "-%lld", &z) == 1 && z > 0) {   // ultimos z bytes
      e.parcial = 1; e.ini = s->total > z ? s->total - z : 0;
    }
  }
  if (e.ini >= s->total) { responder(sock, 416, "Range Not Satisfiable", s->total); goto sai; }
  e.prox = e.ini;
  if (e.head) { enviarCab(&e); goto sai; }
  t0 = agoraMs();
  for (int tentativa = 0; tentativa < 2; tentativa++) {
    char up[8192];
    int semCab, st = 0, cross = 0, r;
    int64_t veio = 0, total = -1;
    pthread_mutex_lock(&s->m);
    snprintf(up, sizeof up, "%s", s->up); semCab = s->upSemCab;
    pthread_mutex_unlock(&s->m);
    r = rede_fluxo64(up, semCab ? "" : s->cab, e.prox, ralo, &e, &total, &st,
                     &s->cancelar, NULL, &cross, MF_PARADO_MS, &veio);
    if (e.prox > e.fim || e.falhouEnvio || s->cancelar) break;
    if (r < 0 && !e.cabFeito && (st == 403 || st == 404 || st == 410) && tentativa == 0 && renovar(s)) continue;
    if (r < 0 || e.prox <= e.fim) {
      if (!e.cabFeito) responder(sock, 502, "Bad Gateway", 0);
      printf("[mkvfix] #%u bytes %lld-%lld: the CDN stopped after %lld bytes (HTTP %d)\n", e.nr,
             (long long)e.ini, (long long)e.fim, (long long)(e.prox - e.ini), st);
      fflush(stdout);
    }
    break;
  }
  { int64_t len = e.prox - e.ini;
    int tocou = s->regiaoN > 0 && e.ini < s->regiao + s->regiaoN && e.prox > s->regiao;
    for (int i = 0; i < s->nOculta; i++) if (s->oculta[i] >= e.ini && s->oculta[i] < e.prox) tocou = 2;
    // Uma linha por pedido curto (cabecalho, Cues) e por pedido que tocou a
    // regiao; os longos (o video andando) so no fim, se pararem sem fim.
    if (tocou || (e.fim - e.ini) < 4 * 1024 * 1024 || e.nr <= 3)
      printf("[mkvfix] #%u %s bytes %lld-%lld: sent %lld in %u ms%s\n", e.nr, e.parcial ? "Range" : "whole",
             (long long)e.ini, (long long)e.fim, (long long)len, (unsigned)(agoraMs() - t0),
             tocou == 2 ? " (subtitle tracks hidden)" : tocou ? " (SeekHead rewritten)" : e.falhouEnvio ? " (player closed)" : "");
    fflush(stdout); }
sai:
  shutdown(sock, SHUT_RDWR);
  close(sock);
  if (s) sessSoltar(s);
  atomic_fetch_sub(&vivos, 1);
  return NULL;
}

static void *ouvir(void *u) {
  (void)u;
  for (;;) {
    int c = accept(sockOuvir, NULL, NULL);
    Conexao *cx;
    pthread_t t;
    if (c < 0) { if (errno == EINTR) continue; usleep(100000); continue; }
    if (atomic_fetch_add(&vivos, 1) >= MF_VIVOS) {
      atomic_fetch_sub(&vivos, 1);
      responder(c, 503, "Busy", 0); close(c); continue;
    }
    cx = malloc(sizeof *cx);
    if (!cx) { atomic_fetch_sub(&vivos, 1); close(c); continue; }
    cx->sock = c;
    if (pthread_create(&t, NULL, atender, cx) != 0) { free(cx); atomic_fetch_sub(&vivos, 1); close(c); continue; }
    pthread_detach(t);
  }
  return NULL;
}

static int subir(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  int um = 1, s;
  pthread_t t;
  if (sockOuvir >= 0) return 1;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return 0;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(s, (struct sockaddr *)&a, sizeof a) < 0 || listen(s, 16) < 0 ||
      getsockname(s, (struct sockaddr *)&a, &al) < 0) { close(s); return 0; }
  sockOuvir = s; porta = ntohs(a.sin_port);
  if (pthread_create(&t, NULL, ouvir, NULL) != 0) { close(s); sockOuvir = -1; porta = 0; return 0; }
  pthread_detach(t);
  printf("[mkvfix] local server on 127.0.0.1:%d\n", porta);
  fflush(stdout);
  return 1;
}

int mkvfix_url(char *out, size_t n) {
  int ok = 0;
  pthread_mutex_lock(&G);
  if (atual && servivel(atual) && subir()) {
    snprintf(out, n, "http://127.0.0.1:%d/%s/v.mkv", porta, atual->token);
    ok = 1;
  }
  pthread_mutex_unlock(&G);
  return ok;
}
#endif
