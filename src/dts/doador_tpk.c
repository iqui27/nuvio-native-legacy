// AUDIO FROM ANOTHER SOURCE on the Samsung .tpk (donor, owner 15/11): see
// doador_tpk.h. The server is the one of TorBox burn-in Passthrough (branch
// subtitle-rewrite, src/dts/tbaudio_tpk.c), without TorBox: the video is the
// local DTS converter's in video-only mode, the audio renditions come from the
// playing file and from a donor file.
#include "doador_tpk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <math.h>

// ---- which audio the match compares (pure, doador_tpk.h) ----------------------
static int mcCasa(int (*casa)(const char *, const char *), const char *a, const char *b) {
  if (!a[0] || !b[0]) return 0;
  return casa ? casa(a, b) || casa(b, a) : !strcasecmp(a, b);
}
// The first usable track of source `fonte` in language l (`pref` first when it is one).
static int mcAchar(const DoaMatchFaixa *fx, int n, int fonte, const char *l, int pref, int (*casa)(const char *, const char *)) {
  int i;
  if (!l || !l[0]) return -1;
  if (pref >= 0 && pref < n && fx[pref].fonte == fonte && fx[pref].ok && mcCasa(casa, fx[pref].lingua, l)) return pref;
  for (i = 0; i < n; i++) if (fx[i].fonte == fonte && fx[i].ok && mcCasa(casa, fx[i].lingua, l)) return i;
  return -1;
}
int doa_match_escolher(const DoaMatchFaixa *fx, int n, int tocando, const char *orig, const char *pLingua, const char *pCodec,
                       int pCanais, int leve, int (*casa)(const char *, const char *), DoaMatchEscolha *o) {
  const DoaMatchFaixa *d;
  int i, j, f;
  memset(o, 0, sizeof *o);
  o->siA = o->siA2 = o->siD = -1;
  if (!fx || tocando < 0 || tocando >= n || fx[tocando].fonte != 1) return 0;
  d = &fx[tocando];
  // 1. Picked by hand (Scan again): that file track, the most like the pick;
  //    the donor's track in its language (the playing one when it is).
  if (pLingua && pLingua[0]) {
    int melhor = -1, mp = -1;
    for (i = 0; i < n; i++) {
      int p;
      if (fx[i].fonte != 0 || !fx[i].ok || !mcCasa(casa, fx[i].lingua, pLingua)) continue;
      p = (pCodec && pCodec[0] && !strcmp(fx[i].codec, pCodec) ? 2 : 0) + (pCanais && fx[i].canais == pCanais ? 1 : 0);
      if (p > mp) { mp = p; melhor = i; }
    }
    if (melhor >= 0) {
      int dd = mcAchar(fx, n, 1, fx[melhor].lingua, tocando, casa);
      o->siA = fx[melhor].si; o->siD = dd >= 0 && dd != tocando ? fx[dd].si : -1; o->como = DOA_MC_PEDIDA;
      snprintf(o->porque, sizeof o->porque, "picked in Scan again: the file's %s %s track %d against the donor's %s track %d%s",
               fx[melhor].lingua, fx[melhor].codec, fx[melhor].si, dd >= 0 ? fx[dd].lingua : d->lingua, dd >= 0 ? fx[dd].si : d->si,
               dd < 0 ? " (two languages)" : "");
      return 1;
    }
  }
  // 2. The original language.
  if (orig && orig[0]) {
    int fo = mcAchar(fx, n, 0, orig, -1, casa), dor = mcAchar(fx, n, 1, orig, tocando, casa);
    if (fo >= 0 && dor >= 0) {
      o->siA = fx[fo].si; o->siD = dor != tocando ? fx[dor].si : -1; o->como = DOA_MC_ORIGINAL;
      snprintf(o->porque, sizeof o->porque, "the original language (%s) on both sides: the file's track %d, the donor's track %d%s", orig,
               fx[fo].si, fx[dor].si, dor != tocando ? " (read from the donor's reads, not the playing one)" : "");
      return 1;
    }
    if (fo >= 0) {
      int sl = mcAchar(fx, n, 0, d->lingua, -1, casa);
      if (sl >= 0 && sl != fo && !leve) {
        o->siA = fx[sl].si; o->siA2 = fx[fo].si; o->como = DOA_MC_DUAS;
        snprintf(o->porque, sizeof o->porque, "the donor has no %s: its %s track against the file's %s track %d AND its %s track %d, scores added",
                 orig, d->lingua, d->lingua, fx[sl].si, fx[fo].lingua, fx[fo].si);
        return 1;
      }
      if (sl >= 0 && sl != fo && leve) {
        o->siA = fx[fo].si; o->como = DOA_MC_ORIG_LEVE;
        snprintf(o->porque, sizeof o->porque, "the donor has no %s; one file track only (older TV or low memory): the original, track %d",
                 orig, fx[fo].si);
        return 1;
      }
      if (sl < 0) {
        // No track in the playing one's language: a shared language first
        // (the same speech), else the original against the playing track.
        for (j = 0; j < n; j++) {
          if (fx[j].fonte != 1 || !fx[j].ok || j == tocando || !fx[j].lingua[0]) continue;
          if ((f = mcAchar(fx, n, 0, fx[j].lingua, -1, casa)) >= 0) {
            o->siA = fx[f].si; o->siD = fx[j].si; o->como = DOA_MC_COMUM;
            snprintf(o->porque, sizeof o->porque, "the file has no %s track: the donor's %s track %d against the file's %s track %d", d->lingua,
                     fx[j].lingua, fx[j].si, fx[f].lingua, fx[f].si);
            return 1;
          }
        }
        o->siA = fx[fo].si; o->como = DOA_MC_OUTRA;
        snprintf(o->porque, sizeof o->porque, "no language in both: the file's original (%s) track %d against the donor's %s (weaker)", orig,
                 fx[fo].si, d->lingua[0] ? d->lingua : "und");
        return 1;
      }
    }
  }
  // 3. As before: the playing track's language, then a shared one, then any.
  if ((f = mcAchar(fx, n, 0, d->lingua, -1, casa)) >= 0) {
    o->siA = fx[f].si; o->como = DOA_MC_LINGUA;
    snprintf(o->porque, sizeof o->porque, "the playing track's language (%s): the file's track %d", d->lingua, fx[f].si);
    return 1;
  }
  for (j = 0; j < n; j++) {
    if (fx[j].fonte != 1 || !fx[j].ok || j == tocando || !fx[j].lingua[0]) continue;
    if ((f = mcAchar(fx, n, 0, fx[j].lingua, -1, casa)) >= 0) {
      o->siA = fx[f].si; o->siD = fx[j].si; o->como = DOA_MC_COMUM;
      snprintf(o->porque, sizeof o->porque, "the file has no %s track: the donor's %s track %d against the file's %s track %d",
               d->lingua[0] ? d->lingua : "und", fx[j].lingua, fx[j].si, fx[f].lingua, fx[f].si);
      return 1;
    }
  }
  for (i = 0; i < n; i++)
    if (fx[i].fonte == 0 && fx[i].ok) {
      o->siA = fx[i].si; o->como = DOA_MC_OUTRA;
      snprintf(o->porque, sizeof o->porque, "no language in both: the file's first track %d (weaker)", fx[i].si);
      return 1;
    }
  return 0;
}
int doa_match_somar(const double *oa, const double *sa, int na, const double *ob, const double *sb, int nb, double tol,
                    double *o, double *media, double *scA, double *scB, int max) {
  double off[16], a[16], b[16];
  int n = 0, i, j, k;
  for (k = 0; k < 2; k++) {
    const double *ox = k ? ob : oa, *sx = k ? sb : sa;
    int nx = k ? nb : na;
    for (i = 0; i < nx; i++) {
      for (j = 0; j < n; j++) if (fabs(off[j] - ox[i]) <= tol) break;
      if (j == n) {
        if (n == 16) continue;
        off[n] = ox[i]; a[n] = b[n] = 0; n++;
      }
      if (k) { if (sx[i] > b[j]) { b[j] = sx[i]; if (sx[i] > a[j]) off[j] = ox[i]; } }
      else if (sx[i] > a[j]) { a[j] = sx[i]; if (sx[i] >= b[j]) off[j] = ox[i]; }
    }
  }
  for (i = 0; i < n && i < max; i++) {
    int m = i;
    for (j = i + 1; j < n; j++) if (a[j] + b[j] > a[m] + b[m]) m = j;
    if (m != i) { double t; t = off[i]; off[i] = off[m]; off[m] = t; t = a[i]; a[i] = a[m]; a[m] = t; t = b[i]; b[i] = b[m]; b[m] = t; }
    o[i] = off[i]; media[i] = (a[i] + b[i]) / 2.0;
    if (scA) scA[i] = a[i];
    if (scB) scB[i] = b[i];
  }
  return n < max ? n : max;
}

#if defined(NV_WEBOS) && defined(NV_DTS_FFMPEG) && !defined(NV_TPK) && !defined(NV_DOADOR_TEST)
// LG: the same calls over its converter, in doador_webos.c (owner 26/11).
#elif !defined(NV_DTS_FFMPEG) || !(defined(NV_TPK) || defined(NV_DOADOR_TEST))
int  doa_disponivel(void) { return 0; }
int  doa_abrir(const DoaAbrir *o, char *out, size_t n) { (void)o; if (out && n) out[0] = 0; return 0; }
void doa_fechar(void) {}
int  doa_ativo(void) { return 0; }
int  doa_pronto(void) { return 0; }
int  doa_falhou(char *m, size_t n) { if (m && n) m[0] = 0; return 0; }
int  doa_rendicoes(DoaRendicao *out, int max) { (void)out; (void)max; return 0; }
int  doa_rendicao_de(int chave) { (void)chave; return -1; }
void doa_definir_audio(int chave) { (void)chave; }
int  doa_audio(void) { return -1; }
void doa_desloc(int ms) { (void)ms; }
int  doa_desloc_ms(void) { return 0; }
int  doa_estado(char *m, size_t n) { if (m && n) m[0] = 0; return 0; }
void doa_log(void) {}
void doa_definir_casa(int (*casa)(const char *, const char *)) { (void)casa; }
int  doa_diferenca(int *c, int *v, int *p) { (void)c; (void)v; (void)p; return 0; }
int  doa_sondar(const char *u, const char *c, DoaSonda *o, int m) { (void)u; (void)c; (void)o; (void)m; return -1; }
int  doa_preabrir(const char *u, const char *c, int p, char *m, size_t n) { (void)u; (void)c; (void)p; if (m && n) m[0] = 0; return 0; }
void doa_dopasuj(int a) { (void)a; }
int  doa_dopasuj_resultado(DoaDopasuj *o, int *a) { (void)o; (void)a; return 0; }
int  doa_dopasuj_ativo(void) { return 0; }
int  doa_doador_saude(int *l) { if (l) *l = 0; return 0; }
int  doa_seguir_evento(int *ms) { (void)ms; return 0; }
int  doa_pedidos_audio(void) { return 0; }
int  doa_pedidos(void) { return 0; }
int  doa_legendas(int f, DoaLegInfo *o, int m) { (void)f; (void)o; (void)m; return 0; }
int  doa_legenda_tempos(int f, int k, double *t, int m) { (void)f; (void)k; (void)t; (void)m; return 0; }
void doa_aplicar_legendas(double k, double d, double c) { (void)k; (void)d; (void)c; }
void doa_dopasuj_fino(void) {}
void doa_mapa_definir(const DoaMapa *m, double p) { (void)m; (void)p; }
void doa_posicao(double p) { (void)p; }
int  doa_mapa_trecho(double p, double *i, double *f, int *r) { (void)p; (void)i; (void)f; (void)r; return -1; }
void doa_dopasuj_fino_trecho(int t) { (void)t; }
void doa_match_definir(const char *o, const char *l, const char *c, int n, int v) { (void)o; (void)l; (void)c; (void)n; (void)v; }
int  doa_faixas_arquivo(DoaRendicao *out, int max) { (void)out; (void)max; return 0; }
#else

#include "dts_engine.h"
#include "dts_remux.h"
#include "dts_match.inc"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#define TBA_FAIXAS     32
#define TBA_FONTES     2                 // the playing file, and one donor
#define TBA_CONEXOES   24                // the player may open every rendition at once
#define ESPERA_PREP_MS 30000
#define FILA_MAX       512
#define AUD_SEG_S      2.0               // audio segments: a seek reads ~2 s
// A donor that does not open by then is dropped: the reader retries a dead
// link for ~30 s (expired, not cached), and the player waits for the session.
#define DOADOR_PRAZO_MS 10000

// ---- S2: connections to the files' hosts in /proc/net/tcp (logging only) ----
#define SK_IPS 24
static pthread_mutex_t skTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned skIp[SK_IPS];
static int skIpUrl[SK_IPS];          // which watched URL (0-3) the address came from (subtitle-rewrite 9c62d57)
static int skN = -1;
static char *skUrl[4];
static void skHost(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  h[0] = 0;
  if (!p) return;
  p += 3; k = strcspn(p, "/?#:");
  { const char *at = memchr(p, '@', k); if (at) { k -= (size_t)(at + 1 - p); p = at + 1; } }
  snprintf(h, n, "%.*s", (int)k, p);
}
static void *skResolver(void *u) {
  unsigned ip[SK_IPS];
  int ipUrl[SK_IPS];
  char hosts[4][256];
  int n = 0, i, j;
  (void)u;
  pthread_mutex_lock(&skTrava);
  for (i = 0; i < 4; i++) { skHost(skUrl[i], hosts[i], sizeof hosts[i]); free(skUrl[i]); skUrl[i] = NULL; }
  pthread_mutex_unlock(&skTrava);
  for (i = 0; i < 4; i++) {
    struct addrinfo dica, *r = NULL, *a;
    if (!hosts[i][0] || !strcmp(hosts[i], "127.0.0.1")) continue;
    for (j = 0; j < i; j++) if (!strcmp(hosts[j], hosts[i])) break;
    if (j < i) continue;
    memset(&dica, 0, sizeof dica); dica.ai_family = AF_INET; dica.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hosts[i], NULL, &dica, &r) != 0) continue;
    for (a = r; a && n < SK_IPS; a = a->ai_next) {
      unsigned v = ((struct sockaddr_in *)a->ai_addr)->sin_addr.s_addr;
      for (j = 0; j < n; j++) if (ip[j] == v) break;
      if (j == n) { ipUrl[n] = i; ip[n++] = v; }
    }
    freeaddrinfo(r);
  }
  pthread_mutex_lock(&skTrava);
  memcpy(skIp, ip, sizeof ip); memcpy(skIpUrl, ipUrl, sizeof ipUrl); skN = n;
  pthread_mutex_unlock(&skTrava);
  printf("[doador] sockets: watching %d address(es) of the files' hosts\n", n);
  fflush(stdout);
  return NULL;
}
static void skVigiar(const char *u1, const char *u2, const char *u3, const char *u4) {
  const char *u[4] = { u1, u2, u3, u4 };
  pthread_t t;
  int i;
  pthread_mutex_lock(&skTrava);
  for (i = 0; i < 4; i++) { free(skUrl[i]); skUrl[i] = u[i] && u[i][0] ? strdup(u[i]) : NULL; }
  skN = -1;
  pthread_mutex_unlock(&skTrava);
  if (pthread_create(&t, NULL, skResolver, NULL) == 0) pthread_detach(t);
}
// Inodes of this process's sockets (/proc/self/fd -> "socket:[N]").
static int skMeus(unsigned long *v, int max) {
  DIR *d = opendir("/proc/self/fd");
  struct dirent *e;
  int n = 0;
  if (!d) return -1;
  while ((e = readdir(d)) != NULL && n < max) {
    char c[300], alvo[64];
    ssize_t k;
    snprintf(c, sizeof c, "/proc/self/fd/%s", e->d_name);
    k = readlink(c, alvo, sizeof alvo - 1);
    if (k <= 0) continue;
    alvo[k] = 0;
    if (!strncmp(alvo, "socket:[", 8)) v[n++] = strtoul(alvo + 8, NULL, 10);
  }
  closedir(d);
  return n;
}
static int skArquivo(const char *nome, const unsigned *ip, const int *ipUrl, int nIp, const unsigned long *meus, int nMeus,
                     int *eu, int *por) {
  FILE *f = fopen(nome, "r");
  char l[512];
  int tot = 0;
  if (!f) return -1;
  if (!fgets(l, sizeof l, f)) { fclose(f); return 0; }
  while (fgets(l, sizeof l, f)) {
    char rem[64], st[8];
    unsigned long ino = 0;
    unsigned v = 0;
    int i, ok = 0;
    if (sscanf(l, "%*s %*s %63s %7s %*s %*s %*s %*s %*s %lu", rem, st, &ino) < 3) continue;
    if (strcmp(st, "01")) continue;   // ESTABLISHED
    if (strlen(rem) >= 13 && rem[8] == ':') v = (unsigned)strtoul(rem, NULL, 16);   // tcp
    else if (strlen(rem) >= 37 && rem[32] == ':' && !strncmp(rem, "0000000000000000", 16) &&
             !strncmp(rem + 16, "FFFF0000", 8)) {   // tcp6 ::ffff:a.b.c.d (words in host order)
      char w[9];
      memcpy(w, rem + 24, 8); w[8] = 0;
      v = (unsigned)strtoul(w, NULL, 16);
    } else continue;
    for (i = 0; i < nIp; i++) if (ip[i] == v) { ok = 1; break; }
    if (!ok) continue;
    tot++;
    { int u = ipUrl[i], k;
      for (k = 0; k < nMeus; k++) if (meus[k] == ino) { (*eu)++; if (por && u >= 0 && u < 4) por[u]++; break; } }
  }
  fclose(f);
  return tot;
}
static int skContar(int *nuvio, int por[4]) {
  unsigned ip[SK_IPS];
  int ipUrl[SK_IPS];
  unsigned long meus[256];
  int n, nm, a, b, eu = 0;
  if (nuvio) *nuvio = -1;
  if (por) memset(por, 0, 4 * sizeof *por);
  pthread_mutex_lock(&skTrava);
  n = skN; if (n > 0) { memcpy(ip, skIp, sizeof ip); memcpy(ipUrl, skIpUrl, sizeof ipUrl); }
  pthread_mutex_unlock(&skTrava);
  if (n <= 0) return -1;
  nm = skMeus(meus, 256);
  a = skArquivo("/proc/net/tcp", ip, ipUrl, n, meus, nm > 0 ? nm : 0, &eu, por);
  b = skArquivo("/proc/net/tcp6", ip, ipUrl, n, meus, nm > 0 ? nm : 0, &eu, por);
  if (a < 0 && b < 0) return -1;
  if (nuvio) *nuvio = nm >= 0 ? eu : -1;
  return (a > 0 ? a : 0) + (b > 0 ? b : 0);
}

enum { M_TS = 0, M_MP4, M_AC3, M_AAC, M_FORA, M_FLAC };
// Decoded and encoded again (AC3, AAC, or FLAC for PCM).
#define CONVERTE(m) ((m) == M_AC3 || (m) == M_AAC || (m) == M_FLAC)
// Goes out as fragmented MP4 (copied FLAC/Opus, or PCM made FLAC).
#define EM_MP4(m) ((m) == M_MP4 || (m) == M_FLAC)
// In the master the TV gets: served, and not left out by the rendition limit.
#define LISTADA(f) ((f)->modo != M_FORA && !(f)->foraLista)
static const char *const MODO_NOME[] = { "copied (MPEG-TS)", "copied (fMP4)", "to AC3", "to AAC", "not offered", "to FLAC (fMP4, lossless)" };

typedef struct {
  int si, ord, canais, taxa, modo;
  int fonte, chave;                    // fo[] it comes from; the app's key (ord, or DOA_DOADOR(ord))
  int foraLista;                       // left out of the master by the rendition limit (DoaAbrir.rend_limite)
  enum AVCodecID id;
  char lingua[16], nome[96], codec[24];
} Faixa;

// An audio source: the playing file (fo[0]) or the donor (fo[1]).
typedef struct {
  char *url, *cab;
  char nome[96];                       // donor: the app's label for it ("" for the file itself)
  DtsEngine *eng;
  AVFormatContext *ic;
  double v0, dur, fim;                 // its video's start, its length, the container's end (s)
  double desloc;                       // its time * k + desloc = the playing file's time (donor: starts + offset)
  double k;                            // its speed against the file (1; a PAL version 25/23.976)
  int sBusca;                          // the stream seeks go through (the one the index covers)
  int leu;                             // the demuxer has read packets (a seek is needed for a fresh start)
  int conectada;                       // its reader holds a connection (0 after dts_engine_release)
  int falhou; char motivo[160];
  long nSoltas, nVoltas;               // connections given back, connections taken again
  double msVolta;                      // last reconnection: request to its segment out
  // Its embedded TEXT subtitle tracks as the index (Cues) lists them: the
  // start of every indexed block, in its own time. Read from the seek index
  // prepFonte already loads: no byte more (the subtitle match, doadorleg.c).
  DoaLegInfo leg[DOA_LEG_MAX]; double *legT[DOA_LEG_MAX]; int nLeg;
} Fonte;

typedef struct { AVPacket *p; double t; } Item;   // t: the playing file's time (s) of the packet

// One side of the automatic match: its audio decoded to band envelopes.
typedef struct {
  AVCodecContext *dec; SwrContext *swr; AVFrame *fr;
  DtsMatchEnv ev; double t0, ult; int cap, temT0, si;
  float *env[DTS_MATCH_BANDS];
} Lado;

typedef struct Sessao {
  atomic_int refs;
  volatile int cancel;
  pthread_mutex_t m;                   // state below
  pthread_cond_t c;
  int pronto, falhou, flacConv, audio;   // audio: index in fx[]
  int variante;                        // DoaAbrir.variante: 1 = CODECS + capped BANDWIDTH on the master (4K test)
  int rendLimite;                      // DoaAbrir.rend_limite: 0 all renditions, else at most this many
  uint64_t plDito;                     // audio playlists whose first lines are in the log (bit = fx index)
  atomic_int pedAudio, pedVideo, pedTotal;   // requests seen: audio segments/inits, the master, everything
  double abertoMs;                     // when doa_abrir opened it (request log)
  char motivo[160], tok[40];
  char *video;                         // the local converter's media playlist (absolute URL)
  Fonte fo[TBA_FONTES]; int nfo;
  int umaConexao;                      // one audio connection: the source not playing gives its connection back
  char doadorLingua[64];               // the donor track to start on: these languages in order (comma-separated)
  int doadorFaixa;                     // the donor stream picked by hand (+1; 0 none)
  char dicaLingua[16], dicaCodec[24], dicaTitulo[96]; int dicaCanais;   // series memory's track
  int difOk, difCorte, difVel, difPpm; // doa_diferenca
  double fpsRazao;                     // the donor's frame rate / the file's (0 unknown)
  double legConf, legDesloc;           // the last subtitle match (confidence 0 none; fo[1].desloc it set)
  int legMs;                           // its offset (ms): a map installed while it is still in force is taken as it is
  int legOk;                           // the subtitles placed this donor: no follow (owner 18/11, the edit map does it)
  // THE EDIT MAP (doa_mapa_definir, owner 18/11): the file's stretches, each
  // with its own offset (ms against baseDe, as doadorMs). mpm guards it alone
  // (no other lock is taken inside): the generator reads it per segment, the
  // app's thread writes it.
  pthread_mutex_t mpm;
  int mapaN;                           // 0 none: fo[1].desloc everywhere
  struct { double ini, fim; int ms, legMs, refinado; } mapa[DOA_MAPA_MAX];
  int mapaPos;                         // the stretch at the playhead (doa_posicao); -1 unknown
  double posApp;                       // the playhead, from the app (doa_posicao)
  int fino1Pend;                       // the first refine's result went to the app: its doa_desloc marks the stretch refined
  int mTrecho;                         // the running refine is this stretch's own (-1: the app's)
  int ppmPedido;                       // DoaAbrir.doador_ppm
  // automatic match (mm guards the sides while a tap feeds them)
  pthread_mutex_t mm;
  int mEstado, mAuto, mNovo;           // 0 idle, 1 running, 2 found, 3 not found; mNovo: a result not taken yet
  int mFino;                           // the running match only refines (+-1 s around the offset, no wide search)
  int mArmD;                           // the donor's generator feeds mDon
  Lado mArq, mDon;
  Lado mArq2;                          // a second file track scored with the first (doa_match_escolher DOA_MC_DUAS); si -1 none
  DoaMatchEscolha mEsc;                // the tracks the running match compares
  int dExtra;                          // a donor stream the generator demuxes for the match (not the playing one; -1 none)
  DtsEngine *mEng;                     // a wide search's own reader (cancelled on close)
  DoaDopasuj mRes;
  // following another cut
  int seguir, segAtivo, segFalhas, segPerdido, segEvento, segMs;
  int dFalhas, dSemResposta, dLento, dLentoDito;   // the donor's segments: failures in a row, slow ones
  double segProx;                      // the file's time of the next look (s)
  int doadorPadrao;                    // start on the donor's track (else on `audio`)
  int doadorMs;                        // the donor's offset the app set (ms, positive = later)
  int doadorFeito;                     // its preparation ended (the watchdog stops); under m
  int doadorVencido;                   // the watchdog cancelled its reader
  double dur;                          // the playlist's length: the file's container end
  double *aIni; int naSeg;             // audio segments (AUD_SEG_S)
  long banda; int largura, altura; long fps1000; char faixaVideo[8];   // the master's STREAM-INF
  char vCodecs[40];                    // the video's RFC 6381 name (the master's form 1), "" unknown
  Faixa fx[TBA_FAIXAS]; int nfx;
  // generator (gm)
  pthread_mutex_t gm;
  int gOk, gFaixa, gSeg, gModo, gFonte;  // gFonte: the source the generator read last (-1 none)
  double gDesloc;                      // that source's desloc when the generator positioned
  const double *gIni;                  // the segment list gSeg counts in
  atomic_int espera;                   // requests waiting for gm
  atomic_long espChave;                // the last one: (track+1) * 1000000 + segment
  Item fila[FILA_MAX]; int fIni, fN;
  AVPacket *pk;
  AVCodecContext *dec, *enc;
  SwrContext *swr;
  AVAudioFifo *fifo;
  AVFrame *fr, *ef;
  int64_t base48, amostras;
  int eof;
  // Laced frames (subtitle-rewrite cde2770): the reading track's next
  // timestamp; packets that came without one follow it. Reset on every seek.
  int64_t proxPts;
  int semPts, semPtsLog;               // packets without a timestamp filled in / lines logged
  long lidos, guardados;               // this segment: packets read from the source / of the track
  int diagLog;                         // FLAC/Opus segment lines logged (first 3)
  // log
  long nSeg, nErro, nLarg; long long bytes; double msLer;
  char ultErro[160];
} Sessao;

static double deslocEm(Sessao *s, double t);
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int (*casaLingua)(const char *, const char *);
void doa_definir_casa(int (*casa)(const char *, const char *)) { casaLingua = casa; }
// doa_match_definir (the app, main thread; read when a match starts).
static char mcOrig[16], mcLing[16], mcCodec[24];
static int mcCanais, mcLeve;
void doa_match_definir(const char *orig, const char *pedidaLingua, const char *pedidaCodec, int pedidaCanais, int leve) {
  pthread_mutex_lock(&trava);
  snprintf(mcOrig, sizeof mcOrig, "%s", orig ? orig : "");
  snprintf(mcLing, sizeof mcLing, "%s", pedidaLingua ? pedidaLingua : "");
  snprintf(mcCodec, sizeof mcCodec, "%s", pedidaCodec ? pedidaCodec : "");
  mcCanais = pedidaCanais; mcLeve = leve;
  pthread_mutex_unlock(&trava);
}
static Sessao *atual;
static int ouvinte = -1, porta;
static atomic_int vivos;

static double agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static void gerarToken(char *t, int n) {
  static const char A[] = "abcdefghjkmnpqrstuvwxyz23456789";
  unsigned char b[32];
  int f = open("/dev/urandom", O_RDONLY), k = 0, i;
  ssize_t r = f >= 0 ? read(f, b, sizeof b) : -1;
  if (f >= 0) close(f);
  if (r < (ssize_t)sizeof b) {
    uint64_t x = (uint64_t)agoraMs() * 6364136223846793005ULL + (uint64_t)getpid();
    for (i = 0; i < (int)sizeof b; i++) { x = x * 6364136223846793005ULL + 1442695040888963407ULL; b[i] = (unsigned char)(x >> 33); }
  }
  for (i = 0; i < (int)sizeof b && k < n - 1; i++) if (b[i] < 248) t[k++] = A[b[i] % 31];
  t[k] = 0;
}

static Sessao *pegar(void) {
  Sessao *s;
  pthread_mutex_lock(&trava);
  s = atual;
  if (s) atomic_fetch_add(&s->refs, 1);
  pthread_mutex_unlock(&trava);
  return s;
}
static void filaLimpar(Sessao *s) {
  while (s->fN) { av_packet_free(&s->fila[s->fIni].p); s->fIni = (s->fIni + 1) % FILA_MAX; s->fN--; }
  s->fIni = 0;
}
static void codecLiberar(Sessao *s) {
  avcodec_free_context(&s->dec); avcodec_free_context(&s->enc);
  swr_free(&s->swr); av_audio_fifo_free(s->fifo); s->fifo = NULL;
}
// ---- automatic match: the sides (as the webOS engine's MatchSide) ----------------
static void ladoLiberar(Lado *l) {
  int b;
  avcodec_free_context(&l->dec); swr_free(&l->swr); av_frame_free(&l->fr);
  for (b = 0; b < DTS_MATCH_BANDS; b++) av_freep(&l->env[b]);
  memset(l, 0, sizeof *l);
}
// Ready to take `cap` blocks of stream `si` (decoder opened on its first packet).
static int ladoPreparar(Lado *l, int si, int cap) {
  int b;
  ladoLiberar(l);
  l->si = si; l->cap = cap;
  for (b = 0; b < DTS_MATCH_BANDS; b++) if (!(l->env[b] = av_malloc(sizeof(float) * (size_t)cap))) return -1;
  return 0;
}
static int ladoCheio(const Lado *l) { return l->ev.max > 0 && l->ev.n >= l->ev.max; }
static void ladoSig(const Lado *l, DtsMatchSig *g) {
  int b;
  for (b = 0; b < DTS_MATCH_BANDS; b++) g->env[b] = l->ev.max ? l->ev.env[b] : NULL;
  g->n = l->ev.n; g->t0 = l->temT0 ? l->t0 : 0;
}
static void ladoQuadro(Lado *l, AVFrame *fr, AVRational tb) {
  float buf[8192];
  uint8_t *out = (uint8_t *)buf;
  int got;
  if (!l->swr) {
    AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
    if (swr_alloc_set_opts2(&l->swr, &mono, AV_SAMPLE_FMT_FLT, fr->sample_rate, &fr->ch_layout, fr->format,
                            fr->sample_rate, 0, NULL) < 0 || swr_init(l->swr) < 0) { swr_free(&l->swr); av_frame_unref(fr); return; }
    dts_match_env_init(&l->ev, fr->sample_rate, l->env, l->cap);
  }
  if (!l->temT0) {
    int64_t p = fr->best_effort_timestamp != AV_NOPTS_VALUE ? fr->best_effort_timestamp : fr->pts;
    if (p == AV_NOPTS_VALUE) { av_frame_unref(fr); return; }
    l->t0 = p * av_q2d(tb); l->temT0 = 1;
  }
  got = swr_convert(l->swr, &out, 8192, (const uint8_t **)fr->extended_data, fr->nb_samples);
  if (got > 0) dts_match_env_push(&l->ev, buf, got);
  av_frame_unref(fr);
}
// One packet of the side's stream. A jump in its time (a seek while it
// fills) starts the capture over from there. 1 full.
static int ladoPacote(Lado *l, const AVPacket *p, const AVStream *st) {
  double t;
  if (!l->cap || ladoCheio(l)) return 1;
  if (p->stream_index != l->si) return 0;
  if (!l->dec) {
    const AVCodec *c = avcodec_find_decoder(st->codecpar->codec_id);
    if (!c || !(l->dec = avcodec_alloc_context3(c)) || !(l->fr = av_frame_alloc())) { avcodec_free_context(&l->dec); return 0; }
    avcodec_parameters_to_context(l->dec, st->codecpar);
    l->dec->pkt_timebase = st->time_base;
    if (st->codecpar->codec_id == AV_CODEC_ID_DTS) av_opt_set_int(l->dec->priv_data, "core_only", 1, 0);
    if (avcodec_open2(l->dec, c, NULL) < 0) { avcodec_free_context(&l->dec); av_frame_free(&l->fr); return 0; }
  }
  t = p->pts != AV_NOPTS_VALUE ? p->pts * av_q2d(st->time_base) : l->ult;
  if (l->temT0 && (t < l->ult - 0.5 || t > l->ult + 3.0)) {
    int b;
    avcodec_flush_buffers(l->dec);
    l->ev.n = 0; l->ev.accn = 0; for (b = 0; b < DTS_MATCH_BANDS; b++) l->ev.acc[b] = 0;
    l->temT0 = 0;
  }
  l->ult = t;
  if (avcodec_send_packet(l->dec, p) < 0) return 0;
  while (avcodec_receive_frame(l->dec, l->fr) == 0) ladoQuadro(l, l->fr, st->time_base);
  return ladoCheio(l);
}

static void soltar(Sessao *s) {
  if (!s || atomic_fetch_sub(&s->refs, 1) != 1) return;
  filaLimpar(s); codecLiberar(s);
  av_packet_free(&s->pk); av_frame_free(&s->fr); av_frame_free(&s->ef);
  { int k; for (k = 0; k < TBA_FONTES; k++) {
      if (s->fo[k].eng) { dts_engine_cancel(s->fo[k].eng); dts_engine_destroy(s->fo[k].eng); }
      free(s->fo[k].url); free(s->fo[k].cab);
      { int j; for (j = 0; j < s->fo[k].nLeg; j++) free(s->fo[k].legT[j]); }
    } }
  free(s->aIni); free(s->video);
  ladoLiberar(&s->mArq); ladoLiberar(&s->mDon); ladoLiberar(&s->mArq2);
  pthread_mutex_destroy(&s->m); pthread_cond_destroy(&s->c); pthread_mutex_destroy(&s->gm); pthread_mutex_destroy(&s->mm);
  pthread_mutex_destroy(&s->mpm);
  free(s);
}
static void falhar(Sessao *s, const char *porque) {
  pthread_mutex_lock(&s->m);
  if (!s->pronto && !s->falhou) {
    s->falhou = 1;
    snprintf(s->motivo, sizeof s->motivo, "%s", porque);
    printf("[doador] session failed: %s\n", porque);
    fflush(stdout);
  }
  pthread_cond_broadcast(&s->c);
  pthread_mutex_unlock(&s->m);
}

// ---- preparation ---------------------------------------------------------------
static const char *nomeLingua(const char *l) {
  static const struct { const char *c, *n; } T[] = {
    {"eng","English"},{"en","English"},{"jpn","Japanese"},{"ja","Japanese"},{"por","Portuguese"},{"pt","Portuguese"},
    {"spa","Spanish"},{"es","Spanish"},{"fre","French"},{"fra","French"},{"fr","French"},{"ger","German"},{"deu","German"},
    {"de","German"},{"ita","Italian"},{"it","Italian"},{"swe","Swedish"},{"sv","Swedish"},{"kor","Korean"},{"ko","Korean"},
    {"chi","Chinese"},{"zho","Chinese"},{"zh","Chinese"},{"rus","Russian"},{"ru","Russian"},{"hin","Hindi"},{"ara","Arabic"},
    {"pol","Polish"},{"pl","Polish"},{"ukr","Ukrainian"},{"uk","Ukrainian"},{"cze","Czech"},{"ces","Czech"},{"cs","Czech"},
    {"dut","Dutch"},{"nld","Dutch"},{"nl","Dutch"},{"tur","Turkish"},{"tr","Turkish"},{"hun","Hungarian"},{"hu","Hungarian"}};
  unsigned i;
  for (i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcasecmp(l, T[i].c)) return T[i].n;
  return l[0] ? l : "Audio";
}
static int modoDe(const Faixa *f, int flacConv) {
  int ok = avcodec_find_decoder(f->id) != NULL;
  switch (f->id) {
    // Opus (subtitle-rewrite 9c62d57, owner 16/11): copied into MPEG-TS the TV
    // player never started (stuck before the first frame). As is in fMP4,
    // like FLAC: that plays on the S95C.
    case AV_CODEC_ID_OPUS: return M_MP4;
    case AV_CODEC_ID_AAC: case AV_CODEC_ID_AC3: case AV_CODEC_ID_EAC3:
    case AV_CODEC_ID_MP3: case AV_CODEC_ID_MP2: return M_TS;
    case AV_CODEC_ID_FLAC:
      if (!flacConv) return M_MP4;
      if (!ok) return M_FORA;
      return f->canais > 2 ? M_AC3 : M_AAC;
    case AV_CODEC_ID_DTS: case AV_CODEC_ID_TRUEHD: case AV_CODEC_ID_MLP: return ok ? M_AC3 : M_FORA;
    // PCM (owner's log 5, an anime with Japanese PCM: "not offered", gone
    // from the menu in donor mode). HLS has no normal PCM carrier; FLAC is
    // lossless (the TV decodes back the same samples) and plays as an fMP4
    // rendition on the S95C.
    case AV_CODEC_ID_PCM_S16LE: case AV_CODEC_ID_PCM_S24LE: case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_S16BE: case AV_CODEC_ID_PCM_S24BE: case AV_CODEC_ID_PCM_S32BE:
      if (!ok) return M_FORA;
      if (avcodec_find_encoder(AV_CODEC_ID_FLAC) && f->canais >= 1 && f->canais <= 8) return M_FLAC;
      return f->canais > 2 ? M_AC3 : M_AAC;
    default: if (!ok) return M_FORA; return f->canais > 2 ? M_AC3 : M_AAC;
  }
}

// Opens audio source k (0 the playing file, 1 the donor), lists its audio
// tracks into fx[] and reads its seek index now (not on the viewer's first
// seek). 0 = failed (the reason in fo[k].motivo; for the file itself the
// session fails, for the donor only its tracks are missing).
// SUBTITLE MATCH (owner 17/11): the embedded text subtitle tracks of a source
// as its seek index lists them. Matroska's Cues carry a point per subtitle
// block when the muxer indexed them (mkvmerge and FFmpeg do); FFmpeg puts them
// in the stream's index when it reads the Cues, which prepFonte just did. No
// byte is read for this. A track without points (Cues of the video only) is
// left out: the audio match then does the whole job.
static int ehTextoLeg(enum AVCodecID id) {
  return id == AV_CODEC_ID_SUBRIP || id == AV_CODEC_ID_TEXT || id == AV_CODEC_ID_ASS || id == AV_CODEC_ID_SSA ||
         id == AV_CODEC_ID_WEBVTT || id == AV_CODEC_ID_MOV_TEXT;
}
static int temSemCaixaL(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}
static int cmpDouble(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}
static void legendasIndice(Fonte *fo, const char *quem) {
  AVFormatContext *ic = fo->ic;
  int i, sem = 0, poucos = 0;
  fo->nLeg = 0;
  for (i = 0; ic && i < (int)ic->nb_streams && fo->nLeg < DOA_LEG_MAX; i++) {
    AVStream *st = ic->streams[i];
    int n = 0, j;
    double *t;
    DoaLegInfo *li;
    if (st->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE || !ehTextoLeg(st->codecpar->codec_id)) continue;
    n = avformat_index_get_entries_count(st);
    if (n <= 0) { sem++; continue; }
    if (n < 20) { poucos++; continue; }
    if (!(t = malloc(sizeof *t * (size_t)n))) continue;
    for (j = 0; j < n; j++) {
      const AVIndexEntry *e = avformat_index_get_entry(st, j);
      t[j] = e ? e->timestamp * av_q2d(st->time_base) : 0;
    }
    qsort(t, (size_t)n, sizeof *t, cmpDouble);
    li = &fo->leg[fo->nLeg];
    memset(li, 0, sizeof *li);
    { AVDictionaryEntry *l = av_dict_get(st->metadata, "language", NULL, 0), *ti = av_dict_get(st->metadata, "title", NULL, 0);
      snprintf(li->lingua, sizeof li->lingua, "%s", l && strcmp(l->value, "und") ? l->value : "");
      snprintf(li->nome, sizeof li->nome, "%s", ti ? ti->value : "");
      snprintf(li->codec, sizeof li->codec, "%s", avcodec_get_name(st->codecpar->codec_id));
      li->forcada = (st->disposition & AV_DISPOSITION_FORCED) || (ti && (temSemCaixaL(ti->value, "forced") || temSemCaixaL(ti->value, "sign"))); }
    li->n = n;
    li->falas = doa_falas(t, n, !strcmp(li->codec, "ass") || !strcmp(li->codec, "ssa"));
    fo->legT[fo->nLeg++] = t;
  }
  printf("[doador] %s: %d text subtitle track(s) indexed for the subtitle match", quem, fo->nLeg);
  for (i = 0; i < fo->nLeg; i++)
    printf("%s%s %s%s%s%.30s%s (%d points, %d lines)", i ? "; " : ": ", fo->leg[i].lingua[0] ? fo->leg[i].lingua : "und", fo->leg[i].codec,
           fo->leg[i].forcada ? " forced" : "", fo->leg[i].nome[0] ? " \"" : "", fo->leg[i].nome, fo->leg[i].nome[0] ? "\"" : "",
           fo->leg[i].n, fo->leg[i].falas);
  if (sem || poucos) printf(" | left out: %d without index points, %d with under 20", sem, poucos);
  printf("\n");
}
static int prepFonte(Sessao *s, int k) {
  Fonte *fo = &s->fo[k];
  double t0 = agoraMs();
  AVFormatContext *ic;
  int i, v = -1, nA = 0, n0 = s->nfx;
  const char *quem = k ? "donor" : "original file";
  { DtsEngine *e = dts_engine_create();
    pthread_mutex_lock(&s->m);   // the donor's watchdog reads it
    fo->eng = e;
    if (e && k && s->doadorVencido) dts_engine_cancel(e);
    pthread_mutex_unlock(&s->m); }
  if (!fo->eng) { snprintf(fo->motivo, sizeof fo->motivo, "out of memory"); fo->falhou = 1; return 0; }
  if (s->cancel) return 0;
  if (dts_engine_open_demux(fo->eng, fo->url, fo->cab) < 0) {
    snprintf(fo->motivo, sizeof fo->motivo, "%s: %s", quem, dts_engine_error(fo->eng)); fo->falhou = 1; return 0;
  }
  fo->conectada = 1;
  ic = fo->ic = dts_engine_avformat(fo->eng);
  for (i = 0; i < (int)ic->nb_streams; i++) {
    AVStream *st = ic->streams[i];
    AVCodecParameters *p = st->codecpar;
    if (p->codec_type == AVMEDIA_TYPE_VIDEO && v < 0 && !(st->disposition & AV_DISPOSITION_ATTACHED_PIC)) v = i;
    if (p->codec_type == AVMEDIA_TYPE_AUDIO && s->nfx < TBA_FAIXAS) {
      Faixa *f = &s->fx[s->nfx++];
      AVDictionaryEntry *l = av_dict_get(st->metadata, "language", NULL, 0);
      AVDictionaryEntry *t = av_dict_get(st->metadata, "title", NULL, 0);
      f->si = i; f->ord = nA; f->id = p->codec_id; f->fonte = k;
      f->chave = k ? DOA_DOADOR(nA) : nA;
      f->canais = p->ch_layout.nb_channels; f->taxa = p->sample_rate;
      snprintf(f->lingua, sizeof f->lingua, "%s", l && strcmp(l->value, "und") ? l->value : "");
      snprintf(f->nome, sizeof f->nome, "%s", t ? t->value : "");
      snprintf(f->codec, sizeof f->codec, "%s", avcodec_get_name(p->codec_id));
    }
    if (p->codec_type == AVMEDIA_TYPE_AUDIO) nA++;
  }
  for (i = 0; i < (int)ic->nb_streams; i++) ic->streams[i]->discard = AVDISCARD_ALL;
  fo->dur = ic->duration > 0 ? ic->duration / (double)AV_TIME_BASE : 0;
  fo->fim = (ic->start_time != AV_NOPTS_VALUE ? ic->start_time / (double)AV_TIME_BASE : 0) + fo->dur;   // as dts_remux's endS
  fo->v0 = 0;
  if (v >= 0 && ic->streams[v]->start_time != AV_NOPTS_VALUE)
    fo->v0 = ic->streams[v]->start_time * av_q2d(ic->streams[v]->time_base);
  else if (ic->start_time != AV_NOPTS_VALUE) fo->v0 = ic->start_time / (double)AV_TIME_BASE;
  // The seek index (Matroska reads its Cues on the first seek): read now, not
  // on the viewer's first seek. Seeks then go through the stream the index
  // covers (the video): with stream -1 FFmpeg picks the non-discarded audio,
  // which has no index entries, and falls back to reading the file forward
  // (log 14/11: seeks of 7-13 s; on the TV's FFmpeg 7.1 a seek back failed).
  // The video's index wins whenever it has one; else the audio with the most
  // entries. NEVER a subtitle track: its Cues can outnumber the video's, but
  // they start at its first line and skip the gaps, and a seek before them
  // left the reader at mid-file (log 14/11, TTGA Kaguya S03E02: segment 0 got
  // no audio and the player waited forever).
  { double t1 = agoraMs();
    int r = -1, mais = -1;
    fo->sBusca = v >= 0 ? v : 0;
    if (fo->dur > 10)
      r = av_seek_frame(ic, fo->sBusca, av_rescale_q((int64_t)(fo->dur * 0.5 * AV_TIME_BASE), AV_TIME_BASE_Q,
                                                     ic->streams[fo->sBusca]->time_base), AVSEEK_FLAG_BACKWARD);
    if (v < 0 || avformat_index_get_entries_count(ic->streams[v]) <= 0)
      for (i = 0; i < (int)ic->nb_streams; i++) {
        int n = avformat_index_get_entries_count(ic->streams[i]);
        if (ic->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
        if (n > mais) { mais = n; if (n > 0) fo->sBusca = i; }
      }
    fo->leu = 1;
    printf("[doador] %s: seek index %s in %.0f ms: seeks go through stream %d (%d entries)\n", quem, r >= 0 ? "read" : "NOT read",
           agoraMs() - t1, fo->sBusca, avformat_index_get_entries_count(ic->streams[fo->sBusca])); }
  legendasIndice(fo, quem);
  for (i = n0; i < s->nfx; i++) s->fx[i].modo = modoDe(&s->fx[i], s->flacConv);
  printf("[doador] %s open in %.0f ms (%s): %.1f s, video starts at %.3f s, %d audio track(s)%s\n", quem,
         agoraMs() - t0, ic->iformat ? ic->iformat->name : "?", fo->dur, fo->v0, s->nfx - n0,
         k && v < 0 ? "; NO VIDEO: its start is the container's" : "");
  for (i = n0; i < s->nfx; i++)
    printf("[doador]   %s audio %d: %s %dch %d Hz, %s%s%s -> %s\n", k ? "donor" : "file", s->fx[i].ord, s->fx[i].codec,
           s->fx[i].canais, s->fx[i].taxa, s->fx[i].lingua[0] ? s->fx[i].lingua : "und", s->fx[i].nome[0] ? ", " : "",
           s->fx[i].nome, MODO_NOME[s->fx[i].modo]);
  fflush(stdout);
  return 1;
}

// The track the session starts on (the DEFAULT rendition): the donor's in the
// asked language (else its first offered track) when the donor leads, else
// the file's asked track, else its first offered one.
static int casa(const char *a, const char *b) {
  return a[0] && b[0] && (casaLingua ? casaLingua(a, b) : !strcasecmp(a, b));
}
static void escolherInicial(Sessao *s, int ordPedido) {
  int i;
  s->audio = -1;
  if (s->doadorPadrao && s->nfo > 1 && !s->fo[1].falhou) {
    // 1. The track picked by hand for this title.
    for (i = 0; i < s->nfx && s->audio < 0 && s->doadorFaixa > 0; i++)
      if (s->fx[i].fonte == 1 && s->fx[i].modo != M_FORA && s->fx[i].si == s->doadorFaixa - 1) s->audio = i;
    // 2. The series memory: in its language, the most like it.
    if (s->audio < 0 && s->dicaLingua[0]) {
      int melhor = -1;
      for (i = 0; i < s->nfx; i++) {
        const Faixa *f = &s->fx[i];
        int p;
        if (f->fonte != 1 || f->modo == M_FORA || !casa(f->lingua, s->dicaLingua)) continue;
        p = 1 + (s->dicaTitulo[0] && !strcasecmp(f->nome, s->dicaTitulo) ? 8 : 0) +
            (s->dicaCodec[0] && !strcmp(f->codec, s->dicaCodec) ? 4 : 0) + (s->dicaCanais && f->canais == s->dicaCanais ? 2 : 0);
        if (p > melhor) { melhor = p; s->audio = i; }
      }
    }
    // 3. The languages in order, then its first offered track.
    { const char *l = s->doadorLingua;
      while (s->audio < 0 && *l) {
        char um[16]; size_t k = strcspn(l, ",");
        snprintf(um, sizeof um, "%.*s", (int)(k < sizeof um ? k : sizeof um - 1), l);
        for (i = 0; i < s->nfx && s->audio < 0 && um[0]; i++)
          if (s->fx[i].fonte == 1 && s->fx[i].modo != M_FORA && casa(s->fx[i].lingua, um)) s->audio = i;
        l += k; if (*l == ',') l++;
      } }
    for (i = 0; i < s->nfx && s->audio < 0; i++) if (s->fx[i].fonte == 1 && s->fx[i].modo != M_FORA) s->audio = i;
  }
  for (i = 0; i < s->nfx && s->audio < 0; i++)
    if (s->fx[i].fonte == 0 && s->fx[i].chave == ordPedido && s->fx[i].modo != M_FORA) s->audio = i;
  for (i = 0; i < s->nfx && s->audio < 0; i++) if (s->fx[i].fonte == 0 && s->fx[i].modo != M_FORA) s->audio = i;
}

// One audio connection (owner 15/11, the TorBox budget): the source the
// generator does not read gives its connection back; its demuxer and index
// stay, so taking it again is one Range request.
static void fonteSoltar(Sessao *s, int k, const char *porque) {
  Fonte *fo = &s->fo[k];
  double t0 = agoraMs();
  if (!fo->eng || !fo->conectada) return;
  dts_engine_release(fo->eng);
  fo->conectada = 0; fo->nSoltas++;
  printf("[doador] %s gave its connection back in %.0f ms (%s)\n", k ? "donor" : "original file", agoraMs() - t0, porque);
  fflush(stdout);
}

// The converter's playlist starts at the file's time 0, and every packet it
// writes is its source time + DTS_REMUX_TS_OFFSET_S: the audio goes there too.
static double tl0(const Sessao *s) { (void)s; return 0.0; }
static double ptsDesloc(const Sessao *s) { (void)s; return (double)DTS_REMUX_TS_OFFSET_S; }
// S2: the files' final hosts (behind the add-ons' redirects): Nuvio's
// connections there (the converter's included) and the TV player's (none: it
// only talks to 127.0.0.1).
static void vigiarHosts(Sessao *s) {
  char h[TBA_FONTES][128], u[TBA_FONTES][160];
  int k;
  for (k = 0; k < TBA_FONTES; k++) {
    h[k][0] = u[k][0] = 0;
    if (k < s->nfo && s->fo[k].eng) dts_engine_final_host(s->fo[k].eng, h[k], sizeof h[k]);
    if (h[k][0]) snprintf(u[k], sizeof u[k], "https://%.140s/", h[k]);
  }
  skVigiar(u[0], u[1], NULL, NULL);
}

// The master's STREAM-INF: what the converter's own master says (size, frame
// rate, HDR range), from the file's video stream.
static void videoInfo(Sessao *s) {
  AVFormatContext *ic = s->fo[0].ic;
  int i;
  int64_t tam = dts_engine_size(s->fo[0].eng);
  for (i = 0; ic && i < (int)ic->nb_streams; i++) {
    AVStream *st = ic->streams[i];
    AVCodecParameters *p = st->codecpar;
    AVRational r;
    if (p->codec_type != AVMEDIA_TYPE_VIDEO || (st->disposition & AV_DISPOSITION_ATTACHED_PIC)) continue;
    s->largura = p->width; s->altura = p->height;
    r = st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
    s->fps1000 = r.num > 0 && r.den > 0 ? lround(av_q2d(r) * 1000.0) : 0;
    if (p->codec_id == AV_CODEC_ID_HEVC) {
      int bits = p->bits_per_raw_sample > 0 ? p->bits_per_raw_sample : 8;
      int pr = p->profile > 0 ? p->profile : bits > 8 ? 2 : 1;
      snprintf(s->vCodecs, sizeof s->vCodecs, "hvc1.%d.4.L%d.B0", pr, p->level > 0 ? p->level : 150);
    } else if (p->codec_id == AV_CODEC_ID_H264)
      snprintf(s->vCodecs, sizeof s->vCodecs, "avc1.%02X00%02X", p->profile > 0 ? p->profile & 0xff : 100, p->level > 0 ? p->level : 41);
    snprintf(s->faixaVideo, sizeof s->faixaVideo, "%s", p->color_trc == AVCOL_TRC_SMPTE2084 ? "PQ" :
             p->color_trc == AVCOL_TRC_ARIB_STD_B67 ? "HLG" :
             av_packet_side_data_get(p->coded_side_data, p->nb_coded_side_data, AV_PKT_DATA_MASTERING_DISPLAY_METADATA) ? "PQ" : "");
    break;
  }
  s->banda = tam > 0 && s->fo[0].dur > 1 ? (long)(tam * 8.0 / s->fo[0].dur) : 20000000L;
  if (s->banda < 1000000L) s->banda = 1000000L;
}

// Watches the donor's opening: past DOADOR_PRAZO_MS its reader is cancelled
// (prepFonte then fails) and the session goes on with the file's tracks.
static void *vigiaDoador(void *u) {
  Sessao *s = u;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += DOADOR_PRAZO_MS / 1000;
  pthread_mutex_lock(&s->m);
  while (!s->doadorFeito && !s->cancel)
    if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) break;
  if (!s->doadorFeito && !s->cancel && s->fo[1].eng) {
    s->doadorVencido = 1;
    dts_engine_cancel(s->fo[1].eng);
  }
  pthread_mutex_unlock(&s->m);
  soltar(s);
  return NULL;
}

// The donor against the file (as the webOS engine, dts_engine.c): a debrid
// file not ready yet answers with a short clip that plays normally. Under two
// minutes for a picture of ten or more, or no audio and far shorter: the
// placeholder clip (the app retries it). Under half the file: not this title.
// 24/23.976, 25/24, 25/23.976 and back (webOS known_speed).
static int velocidadeConhecida(double k) {
  static const double conh[] = { 24000.0 / 23976.0, 25.0 / 24.0, 25000.0 / 23976.0 };
  int i;
  for (i = 0; i < 3; i++)
    if (fabs(k - conh[i]) < 0.15 * (conh[i] - 1.0) || fabs(k - 1.0 / conh[i]) < 0.15 * (conh[i] - 1.0)) return 1;
  return 0;
}
static double fpsDe(AVFormatContext *ic) {
  int i;
  for (i = 0; ic && i < (int)ic->nb_streams; i++) {
    AVStream *st = ic->streams[i];
    AVRational r;
    if (st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO || (st->disposition & AV_DISPOSITION_ATTACHED_PIC)) continue;
    r = st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
    return r.num > 0 && r.den > 0 ? av_q2d(r) : 0;
  }
  return 0;
}
// Shorter than this (s) in the menu's track list: the placeholder clip. The
// host test's fixtures are short, it lowers it.
#ifndef DOA_CLIP_S
#define DOA_CLIP_S 120
#endif
static int conferirDoador(Sessao *s) {
  Fonte *a = &s->fo[0], *b = &s->fo[1];
  int i, audio = 0;
  double fa = fpsDe(a->ic), fb = fpsDe(b->ic);
  for (i = 0; i < s->nfx; i++) audio += s->fx[i].fonte == 1;
  if ((!audio && b->dur > 0 && b->dur < a->dur / 2) || (b->dur > 0 && b->dur < 120 && a->dur > 600)) {
    snprintf(b->motivo, sizeof b->motivo, "%s", "External audio: this version is not ready on the debrid service yet (placeholder clip)");
    b->falhou = 1; return 0;
  }
  if (!audio) { snprintf(b->motivo, sizeof b->motivo, "%s", "External audio: the other version has no audio track"); b->falhou = 1; return 0; }
  if (b->dur > 0 && b->dur < a->dur / 2) {
    snprintf(b->motivo, sizeof b->motivo, "External audio: the other version lasts %.0f s, this one %.0f s (not cached?)", b->dur, a->dur);
    b->falhou = 1; return 0;
  }
  pthread_mutex_lock(&s->m);
  s->fpsRazao = fa > 0 && fb > 0 ? fb / fa : 0;
  // 24.000 against 23.976 is exactly 0.1 % (owner 17/11, Fight Club: a 4K
  // remux at 24.000 and a 1080p at 23.976, lengths 8342.2 / 8348.4 s, ~2.5 s
  // apart at 42 min). The old "> 0.001" left it out by a rounding hair.
  s->difVel = fa > 0 && fb > 0 && fabs(fa - fb) / fa >= 0.0009;
  // No speed asked and a known film/TV ratio the lengths confirm (a French
  // 25 fps version of a 23.976 picture): that speed from the start, so a
  // match has only the offset left to find (webOS, owner's log 14/11).
  s->difPpm = 0;
  // The lengths must also fit that speed better than none (a file whose
  // header rounds 23.976 to 24 keeps 1.0); the subtitle match confirms it.
  if (!s->ppmPedido && s->difVel && velocidadeConhecida(s->fpsRazao) && a->dur > 0 &&
      fabs(b->dur * s->fpsRazao / a->dur - 1.0) < 0.01 && fabs(b->dur * s->fpsRazao - a->dur) < fabs(b->dur - a->dur)) {
    s->fo[1].k = s->fpsRazao;
    s->difPpm = (int)lround((s->fpsRazao - 1.0) * 1e6);
    printf("[doador] donor at another speed (frame rates %.3f / %.3f, the lengths agree): played at ratio %.6f\n", fb, fa, s->fpsRazao);
  }
  // In the file's time: a sped-up version is not another cut.
  s->difCorte = (int)lround(b->dur * (s->difVel ? s->fpsRazao : 1.0) - a->dur);
  s->difOk = 1;
  pthread_mutex_unlock(&s->m);
  // Played at another speed: its tracks converted (a copy keeps its own pace).
  if (b->k != 1.0)
    for (i = 0; i < s->nfx; i++) {
      Faixa *f = &s->fx[i];
      if (f->fonte != 1 || f->modo == M_AC3 || f->modo == M_AAC || f->modo == M_FORA) continue;
      f->modo = !avcodec_find_decoder(f->id) ? M_FORA : f->canais > 2 ? M_AC3 : M_AAC;
      printf("[doador]   donor audio %d at ratio %.6f -> %s\n", f->ord, b->k, MODO_NOME[f->modo]);
    }
  printf("[doador] file fps %.3f %.1f s, donor fps %.3f %.1f s%s\n", fa, a->dur, fb, b->dur,
         s->difVel ? ": frame rates differ, its audio would drift" : "");
  return 1;
}

// A donor that starts after the file (its offset puts its first sample past
// the file's start): its copied tracks without a silence encoder here (Opus,
// MP3) play converted, so the segments before its start are not empty.
static int silencioPossivel(const Sessao *s, const Faixa *f);
static void silencioPreparar(Sessao *s) {
  double ini = s->fo[1].v0 * s->fo[1].k + deslocEm(s, s->fo[0].v0) - s->fo[0].v0;   // the donor's start in the file's time
  int i;
  if (ini < 0.12) return;
  for (i = 0; i < s->nfx; i++) {
    Faixa *f = &s->fx[i];
    if (f->fonte != 1 || f->modo == M_FORA || CONVERTE(f->modo) || silencioPossivel(s, f)) continue;
    f->modo = !avcodec_find_decoder(f->id) ? M_FORA : f->canais > 2 ? M_AC3 : M_AAC;
    printf("[doador]   donor audio %d (%s) starts %.3f s after the file and has no silence encoder: %s\n", f->ord, f->codec, ini, MODO_NOME[f->modo]);
  }
}
static void *preparar(void *u) {
  Sessao *s = u;
  double t0 = agoraMs();
  int ord = s->audio, i;
  if (!s->cancel && (prepFonte(s, 0) || (falhar(s, s->fo[0].motivo[0] ? s->fo[0].motivo : "original file did not open"), 0)) &&
      !s->cancel) {
    if (s->nfo > 1) {
      pthread_t tv;
      int ok;
      atomic_fetch_add(&s->refs, 1);
      if (pthread_create(&tv, NULL, vigiaDoador, s) == 0) pthread_detach(tv); else atomic_fetch_sub(&s->refs, 1);
      ok = prepFonte(s, 1);
      pthread_mutex_lock(&s->m);
      s->doadorFeito = 1;
      if (s->doadorVencido) { ok = 0; s->fo[1].falhou = 1; snprintf(s->fo[1].motivo, sizeof s->fo[1].motivo, "donor: no answer in %d s", DOADOR_PRAZO_MS / 1000); }
      pthread_cond_broadcast(&s->c);
      pthread_mutex_unlock(&s->m);
      if (ok) ok = conferirDoador(s);
      if (ok) {
        s->fo[1].desloc = s->fo[0].v0 - s->fo[1].v0 * s->fo[1].k + s->fo[1].desloc;   // desloc held the offset (ms / 1000) until here
        printf("[doador] donor \"%s\": %.1f s against the file's %.1f s (%+.1f s), starts %.3f vs %.3f s; its time %+.3f s = the file's\n",
               s->fo[1].nome, s->fo[1].dur, s->fo[0].dur, s->fo[1].dur - s->fo[0].dur, s->fo[1].v0, s->fo[0].v0, s->fo[1].desloc);
        silencioPreparar(s);
      } else {
        printf("[doador] donor not offered: %s (the file's own tracks go on)\n", s->fo[1].motivo);
        if (s->fo[1].eng) { dts_engine_cancel(s->fo[1].eng); dts_engine_destroy(s->fo[1].eng); s->fo[1].eng = NULL; }
        s->fo[1].ic = NULL; s->fo[1].conectada = 0;
      }
      fflush(stdout);
    }
    escolherInicial(s, ord);
    if (s->audio < 0) { falhar(s, "no audio track Nuvio can serve"); soltar(s); return NULL; }
    s->pk = av_packet_alloc(); s->fr = av_frame_alloc(); s->ef = av_frame_alloc();
    if (!s->pk || !s->fr || !s->ef) { falhar(s, "out of memory"); soltar(s); return NULL; }
    videoInfo(s);
    // The converter's playlist runs from 0 to the container's end (dts_remux).
    s->dur = s->fo[0].fim;
    s->naSeg = (int)floor(s->dur / AUD_SEG_S);   // a tail under 0.5 s joins the last segment
    if (s->dur - s->naSeg * AUD_SEG_S > 0.5) s->naSeg++;
    if (s->naSeg < 1) s->naSeg = 1;
    s->aIni = malloc(sizeof *s->aIni * (size_t)(s->naSeg + 1));
    if (s->aIni) { int k; for (k = 0; k < s->naSeg; k++) s->aIni[k] = k * AUD_SEG_S; s->aIni[s->naSeg] = s->dur; }
    // Both sources read their header and index: the one not starting gives
    // its connection back now (one audio connection).
    if (s->umaConexao)
      for (i = 0; i < s->nfo; i++) if (i != s->fx[s->audio].fonte) fonteSoltar(s, i, "not the starting track's source");
    vigiarHosts(s);
    // THE 4K TEST (owner 17/11): both 4K donors that never got an audio request
    // had 7 renditions; every 4K that played had 6 or fewer. The limit keeps
    // the starting track and the first others in the master's order.
    if (s->rendLimite > 0) {
      int k = 1, fora = 0;
      for (i = 0; i < s->nfx; i++) {
        Faixa *f = &s->fx[i];
        if (f->modo == M_FORA || i == s->audio) continue;
        if (k < s->rendLimite) k++; else { f->foraLista = 1; fora++; }
      }
      printf("[doador] rendition limit %d: %d left out of the master\n", s->rendLimite, fora);
    }
    pthread_mutex_lock(&s->m);
    if (!s->falhou && s->aIni) {
      s->pronto = 1;
      printf("[doador] ready in %.0f ms: %.1f s, %d audio segments; audio %s %d%s\n", agoraMs() - t0, s->dur, s->naSeg,
             s->fx[s->audio].fonte ? "donor" : "file", s->fx[s->audio].ord, s->nfo > 1 && !s->fo[1].falhou ? ", donor offered" : "");
    } else if (!s->falhou) { s->falhou = 1; snprintf(s->motivo, sizeof s->motivo, "out of memory"); }
    pthread_cond_broadcast(&s->c);
    pthread_mutex_unlock(&s->m);
    fflush(stdout);
  } else if (!s->falhou) falhar(s, s->cancel ? "closed" : "preparation failed");
  soltar(s);
  return NULL;
}

// ---- generator: packets of the active track, in source time --------------------
// ---- automatic match and following another cut (as the webOS engine) -------------
#define MATCH_ESPERA_S   120     // the two captures fill while playing; paused longer: given up
#define MATCH_MAX_MS     3600000 // beyond the sync's reach: never applied
#define SEGUIR_A_CADA_S  120.0
typedef struct { Sessao *s; int automatico, seguir, fino, siD; } MatchPedido;   // siD >= 0: the donor read on its own (another track than the one playing)

// The file's audio the donor is compared with: the track in the donor
// track's language (a dub keeps the music and effects), else its first one
// with a decoder.
static int matchSiArquivo(Sessao *s, const Faixa *d) {
  int i, primeiro = -1;
  for (i = 0; i < s->nfx; i++) {
    const Faixa *f = &s->fx[i];
    if (f->fonte != 0 || !avcodec_find_decoder(f->id)) continue;
    if (primeiro < 0) primeiro = f->si;
    if (d && casa(f->lingua, d->lingua)) return f->si;
  }
  return primeiro;
}
static void tapArquivo(void *op, const AVPacket *p, const AVStream *st) {
  Sessao *s = op;
  pthread_mutex_lock(&s->mm);
  ladoPacote(&s->mArq, p, st);
  if (s->mArq2.cap) ladoPacote(&s->mArq2, p, st);
  pthread_mutex_unlock(&s->mm);
}
static void matchIniciar(Sessao *s, int automatico, int seguir);
static void matchIniciarF(Sessao *s, int automatico, int seguir, int fino);
static void matchIniciarT(Sessao *s, int automatico, int seguir, int fino, int trecho);
// The donor's packets the generator reads (its playing track): the match's
// donor side, and the follow's next look when it is due.
static void doadorOuvir(Sessao *s, const AVPacket *p, const AVStream *st) {
  if (s->mArmD) {
    pthread_mutex_lock(&s->mm);
    if (s->mArmD) ladoPacote(&s->mDon, p, st);
    pthread_mutex_unlock(&s->mm);
  } else if (s->segAtivo && !s->legOk && !s->mapaN && !s->mEstado && p->pts != AV_NOPTS_VALUE &&
             p->pts * av_q2d(st->time_base) * s->fo[1].k + s->gDesloc >= s->segProx)
    matchIniciar(s, 1, 1);
}
static double baseDe(const Sessao *s, double k) { return s->fo[0].v0 - s->fo[1].v0 * k; }
static int msDe(const Sessao *s, double off, double k) { return (int)lround((off - baseDe(s, k)) * 1000.0); }
// THE EDIT MAP: the stretch at the file's time t (under mpm).
static int trechoEm(const Sessao *s, double t) {
  int i;
  for (i = 0; i + 1 < s->mapaN && t >= s->mapa[i].fim; i++) {}
  return i;
}
// THE DONOR'S OFFSET AT THE FILE'S TIME t (owner 18/11): its time * k + this
// = the file's. The edit map's stretch there, else the one offset. Everything
// that places the donor asks here (the segments, the match's centre, the
// follow, the opening check).
static double deslocEm(Sessao *s, double t) {
  double d;
  pthread_mutex_lock(&s->mpm);
  d = s->mapaN > 0 ? baseDe(s, s->fo[1].k) + s->mapa[trechoEm(s, t)].ms / 1000.0 : s->fo[1].desloc;
  pthread_mutex_unlock(&s->mpm);
  return d;
}
// Where the file has content the donor lacks (a stretch's offset greater than
// the one before it: the donor would play again what it played just before
// the edit), the file's time up to which the donor's packets are left out (the
// segment's silence fills it, owner 18/11). -1 nothing to leave out at t.
static double silencioAte(Sessao *s, double t, double seg) {
  double r = -1;
  pthread_mutex_lock(&s->mpm);
  if (s->mapaN > 1) {
    int i = trechoEm(s, t);
    if (i > 0 && s->mapa[i].ms > s->mapa[i - 1].ms) {
      // The first segment the stretch's offset is used for starts at E.
      double e = ceil(s->mapa[i].ini / seg - 1e-9) * seg, ate = e + (s->mapa[i].ms - s->mapa[i - 1].ms) / 1000.0;
      if (t < ate) r = ate;
    }
  }
  pthread_mutex_unlock(&s->mpm);
  return r;
}
static void deslocAplicar(Sessao *s, int ms) {
  pthread_mutex_lock(&s->gm);
  s->fo[1].desloc = baseDe(s, s->fo[1].k) + ms / 1000.0;
  s->doadorMs = ms;
  if (s->gFonte == 1) s->gOk = 0;
  pthread_mutex_unlock(&s->gm);
}
// The best places over several searches, 0.5 s apart, best first.
static void candAdd(int *n, double *off, double *sc, double o, double v) {
  int i, k;
  for (i = 0; i < *n; i++) if (fabs(off[i] - o) < 0.5) { if (v > sc[i]) { sc[i] = v; off[i] = o; } goto ordem; }
  if (*n < DTS_MATCH_CAND) { off[*n] = o; sc[*n] = v; (*n)++; }
  else if (v > sc[*n - 1]) { off[*n - 1] = o; sc[*n - 1] = v; }
ordem:
  for (i = 1; i < *n; i++) for (k = i; k > 0 && sc[k] > sc[k - 1]; k--) {
    double t = sc[k]; sc[k] = sc[k - 1]; sc[k - 1] = t; t = off[k]; off[k] = off[k - 1]; off[k - 1] = t;
  }
}
static int tentarConfirmar(const DtsMatchSig *sm, const DtsMatchSig *sd, double k, DtsMatchResult *r) {
  int i;
  for (i = 0; i < r->ncand; i++)
    if (r->cand_score[i] >= 0.10 && dts_match_verify(sm, sd, k, r->cand_off[i])) {
      printf("[doador] match: %.3f s (score %.2f) confirmed on both halves of the capture\n", r->cand_off[i], r->cand_score[i]);
      r->offset_s = r->cand_off[i]; r->score = r->cand_score[i]; r->confident = 1;
      return 1;
    }
  return 0;
}
static void respiro(void) { usleep(1000); }
// A wide search's capture: the donor read on its own from `de` for `seg` s.
static int capturar(Sessao *s, int si, double de, double seg, Lado *out) {
  DtsEngine *e = dts_engine_create();
  AVFormatContext *ic;
  AVPacket *pk;
  int i, v = -1, r = -1;
  if (!e) return -1;
  pthread_mutex_lock(&s->m);
  if (s->cancel) { pthread_mutex_unlock(&s->m); dts_engine_destroy(e); return -1; }
  s->mEng = e;
  pthread_mutex_unlock(&s->m);
  if (dts_engine_open_demux(e, s->fo[1].url, s->fo[1].cab) == 0 && (ic = dts_engine_avformat(e)) && si < (int)ic->nb_streams &&
      ladoPreparar(out, si, (int)(seg * DTS_MATCH_HZ) + DTS_MATCH_HZ) == 0) {
    for (i = 0; i < (int)ic->nb_streams; i++) {
      if (v < 0 && ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) v = i;
      if (i != si) ic->streams[i]->discard = AVDISCARD_ALL;
    }
    if (v < 0) v = si;
    av_seek_frame(ic, v, av_rescale_q((int64_t)(de * AV_TIME_BASE), AV_TIME_BASE_Q, ic->streams[v]->time_base), AVSEEK_FLAG_BACKWARD);
    if ((pk = av_packet_alloc())) {
      double fim = de + seg + 60.0;
      while (!s->cancel && av_read_frame(ic, pk) >= 0) {
        int cheio = 0;
        if (pk->stream_index == si) {
          cheio = ladoPacote(out, pk, ic->streams[si]);
          if (pk->pts != AV_NOPTS_VALUE && pk->pts * av_q2d(ic->streams[si]->time_base) > fim) cheio = 1;
        }
        av_packet_unref(pk);
        if (cheio) break;
      }
      av_packet_free(&pk);
    }
    if (out->ev.n >= 5 * DTS_MATCH_HZ && out->temT0) r = 0;
  }
  pthread_mutex_lock(&s->m); s->mEng = NULL; pthread_mutex_unlock(&s->m);
  dts_engine_destroy(e);
  return r;
}
static void matchFim(Sessao *s, int achou, const DoaDopasuj *r) {
  pthread_mutex_lock(&s->m);
  s->mRes = *r; s->mEstado = achou ? 2 : 3; s->mNovo = 1;
  pthread_mutex_unlock(&s->m);
}
static void *matchFio(void *u) {
  MatchPedido *pd = u;
  Sessao *s = pd->s;
  int seguir = pd->seguir, fino = pd->fino, siD = pd->siD, i, q, nk, achou = 0, como = 0, ncand = 0, trecho = s->mTrecho;
  double legD, tIni = -1e18, tFim = 1e18;
  Lado proprio;
  double t0 = agoraMs(), k0, kf, ks[2], center, off = 0, razao = 1, score = 0, cand[DTS_MATCH_CAND], candSc[DTS_MATCH_CAND];
  DtsMatchSig sm, sd, sm2;
  int dual = 0;
  DoaDopasuj res;
  free(pd);
  memset(&proprio, 0, sizeof proprio);
#ifdef SYS_gettid
  setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 10);
#endif
  dts_match_respiro = respiro;
  memset(&res, 0, sizeof res);
  // The captures fill while playing (~40 s): the file's track(s) from the
  // converter's reads, the donor's from the generator's. Another of the
  // donor's tracks than the playing one may not come (a donor read ahead):
  // 10 s after the file's are full, its own read below takes over.
  { double arqCheio = 0;
    for (;;) {
      int prontos, arq, tocaDoador;
      pthread_mutex_lock(&s->mm);
      arq = ladoCheio(&s->mArq) && (!s->mArq2.cap || ladoCheio(&s->mArq2));
      prontos = arq && ladoCheio(&s->mDon);
      pthread_mutex_unlock(&s->mm);
      if (arq && !arqCheio) arqCheio = agoraMs();
      if (arq && siD >= 0 && agoraMs() - arqCheio > 10000.0) prontos = 1;
      tocaDoador = s->audio >= 0 && s->fx[s->audio].fonte == 1;
      if (prontos || s->cancel || !tocaDoador || agoraMs() - t0 > MATCH_ESPERA_S * 1000.0) break;
      usleep(200000);
    } }
  dts_remux_set_tap(NULL, s, -1);
  pthread_mutex_lock(&s->mm); s->mArmD = 0; pthread_mutex_unlock(&s->mm);
  ladoSig(&s->mArq, &sm); ladoSig(&s->mDon, &sd);
  dual = s->mArq2.cap > 0;
  if (dual) ladoSig(&s->mArq2, &sm2);
  // SAME LANGUAGE ON BOTH SIDES (owner 17/11): the tracks of one file are in
  // sync with each other, so when the donor track playing has no language in
  // the file (an Italian dub), the donor's track in a language the file has is
  // read on its own over the same stretch, and its offset is the playing one's.
  if (siD >= 0 && !s->cancel && sm.n >= (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ && sd.n < (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ) {
    printf("[doador] match: the donor's reads gave %.1f s of its track %d: reading it on its own\n", sd.n / (double)DTS_MATCH_HZ, siD);
    const double tm = sm.t0 + sm.n / 2.0 / DTS_MATCH_HZ, pad = DTS_MATCH_WIN_S / 2.0 + 5.0;
    const double kk = s->fo[1].k > 0 ? s->fo[1].k : 1.0, raio = fino ? 1.0 : 10.0, c = deslocEm(s, tm);
    double lo = (tm - pad - (c + raio)) / kk - 2.0, hi = (tm + pad - (c - raio)) / kk + 2.0, mb;
    int64_t total = s->fo[1].eng ? dts_engine_size(s->fo[1].eng) : 0;
    if (lo < 0) lo = 0;
    mb = total > 0 && s->fo[1].dur > 0 ? total / s->fo[1].dur * (hi - lo) / 1048576.0 : 0;
    if (mb > 300) printf("[doador] match: the donor's same-language track would need about %.0f MB read, skipped\n", mb);
    else {
      printf("[doador] match: reading the donor's track %d (same language as the file's %d) on its own, %.0f-%.0f s (about %.0f MB)\n",
             siD, s->mArq.si, lo, hi, mb);
      fflush(stdout);
      if (capturar(s, siD, lo, hi - lo, &proprio) == 0) ladoSig(&proprio, &sd);
      else printf("[doador] match: the donor's own read did not fill\n");
    }
  }
  printf("[doador] match%s: captured %.1f s of the file at %.1f s and %.1f s of the donor at %.1f s in %.0f ms\n",
         seguir ? " (follow)" : fino ? " (audio refine)" : "",
         sm.n / (double)DTS_MATCH_HZ, sm.t0, sd.n / (double)DTS_MATCH_HZ, sd.t0, agoraMs() - t0);
  k0 = s->fo[1].k; kf = s->fpsRazao;
  ks[0] = k0; ks[1] = kf; nk = kf > 0 && fabs(kf - k0) > 0.0005 && !seguir && !fino ? 2 : 1;
  // Near the offset in force where the capture was (the edit map's stretch there).
  center = deslocEm(s, sm.t0 + sm.n / 2.0 / DTS_MATCH_HZ);
  // A stretch's own refine: the subtitles' share is that stretch's (its offset
  // from the map, before any shift), and the capture must lie inside it.
  legD = s->legDesloc;
  pthread_mutex_lock(&s->mpm);
  if (trecho >= 0 && trecho < s->mapaN) {
    legD = baseDe(s, k0) + s->mapa[trecho].legMs / 1000.0;
    tIni = s->mapa[trecho].ini; tFim = s->mapa[trecho].fim;
  }
  pthread_mutex_unlock(&s->mpm);
  if (!s->cancel && sm.n >= (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ && sd.n >= (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ) {
    // 1. Near the current offset, from what was played.
    for (q = 0; q < nk; q++) {
      DtsMatchResult r, r2;
      // The refine (the subtitle match placed it already): +-1 s only.
      int got = dts_match_search(&sm, &sd, ks[q], center, fino ? 1.0 : 10.0, &r), got2 = 0;
      if (got && !r.confident) tentarConfirmar(&sm, &sd, ks[q], &r);
      printf("[doador] match: ratio %.5f%s: %s offset %d ms (was %d) score %.2f margin %.2f\n", ks[q], dual ? " (the file's first track)" : "",
             got && r.confident ? "found" : "not trusted", got ? msDe(s, r.offset_s, ks[q]) : 0, s->doadorMs, got ? r.score : 0.0, got ? r.margin : 0.0);
      if (dual && sm2.n >= (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ) {
        // TWO FILE TRACKS (owner 18/11): the donor against the file's track in
        // its own language and the file's original-language track; each
        // candidate scored by both (their mean), so a dub track of the file
        // that is off is outvoted by the original.
        double o[2 * DTS_MATCH_CAND], md[2 * DTS_MATCH_CAND], a1[2 * DTS_MATCH_CAND], a2[2 * DTS_MATCH_CAND];
        int nm, b;
        got2 = dts_match_search(&sm2, &sd, ks[q], center, fino ? 1.0 : 10.0, &r2);
        if (got2 && !r2.confident) tentarConfirmar(&sm2, &sd, ks[q], &r2);
        printf("[doador] match: ratio %.5f (the file's original-language track): %s offset %d ms score %.2f margin %.2f\n", ks[q],
               got2 && r2.confident ? "found" : "not trusted", got2 ? msDe(s, r2.offset_s, ks[q]) : 0, got2 ? r2.score : 0.0, got2 ? r2.margin : 0.0);
        nm = doa_match_somar(got ? r.cand_off : NULL, got ? r.cand_score : NULL, got ? r.ncand : 0,
                             got2 ? r2.cand_off : NULL, got2 ? r2.cand_score : NULL, got2 ? r2.ncand : 0, 0.10, o, md, a1, a2, 2 * DTS_MATCH_CAND);
        for (b = 0; b < nm; b++)
          printf("[doador] match (two tracks): %+d ms: audio (file track %d) %.2f + audio (file track %d) %.2f, mean %.2f\n",
                 msDe(s, o[b], ks[q]), s->mArq.si, a1[b], s->mArq2.si, a2[b], md[b]);
        if (q == 0) for (b = 0; b < nm; b++) candAdd(&ncand, cand, candSc, o[b], md[b]);
        if (nm > 0 && a1[0] >= 0.10 && a2[0] >= 0.10 && a1[0] + a2[0] >= 0.40 && (nm < 2 || md[0] - md[1] >= 0.05)) {
          if (!achou || md[0] > score) {
            achou = 1; como = fino ? 7 : 1; off = o[0]; razao = ks[q]; score = md[0];
            printf("[doador] match (two tracks): %+d ms wins (mean %.2f%s)\n", msDe(s, o[0], ks[q]), md[0], nm > 1 ? "" : ", alone");
          }
        } else if (got2 && r2.confident && (!achou || r2.score > score)) {
          // No clear sum: the original-language track's own pick (the most correct one).
          achou = 1; como = fino ? 7 : 1; off = r2.offset_s; razao = ks[q]; score = r2.score;
          printf("[doador] match (two tracks): no clear sum, the original-language track's %+d ms\n", msDe(s, r2.offset_s, ks[q]));
        } else if (got && r.confident && (!achou || r.score > score)) {
          achou = 1; como = fino ? 7 : 1; off = r.offset_s; razao = ks[q]; score = r.score;
        }
        continue;
      }
      if (got && q == 0) for (i = 0; i < r.ncand; i++) candAdd(&ncand, cand, candSc, r.cand_off[i], r.cand_score[i]);
      if (got && r.confident && (!achou || r.score > score)) { achou = 1; como = fino ? 7 : 1; off = r.offset_s; razao = ks[q]; score = r.score; }
    }
    // 1b. SUBTITLES + AUDIO (owner 18/11, "add up the score"): the refine's
    // candidates get the subtitle match's share on top of their audio score.
    // Subtitles written by different people start lines a few hundred ms
    // apart (log 9: subtitles +2347, audio +1980, the owner confirmed 1980),
    // so within 0.5 s of their offset they back every candidate the same and
    // the audio decides; farther, their share fades. A candidate the audio
    // alone did not trust is taken when the sum is clear.
    if (fino && s->legConf > 0 && ncand > 0) {
      int mi = -1;
      double mv = -1, seg = -1;
      for (i = 0; i < ncand; i++) {
        double dd = fabs(cand[i] - legD), sub = s->legConf * 0.5 * (dd <= 0.5 ? 1.0 : exp(-pow((dd - 0.5) / 0.3, 2)));
        double v = candSc[i] + sub;
        printf("[doador] match (subtitles + audio): %+d ms: audio %.2f + subtitles %.2f = %.2f\n", msDe(s, cand[i], k0), candSc[i], sub, v);
        if (v > mv) { seg = mv; mv = v; mi = i; } else if (v > seg) seg = v;
      }
      if (mi >= 0 && candSc[mi] >= 0.30 && (seg < 0 || mv - seg >= 0.10)) {
        if (!achou || fabs(off - cand[mi]) > 0.05)
          printf("[doador] match (subtitles + audio): %+d ms wins (%.2f, next %.2f)\n", msDe(s, cand[mi], k0), mv, seg < 0 ? 0.0 : seg);
        achou = 1; como = 7; off = cand[mi]; razao = k0; score = candSc[mi];
      } else if (mi >= 0 && achou && fabs(off - cand[mi]) > 0.05) {
        // The sum is not clear and the audio's own pick is not its winner: no move.
        printf("[doador] match (subtitles + audio): no clear winner (%.2f vs %.2f): the subtitles' offset stays\n", mv, seg);
        achou = 0;
      }
    }
    // 2. Not near (a click, not the follow, not the refine): the donor read
    // on its own where the lengths suggest, +-60 s, +-5 min (webOS match_job_run).
    if (!achou && !seguir && !fino && !s->cancel) {
      const double tm = sm.t0 + sm.n / 2.0 / DTS_MATCH_HZ, base = baseDe(s, k0), pad = DTS_MATCH_WIN_S / 2.0 + 5.0;
      int64_t total = s->fo[1].eng ? dts_engine_size(s->fo[1].eng) : 0;
      const double bps = total > 0 && s->fo[1].dur > 0 ? total / s->fo[1].dur : 0;
      int corte = s->difCorte, longe = abs(corte) > 600, n = 0;
      struct { double c, r, capMb; int como; } st[5];
      if (abs(corte) > 10) {
        double h1 = -corte + base, h2 = -corte * (s->fo[0].dur > 0 ? tm / s->fo[0].dur : 0) + base;
        if (!longe) { st[n].c = h1; st[n].r = 10; st[n].capMb = 1e9; st[n++].como = 2; }
        if (longe || (fabs(h2 - h1) > 15 && fabs(h2 - center) > 15)) { st[n].c = h2; st[n].r = longe ? 60 : 10; st[n].capMb = 300; st[n++].como = 2; }
        if (longe) {
          double c = h2, r = 150;
          if (corte < 0 && c - r < base) c = base + r;
          if (corte > 0 && c + r > base) c = base - r;
          st[n].c = c; st[n].r = r; st[n].capMb = 300; st[n++].como = 4;
        }
      }
      st[n].c = center; st[n].r = 60; st[n].capMb = 300; st[n++].como = 3;
      if (!longe) { st[n].c = center; st[n].r = 300; st[n].capMb = 300; st[n++].como = 4; }
      for (i = 0; i < n && !s->cancel && !achou; i++) {
        double lo = 1e18, hi = -1e18, mb;
        Lado lado;
        if (st[i].como == 2 && fabs(st[i].c - center) < 10) continue;   // the near search covered it
        for (q = 0; q < nk; q++) {
          double a = (tm - pad - (st[i].c + st[i].r)) / ks[q], b = (tm + pad - (st[i].c - st[i].r)) / ks[q];
          if (a < lo) lo = a;
          if (b > hi) hi = b;
        }
        lo -= 2.0; hi += 2.0;
        if (lo < 0) lo = 0;
        if (hi <= lo + 5.0) continue;
        mb = bps * (hi - lo) / 1048576.0;
        if (bps > 0 && mb > st[i].capMb) {
          printf("[doador] match: +-%.0f s skipped, about %.0f MB of the donor to read\n", st[i].r, mb);
          res.pesado = 1; continue;
        }
        printf("[doador] match: %s, reading %.0f s of the donor (about %.0f MB)\n",
               st[i].como == 2 ? "trying the offset its length suggests" : st[i].r < 100 ? "searching +-60 s" : "searching +-5 min", hi - lo, mb);
        fflush(stdout);
        memset(&lado, 0, sizeof lado);
        if (capturar(s, siD >= 0 ? siD : s->mDon.si, lo, hi - lo, &lado) == 0) {
          DtsMatchSig sw;
          ladoSig(&lado, &sw);
          for (q = 0; q < nk; q++) {
            DtsMatchResult r;
            int got = dts_match_search(&sm, &sw, ks[q], st[i].c, st[i].r, &r);
            if (got && !r.confident) tentarConfirmar(&sm, &sw, ks[q], &r);
            if (got && r.confident && (!achou || r.score > score)) { achou = 1; como = st[i].como; off = r.offset_s; razao = ks[q]; score = r.score; }
            if (got && q == 0) for (int c = 0; c < r.ncand; c++) candAdd(&ncand, cand, candSc, r.cand_off[c], r.cand_score[c]);
            printf("[doador] match: ratio %.5f around %.1f s: offset %.3f s score %.2f margin %.2f%s\n", ks[q], st[i].c - base,
                   r.offset_s - base, r.score, r.margin, r.confident ? " (trusted)" : "");
          }
        }
        ladoLiberar(&lado);
      }
    }
  }
  for (i = 0; i < ncand && res.ncand < 3; i++) {
    int m = msDe(s, cand[i], k0);
    if (m > MATCH_MAX_MS || m < -MATCH_MAX_MS) continue;
    res.cand_ms[res.ncand] = m; res.cand_pct[res.ncand++] = (int)lround(candSc[i] * 100.0);
  }
  if (achou) {
    res.ms = msDe(s, off, razao); res.como = como;
    res.ppm = razao != 1.0 ? (int)lround((razao - 1.0) * 1e6) : 0;
    if (res.ms > MATCH_MAX_MS || res.ms < -MATCH_MAX_MS) {
      printf("[doador] match: %d ms is beyond the sync's reach, not applied\n", res.ms);
      achou = 0;
    }
  }
  if (!achou) res.dica_ms = abs(s->difCorte) > 10 && abs(s->difCorte) <= 600 ? -s->difCorte * 1000 : 0;
  if (fino) res.como = 7;
  // Owner's logs 5-7: every "not found" there was a capture cut short (another
  // pick, or the player left) before the search ran. Said as it is.
  if (!achou && s->cancel)
    printf("[doador] match%s: cancelled after %.0f ms (the session closed before the search; %.1f s of the file and %.1f s of the donor captured)\n",
           seguir ? " (follow)" : fino ? " (audio refine)" : "", agoraMs() - t0, sm.n / (double)DTS_MATCH_HZ, sd.n / (double)DTS_MATCH_HZ);
  else if (!achou && (sm.n < (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ || sd.n < (DTS_MATCH_WIN_S + 10) * DTS_MATCH_HZ))
    printf("[doador] match%s: not searched in %.0f ms (under %d s captured: paused, seeking or not playing the donor)\n",
           seguir ? " (follow)" : fino ? " (audio refine)" : "", agoraMs() - t0, DTS_MATCH_WIN_S + 10);
  else
  printf("[doador] match%s: %s in %.0f ms%s\n", seguir ? " (follow)" : fino ? " (audio refine)" : "", achou ? "found" : "not found", agoraMs() - t0,
         achou ? "" : res.pesado ? " (a wide search skipped for its download)" : "");
  if (fino && trecho >= 0) {
    // A STRETCH'S OWN REFINE (owner 18/11): only that stretch's offset in the
    // server changes, for the segments not made yet; no seek, no reopen, not
    // a match result for the app (event 5 updates the offset it shows when
    // the playhead is there). A capture outside the stretch (a seek, or it ran
    // over the edit) says nothing about it: dropped.
    double c0 = sm.t0, c1 = sm.t0 + sm.n / (double)DTS_MATCH_HZ;
    int antes = 0, depois = 0, dentro = c0 >= tIni - 0.5 && c1 <= tFim + 0.5;
    pthread_mutex_lock(&s->mpm);
    if (trecho < s->mapaN) {
      antes = s->mapa[trecho].ms;
      if (achou && dentro && fabs(razao - k0) < 1e-6) {
        depois = msDe(s, off, razao);
        s->mapa[trecho].ms = depois; s->mapa[trecho].refinado = 1;
        if (s->mapaPos == trecho && depois != s->doadorMs) { s->doadorMs = depois; s->segEvento = 5; s->segMs = depois; }
      } else s->mapa[trecho].refinado = -1;
    }
    pthread_mutex_unlock(&s->mpm);
    if (achou && dentro && fabs(razao - k0) < 1e-6)
      printf("[doador] stretch %d refined: %+d -> %+d ms (audio score %.2f, captured %.0f-%.0f s)\n", trecho + 1, antes, depois, score, c0, c1);
    else if (!dentro)
      printf("[doador] stretch %d (%.0f-%.0f s): the refine's capture (%.0f-%.0f s) is not inside it: dropped, %+d ms kept\n", trecho + 1,
             tIni, tFim, c0, c1, antes);
    else printf("[doador] stretch %d: refine not found, %+d ms kept\n", trecho + 1, antes);
    pthread_mutex_lock(&s->m); s->mEstado = 0; s->mTrecho = -1; pthread_mutex_unlock(&s->m);
  } else if (seguir) {
    // The follow corrects the offset by itself; the app hears of it.
    pthread_mutex_lock(&s->m);
    if (achou && fabs(razao - k0) < 1e-6) {
      s->segFalhas = 0; s->segPerdido = 0;
      if (abs(res.ms - s->doadorMs) > 250) {
        pthread_mutex_unlock(&s->m);
        deslocAplicar(s, res.ms);
        pthread_mutex_lock(&s->m);
        s->segEvento = 4; s->segMs = res.ms;
        printf("[doador] follow: a shift of %+d ms (a gap longer in one version): offset %+d ms now\n", res.ms - s->doadorMs, res.ms);
      }
    } else if (++s->segFalhas >= 2 && !s->segPerdido) {
      s->segPerdido = 1; s->segEvento = 3; s->segMs = s->doadorMs;
      printf("[doador] follow: lost it (two looks without a trusted place): use Match automatically\n");
    }
    s->segProx = sm.t0 + sm.n / (double)DTS_MATCH_HZ + SEGUIR_A_CADA_S;
    s->mEstado = 0;
    pthread_mutex_unlock(&s->m);
  } else {
    // The first refine with an edit map: its doa_desloc shifts every stretch
    // and marks the playhead's refined.
    if (fino && achou) {
      pthread_mutex_lock(&s->mpm);
      s->fino1Pend = 1;
      if (s->mapaN > 0) s->mapa[trechoEm(s, sm.t0 + sm.n / 2.0 / DTS_MATCH_HZ)].refinado = 1;
      pthread_mutex_unlock(&s->mpm);
    }
    matchFim(s, achou, &res);
    // Another cut and "Follow through the whole film": from this find on, only
    // for a donor the subtitles could not place (owner 18/11).
    if (achou && s->seguir && abs(s->difCorte) > 10 && !s->legOk && !fino) {
      pthread_mutex_lock(&s->m);
      s->segAtivo = 1; s->segFalhas = 0; s->segPerdido = 0;
      s->segProx = sm.t0 + sm.n / (double)DTS_MATCH_HZ + SEGUIR_A_CADA_S;
      pthread_mutex_unlock(&s->m);
      printf("[doador] follow: on (the lengths differ by %d s), next look in %.0f s\n", s->difCorte, SEGUIR_A_CADA_S);
    }
  }
  fflush(stdout);
  ladoLiberar(&proprio);
  soltar(s);
  return NULL;
}
static void matchIniciar(Sessao *s, int automatico, int seguir) { matchIniciarT(s, automatico, seguir, 0, -1); }
static void matchIniciarF(Sessao *s, int automatico, int seguir, int fino) { matchIniciarT(s, automatico, seguir, fino, -1); }
// A failed start: the app hears "not found" (not for the follow, nor for a
// stretch's own refine, which only marks the stretch tried).
static void matchNaoComecou(Sessao *s, int seguir, int trecho) {
  if (trecho >= 0) {
    pthread_mutex_lock(&s->mpm);
    if (trecho < s->mapaN && !s->mapa[trecho].refinado) s->mapa[trecho].refinado = -1;
    pthread_mutex_unlock(&s->mpm);
  } else if (!seguir) { DoaDopasuj z; memset(&z, 0, sizeof z); matchFim(s, 0, &z); }
}
static void matchIniciarT(Sessao *s, int automatico, int seguir, int fino, int trecho) {
  MatchPedido *pd;
  pthread_t t;
  const Faixa *d;
  int siA, siD = -1;
  pthread_mutex_lock(&s->m);
  // A stretch's own refine waits while a result is not taken by the app yet.
  if (trecho >= 0 && s->mNovo) { pthread_mutex_unlock(&s->m); return; }
  if (s->mEstado == 1 || s->cancel || !s->pronto || s->audio < 0 || s->fx[s->audio].fonte != 1) {
    int ocupado = s->mEstado == 1;
    pthread_mutex_unlock(&s->m);
    if (!ocupado) matchNaoComecou(s, seguir, trecho);
    return;
  }
  s->mEstado = 1; s->mAuto = automatico; s->mFino = fino; s->mTrecho = trecho;
  pthread_mutex_unlock(&s->m);
  d = &s->fx[s->audio];
  // WHICH AUDIO (owner 18/11, doa_match_escolher): the original language on
  // both sides first; a donor without it against two of the file's tracks.
  { DoaMatchFaixa mf[TBA_FAIXAS];
    char orig[16], pl[16], pc[24];
    int i, pcn, leve, ok;
    for (i = 0; i < s->nfx; i++) {
      const Faixa *f = &s->fx[i];
      memset(&mf[i], 0, sizeof mf[i]);
      mf[i].fonte = f->fonte; mf[i].si = f->si; mf[i].canais = f->canais;
      mf[i].ok = avcodec_find_decoder(f->id) != NULL && (f->fonte == 0 || !s->fo[1].falhou);
      snprintf(mf[i].lingua, sizeof mf[i].lingua, "%s", f->lingua);
      snprintf(mf[i].codec, sizeof mf[i].codec, "%s", f->codec);
    }
    pthread_mutex_lock(&trava);
    snprintf(orig, sizeof orig, "%s", mcOrig); snprintf(pl, sizeof pl, "%s", mcLing); snprintf(pc, sizeof pc, "%s", mcCodec);
    pcn = mcCanais; leve = mcLeve;
    pthread_mutex_unlock(&trava);
    ok = doa_match_escolher(mf, s->nfx, s->audio, orig, pl, pc, pcn, leve || seguir, casaLingua, &s->mEsc);
    siA = ok ? s->mEsc.siA : matchSiArquivo(s, d);
    siD = ok ? s->mEsc.siD : -1;
    if (!ok) s->mEsc.siA2 = -1;
    if (!seguir)
      printf("[doador] match tracks: %s%s\n", ok ? s->mEsc.porque : "the file's first track",
             orig[0] ? "" : " (original language unknown)"); }
  pd = malloc(sizeof *pd);
  pthread_mutex_lock(&s->mm);
  ladoLiberar(&s->mArq2);
  if (siA < 0 || !pd || ladoPreparar(&s->mArq, siA, DTS_MATCH_MAX) < 0 || ladoPreparar(&s->mDon, siD >= 0 ? siD : d->si, DTS_MATCH_MAX) < 0 ||
      (s->mEsc.siA2 >= 0 && ladoPreparar(&s->mArq2, s->mEsc.siA2, DTS_MATCH_MAX) < 0)) {
    ladoLiberar(&s->mArq2);
    pthread_mutex_unlock(&s->mm);
    free(pd);
    pthread_mutex_lock(&s->m); s->mEstado = 0; s->mTrecho = -1; pthread_mutex_unlock(&s->m);
    matchNaoComecou(s, seguir, trecho);
    return;
  }
  // The donor side always from the generator's reads: the playing track, or
  // (another of the donor's tracks) the same downloaded bytes demuxed for it
  // too (produzir); no read of its own unless that one does not fill.
  s->mArmD = 1;
  pthread_mutex_unlock(&s->mm);
  dts_remux_set_tap2(tapArquivo, s, siA, s->mArq2.cap ? s->mArq2.si : -1);
  pd->s = s; pd->automatico = automatico; pd->seguir = seguir; pd->fino = fino; pd->siD = siD;
  atomic_fetch_add(&s->refs, 1);
  if (pthread_create(&t, NULL, matchFio, pd) != 0) {
    atomic_fetch_sub(&s->refs, 1); free(pd);
    dts_remux_set_tap(NULL, s, -1);
    pthread_mutex_lock(&s->mm); s->mArmD = 0; pthread_mutex_unlock(&s->mm);
    pthread_mutex_lock(&s->m); s->mEstado = 0; s->mTrecho = -1; pthread_mutex_unlock(&s->m);
    matchNaoComecou(s, seguir, trecho);
    return;
  }
  pthread_detach(t);
  if (trecho >= 0) printf("[doador] stretch %d: its own audio refine (+-1 s around its offset)\n", trecho + 1);
  printf("[doador] match%s%s: capturing ~%d s of the file's audio %d and the donor's %d while playing%s\n",
         seguir ? " (follow)" : fino ? " (audio refine, +-1 s)" : "", automatico ? " (by itself)" : "", DTS_MATCH_SECONDS, siA,
         siD >= 0 ? siD : d->si, siD >= 0 ? " (from the donor's reads, not the playing track)" : "");
  if (s->mArq2.cap) printf("[doador] match: also the file's audio %d, scored with the first\n", s->mArq2.si);
  fflush(stdout);
}

static int filaPor(Sessao *s, AVPacket *p, double t) {
  int k;
  if (s->fN == FILA_MAX) { av_packet_free(&p); return 0; }
  k = (s->fIni + s->fN) % FILA_MAX;
  s->fila[k].p = p; s->fila[k].t = t; s->fN++;
  return 1;
}
// FLAC from PCM: the source's channels, 24 bits when the PCM has more than 16.
static int flacBits(const AVCodecParameters *cp) {
  int b = cp->bits_per_raw_sample > 0 ? cp->bits_per_raw_sample : cp->bits_per_coded_sample;
  return b > 16 ? 24 : 16;
}
static int flacPreparar(AVCodecContext *e, const AVCodecParameters *cp, int canais) {
  av_channel_layout_uninit(&e->ch_layout);
  if (cp->ch_layout.nb_channels == canais && cp->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC) av_channel_layout_copy(&e->ch_layout, &cp->ch_layout);
  else av_channel_layout_default(&e->ch_layout, canais);
  e->sample_rate = 48000; e->time_base = (AVRational){1, 48000};
  if (flacBits(cp) == 24) { e->sample_fmt = AV_SAMPLE_FMT_S32; e->bits_per_raw_sample = 24; }
  else e->sample_fmt = AV_SAMPLE_FMT_S16;
  return 0;
}
static int abrirConversor(Sessao *s, const Faixa *f, char *err, size_t en) {
  AVStream *st = s->fo[f->fonte].ic->streams[f->si];
  const AVCodec *dc = avcodec_find_decoder(f->id);
  const AVCodec *ec = avcodec_find_encoder(f->modo == M_FLAC ? AV_CODEC_ID_FLAC : f->modo == M_AC3 ? AV_CODEC_ID_AC3 : AV_CODEC_ID_AAC);
  AVChannelLayout alvo;
  int r, ch = f->modo == M_FLAC ? f->canais : f->modo == M_AC3 && f->canais > 2 ? 6 : 2;
  codecLiberar(s);
  if (!dc || !ec) { snprintf(err, en, "decoder/encoder not in this build (%s)", f->codec); return -1; }
  s->dec = avcodec_alloc_context3(dc);
  if (!s->dec) { snprintf(err, en, "out of memory"); return -1; }
  avcodec_parameters_to_context(s->dec, st->codecpar);
  s->dec->pkt_timebase = st->time_base;
  av_channel_layout_default(&alvo, ch);
  if (f->id == AV_CODEC_ID_DTS) av_opt_set_int(s->dec->priv_data, "core_only", 1, 0);
  else if ((f->id == AV_CODEC_ID_TRUEHD || f->id == AV_CODEC_ID_MLP) && (f->canais == 0 || f->canais > ch))
    av_opt_set_chlayout(s->dec->priv_data, "downmix", &alvo, 0);
  if ((r = avcodec_open2(s->dec, dc, NULL)) < 0) { snprintf(err, en, "audio decoder open (%d)", r); return -1; }
  s->enc = avcodec_alloc_context3(ec);
  if (!s->enc) { snprintf(err, en, "out of memory"); return -1; }
  if (f->modo == M_FLAC) {
    flacPreparar(s->enc, st->codecpar, ch);
    s->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    s->enc->compression_level = 2;   // light on the TV's CPU; still lossless
  } else {
    av_channel_layout_copy(&s->enc->ch_layout, &alvo);
    s->enc->sample_rate = 48000; s->enc->time_base = (AVRational){1, 48000};
    s->enc->sample_fmt = AV_SAMPLE_FMT_FLTP;
    s->enc->bit_rate = f->modo == M_AC3 ? (ch == 6 ? 640000 : 192000) : 192000;
    if (f->modo == M_AAC) { s->enc->profile = AV_PROFILE_AAC_LOW; s->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; }
  }
  if ((r = avcodec_open2(s->enc, ec, NULL)) < 0) { snprintf(err, en, "audio encoder open (%d)", r); return -1; }
  s->fifo = av_audio_fifo_alloc(s->enc->sample_fmt, ch, 8192);
  if (!s->fifo) { snprintf(err, en, "out of memory"); return -1; }
  s->base48 = AV_NOPTS_VALUE; s->amostras = 0;
  av_channel_layout_uninit(&alvo);
  return 0;
}
static int encodar(Sessao *s, AVFrame *quadro) {
  int r = avcodec_send_frame(s->enc, quadro);
  if (r < 0 && r != AVERROR_EOF) return -1;
  for (;;) {
    AVPacket *o = av_packet_alloc();
    if (!o) return -1;
    r = avcodec_receive_packet(s->enc, o);
    if (r < 0) { av_packet_free(&o); return r == AVERROR(EAGAIN) || r == AVERROR_EOF ? 0 : -1; }
    filaPor(s, o, o->pts / 48000.0 + s->gDesloc);   // the playing file's time
  }
}
static int drenarFifo(Sessao *s, int tudo) {
  int fs = s->enc->frame_size > 0 ? s->enc->frame_size : 1536;
  while (av_audio_fifo_size(s->fifo) >= fs || (tudo && av_audio_fifo_size(s->fifo) > 0)) {
    int n = av_audio_fifo_size(s->fifo) < fs ? av_audio_fifo_size(s->fifo) : fs;
    av_frame_unref(s->ef);
    s->ef->nb_samples = fs; s->ef->format = s->enc->sample_fmt; s->ef->sample_rate = 48000;
    av_channel_layout_copy(&s->ef->ch_layout, &s->enc->ch_layout);
    if (av_frame_get_buffer(s->ef, 0) < 0) return -1;
    if (n < fs) av_samples_set_silence(s->ef->data, 0, fs, s->ef->ch_layout.nb_channels, s->enc->sample_fmt);
    av_audio_fifo_read(s->fifo, (void **)s->ef->data, n);
    s->ef->pts = s->base48 + s->amostras;
    s->amostras += fs;
    if (encodar(s, s->ef) < 0) return -1;
  }
  return 0;
}
// A packet's length in its stream's time base: the container's, else from
// the codec (FFmpeg's frame size). 0 = unknown. (subtitle-rewrite cde2770;
// the Opus TOC reading there was replaced by the opus parser, 7b60ede, which
// this FFmpeg has.)
static int64_t pacoteDur(const AVStream *st, const AVPacket *p) {
  int a;
  if (p->duration > 0) return p->duration;
  a = av_get_audio_frame_duration2(st->codecpar, p->size);
  return a > 0 && st->codecpar->sample_rate > 0 ? av_rescale_q(a, (AVRational){1, st->codecpar->sample_rate}, st->time_base) : 0;
}
// One more step: >0 progressed, 0 end of file, -1 error.
static int produzir(Sessao *s, const Faixa *f, char *err, size_t en) {
  Fonte *fo = &s->fo[f->fonte];
  AVStream *st = fo->ic->streams[f->si];
  // The match reads another of the donor's tracks (doa_match_escolher siD)
  // from the bytes this reader downloads anyway: that stream is demuxed too
  // while the match captures.
  if (f->fonte == 1) {
    int x = s->mArmD && s->mDon.cap && s->mDon.si != f->si && s->mDon.si < (int)fo->ic->nb_streams ? s->mDon.si : -1;
    if (x != s->dExtra) {
      if (s->dExtra >= 0 && s->dExtra < (int)fo->ic->nb_streams && s->dExtra != f->si) fo->ic->streams[s->dExtra]->discard = AVDISCARD_ALL;
      if (x >= 0) fo->ic->streams[x]->discard = AVDISCARD_DEFAULT;
      s->dExtra = x;
    }
  }
  for (;;) {
    int r;
    if (s->cancel) { snprintf(err, en, "closed"); return -1; }
    if (s->eof) return 0;
    r = av_read_frame(fo->ic, s->pk);
    fo->leu = 1;
    if (r >= 0) s->lidos++;
    if (r < 0) {
      if (r != AVERROR_EOF) { snprintf(err, en, "reading the %s (%d)", f->fonte ? "donor" : "original file", r); return -1; }
      s->eof = 1;
      if (s->enc) { drenarFifo(s, 1); encodar(s, NULL); }
      return 0;
    }
    if (s->pk->stream_index != f->si) {
      if (f->fonte == 1 && s->dExtra >= 0 && s->pk->stream_index == s->dExtra && s->mArmD) {
        pthread_mutex_lock(&s->mm);
        if (s->mArmD && s->mDon.cap) ladoPacote(&s->mDon, s->pk, fo->ic->streams[s->dExtra]);
        pthread_mutex_unlock(&s->mm);
      }
      av_packet_unref(s->pk);
      continue;
    }
    if (s->pk->pts == AV_NOPTS_VALUE) s->pk->pts = s->pk->dts;
    // Matroska lacing without BlockDuration/DefaultDuration (subtitle-rewrite
    // cde2770, owner 16/11): only the block's first frame has a timestamp, the
    // others come with none. They used to be dropped (14 of 100 Opus packets
    // per 2 s: no sound). Each one follows the one before it.
    {
      int64_t d = pacoteDur(st, s->pk);
      if (s->pk->pts == AV_NOPTS_VALUE && s->proxPts != AV_NOPTS_VALUE) {
        s->pk->pts = s->pk->dts = s->proxPts;
        s->semPts++;
        if (s->semPtsLog < 3) {
          s->semPtsLog++;
          printf("[doador] %saudio %d: a packet without a timestamp (laced frame) follows the one before (%.3f s, %.1f ms)\n",
                 f->fonte ? "donor " : "", f->ord, s->proxPts * av_q2d(st->time_base), d * av_q2d(st->time_base) * 1000.0);
        }
      }
      if (s->pk->pts == AV_NOPTS_VALUE) { av_packet_unref(s->pk); continue; }
      if (s->pk->duration <= 0 && d > 0) s->pk->duration = d;
      s->proxPts = d > 0 ? s->pk->pts + d : AV_NOPTS_VALUE;
    }
    s->guardados++;
    if (f->fonte == 1) doadorOuvir(s, s->pk, st);
    if (!s->dec) {
      AVPacket *c = av_packet_clone(s->pk);
      double t = s->pk->pts * av_q2d(st->time_base) + s->gDesloc;   // the playing file's time
      av_packet_unref(s->pk);
      if (!c) { snprintf(err, en, "out of memory"); return -1; }
      filaPor(s, c, t);
      return 1;
    }
    r = avcodec_send_packet(s->dec, s->pk);
    av_packet_unref(s->pk);
    if (r < 0 && r != AVERROR(EAGAIN)) continue;   // a broken frame: skip it
    for (;;) {
      r = avcodec_receive_frame(s->dec, s->fr);
      if (r < 0) break;
      if (!s->swr) {
        // Another speed: the samples are made at 48 kHz * k and taken as 48
        // kHz (the audio lasts k times longer, its pitch follows).
        if (swr_alloc_set_opts2(&s->swr, &s->enc->ch_layout, s->enc->sample_fmt, (int)lround(48000.0 * fo->k), &s->fr->ch_layout,
                                s->fr->format, s->fr->sample_rate, 0, NULL) < 0 || swr_init(s->swr) < 0) {
          snprintf(err, en, "resampler"); av_frame_unref(s->fr); return -1;
        }
      }
      if (s->base48 == AV_NOPTS_VALUE) {
        int64_t p = s->fr->pts != AV_NOPTS_VALUE ? s->fr->pts : s->fr->best_effort_timestamp;
        s->base48 = p != AV_NOPTS_VALUE ? llround(p * av_q2d(st->time_base) * fo->k * 48000.0) : 0;
      }
      { int max = swr_get_out_samples(s->swr, s->fr->nb_samples);
        uint8_t **buf = NULL;
        if (max > 0 && av_samples_alloc_array_and_samples(&buf, NULL, s->enc->ch_layout.nb_channels, max,
                                                          s->enc->sample_fmt, 0) >= 0) {
          int k = swr_convert(s->swr, buf, max, (const uint8_t **)s->fr->extended_data, s->fr->nb_samples);
          if (k > 0) av_audio_fifo_write(s->fifo, (void **)buf, k);
          av_freep(&buf[0]); av_freep(&buf);
        } }
      av_frame_unref(s->fr);
      if (drenarFifo(s, 0) < 0) { snprintf(err, en, "audio encoder"); return -1; }
    }
    if (s->fN) return 1;
  }
}
// Stop what is being made: the player hung up (`so` closed) or asked for a
// segment that is not this one or the next (a seek). 1 hung up, 2 superseded.
static int abandonar(Sessao *s, int so, long chave) {
  if (s->cancel) return 1;
  if (so >= 0) {
    struct pollfd p = { so, POLLIN, 0 };
    if (poll(&p, 1, 0) > 0) {
      char c;
      if (p.revents & (POLLHUP | POLLERR)) return 1;
      if (recv(so, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0) return 1;
    }
  }
  if (atomic_load(&s->espera) > 0) {
    long w = atomic_load(&s->espChave);
    if (w != chave && w != chave + 1) return 2;
  }
  return 0;
}
// Positions the generator for segment `seg` of track `fi` on the list `ini`
// (the 2 s audio segments), whose
// segments are about `len` s. *saltou = 1 when packets before the segment
// must be dropped. A jump of more than one segment seeks: these files run at
// 40 Mbit/s, reading 15 s through took 6.4 s (log 14/11).
static int posicionar(Sessao *s, int fi, int seg, const double *ini, double len, int *saltou, const char **como,
                      char *err, size_t en) {
  const Faixa *f = &s->fx[fi];
  Fonte *fo = &s->fo[f->fonte];
  // The donor's offset for this segment: the edit map's at its start (owner
  // 18/11: an edit is a donor seek between two segments; the TV player never
  // seeks for it).
  double dl = f->fonte ? deslocEm(s, ini[seg] + tl0(s)) : fo->desloc;
  // Segment `seg` in the playing file's time, then in this source's own.
  double alvo = (ini[seg] + tl0(s) - dl) / fo->k;
  int mesmo = s->gOk && s->gFaixa == fi && s->gModo == f->modo && s->gIni == ini && s->gSeg >= 0 && s->gDesloc == dl;
  *saltou = 0; *como = "continued";
  if (!mesmo && f->fonte && s->gOk && s->gFaixa == fi && s->gModo == f->modo && s->gIni == ini && s->gSeg == seg && s->gDesloc != dl)
    printf("[doador] seg %d crosses the edit at %.1f s: offset %+.0f -> %+.0f ms (a donor seek, the player plays on)\n", seg, ini[seg],
           (s->gDesloc - baseDe(s, fo->k)) * 1000.0, (dl - baseDe(s, fo->k)) * 1000.0);
  if (mesmo && s->gSeg == seg) return 0;
  if (mesmo && seg > s->gSeg && ini[seg] - ini[s->gSeg] <= len + 0.5) { *saltou = 1; *como = "read ahead"; return 0; }
  *saltou = 1; *como = fo->conectada ? "seeked" : "seeked, connection taken again";
  // The other source goes quiet: its connection back (one audio connection).
  if (s->umaConexao && s->gFonte >= 0 && s->gFonte != f->fonte) fonteSoltar(s, s->gFonte, "the player moved to the other source's track");
  if (!fo->conectada) { fo->conectada = 1; fo->nVoltas++; }
  filaLimpar(s);
  s->eof = 0;
  s->proxPts = AV_NOPTS_VALUE;   // a seek: the next timestamp comes from the file again
  { int i; for (i = 0; i < (int)fo->ic->nb_streams; i++) fo->ic->streams[i]->discard = i == f->si ? AVDISCARD_DEFAULT : AVDISCARD_ALL; }
  if (f->fonte == 1) s->dExtra = -1;   // produzir opens the match's extra stream again
  if (CONVERTE(f->modo)) { if (abrirConversor(s, f, err, en) < 0) { s->gOk = 0; return -1; } }
  else codecLiberar(s);
  dts_engine_interrupt(fo->eng, 0);
  s->gFonte = f->fonte; s->gDesloc = dl;
  if (alvo < fo->v0) alvo = fo->v0;   // a donor that starts later than the file: from its own start
  if (seg > 0 || fo->leu || f->fonte) {
    // Straight to the segment: the index gives the keyframe at or before it.
    AVStream *b = fo->ic->streams[fo->sBusca];
    int64_t ts = av_rescale_q((int64_t)(alvo * AV_TIME_BASE), AV_TIME_BASE_Q, b->time_base);
    // Before the index's first entry Matroska does not move at all (it
    // returns 0 and the reader stays where it was): ask for the first entry.
    if (avformat_index_get_entries_count(b) > 0 && av_index_search_timestamp(b, ts, AVSEEK_FLAG_BACKWARD) < 0) {
      const AVIndexEntry *e0 = avformat_index_get_entry(b, 0);
      if (e0 && e0->timestamp > ts) {
        printf("[doador] seek to %.3f s is before the index (first entry %.3f s): going to the first entry\n", alvo,
               e0->timestamp * av_q2d(b->time_base));
        ts = e0->timestamp;
      }
    }
    if (av_seek_frame(fo->ic, fo->sBusca, ts, AVSEEK_FLAG_BACKWARD) < 0 &&
        av_seek_frame(fo->ic, -1, (int64_t)(alvo * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD) < 0) {
      snprintf(err, en, "seek in the %s to %.1f s", f->fonte ? "donor" : "original file", alvo); s->gOk = 0; return -1;
    }
  }
  s->gOk = 1; s->gFaixa = fi; s->gModo = f->modo; s->gSeg = seg; s->gIni = ini;
  return 0;
}

// ---- muxing into memory ----------------------------------------------------------
typedef struct { AVFormatContext *oc; int64_t cab; } Mux;
static void muxFechar(Mux *m) {
  if (!m->oc) return;
  if (m->oc->pb) { uint8_t *b = NULL; avio_close_dyn_buf(m->oc->pb, &b); av_free(b); m->oc->pb = NULL; }
  avformat_free_context(m->oc);
  m->oc = NULL;
}
// streams: 1 = audio only, 2 = video (from `vpar`) + audio.
static int muxAbrir(Mux *m, const char *fmt, const AVCodecParameters *vpar, AVRational vtb,
                    const AVCodecParameters *apar, AVRational atb, const char *lingua) {
  AVDictionary *o = NULL;
  AVStream *st;
  int r;
  memset(m, 0, sizeof *m);
  if (avformat_alloc_output_context2(&m->oc, NULL, fmt, NULL) < 0 || !m->oc) return -1;
  if (vpar) {
    if (!(st = avformat_new_stream(m->oc, NULL))) return -1;
    avcodec_parameters_copy(st->codecpar, vpar); st->codecpar->codec_tag = 0; st->time_base = vtb;
  }
  if (!(st = avformat_new_stream(m->oc, NULL))) return -1;
  avcodec_parameters_copy(st->codecpar, apar); st->codecpar->codec_tag = 0; st->time_base = atb;
  if (lingua && lingua[0]) av_dict_set(&st->metadata, "language", lingua, 0);
  if (avio_open_dyn_buf(&m->oc->pb) < 0) return -1;
  m->oc->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;
  if (!strcmp(fmt, "mp4")) {
    m->oc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;   // FLAC in MP4
    av_dict_set(&o, "movflags", "frag_custom+empty_moov+default_base_moof+frag_discont", 0);
    av_dict_set(&o, "use_editlist", "0", 0);
  }
  r = avformat_write_header(m->oc, &o);
  av_dict_free(&o);
  if (r < 0) return -1;
  avio_flush(m->oc->pb);
  m->cab = avio_tell(m->oc->pb);
  return 0;
}
// Finishes and hands the bytes (header dropped with `semCabecalho`). malloc'd.
static unsigned char *muxFim(Mux *m, int mp4, int semCabecalho, size_t *n) {
  uint8_t *b = NULL;
  int k, pula;
  unsigned char *o;
  if (mp4) av_write_frame(m->oc, NULL); else av_write_trailer(m->oc);
  k = avio_close_dyn_buf(m->oc->pb, &b);
  m->oc->pb = NULL;
  pula = semCabecalho && m->cab > 0 && m->cab <= k ? (int)m->cab : 0;
  o = malloc((size_t)(k - pula) + 1);
  if (o && k - pula > 0) memcpy(o, b + pula, (size_t)(k - pula));
  *n = o ? (size_t)(k - pula) : 0;
  av_free(b);
  muxFechar(m);
  return o;
}
// Parameters of what goes out for track f (copied, or the encoder's).
static int parametrosSaida(Sessao *s, const Faixa *f, AVCodecParameters *p, AVRational *tb) {
  if (s->enc && CONVERTE(f->modo)) {
    avcodec_parameters_from_context(p, s->enc);
    *tb = (AVRational){1, 48000};
  } else {
    avcodec_parameters_copy(p, s->fo[f->fonte].ic->streams[f->si]->codecpar);
    *tb = s->fo[f->fonte].ic->streams[f->si]->time_base;
  }
  return 0;
}
// Writes one audio packet (its source's time base `tb`) shifted onto the
// video's timeline: its source's time + desloc = the playing file's time, + ptsDesloc.
static int escreverAudio(Sessao *s, Mux *m, int idx, AVPacket *p, AVRational tb, int interl) {
  AVStream *st = m->oc->streams[idx];
  double desloc = ptsDesloc(s) + s->gDesloc;
  int64_t d = llround(desloc / av_q2d(st->time_base));
  if (p->duration <= 0 && s->enc) p->duration = s->enc->frame_size > 0 ? s->enc->frame_size : 1536;
  av_packet_rescale_ts(p, tb, st->time_base);
  if (p->pts != AV_NOPTS_VALUE) p->pts += d;
  if (p->dts != AV_NOPTS_VALUE) p->dts += d; else p->dts = p->pts;
  p->stream_index = idx;
  p->pos = -1;
  return interl ? av_interleaved_write_frame(m->oc, p) : av_write_frame(m->oc, p);
}

typedef struct { unsigned char *b; size_t n; char tipo[48]; int status; } Resp;

// SILENCE WHERE A SOURCE HAS NO AUDIO (owner 18/11, log 2: Fight Club with a
// 25 fps Turkish donor; the subtitle match put it +2953 ms later, the reopen's
// segment 0 (0-2 s) fell before the donor's first sample and went out with 0
// packets, and the TV player never prepared, twice). A stretch of a segment
// before a source's first packet, or after its end, is encoded silence in what
// the rendition carries (the converter's codec, or the copied one's: AAC LC,
// AC3, E-AC3, FLAC). Opus/MP3 have no encoder here: a donor that starts after
// the file plays those converted instead (silencioPreparar).
#define SIL_MIN_S 0.12   // under this a gap is the frames' own rounding
static int silencioCodec(const AVCodecParameters *cp, enum AVCodecID *id) {
  *id = cp->codec_id;
  if (cp->codec_id == AV_CODEC_ID_AAC && cp->profile > 0 && cp->profile != AV_PROFILE_AAC_LOW) return 0;   // HE-AAC: LC frames would not fit its header
  return (cp->codec_id == AV_CODEC_ID_AAC || cp->codec_id == AV_CODEC_ID_AC3 || cp->codec_id == AV_CODEC_ID_EAC3 ||
          cp->codec_id == AV_CODEC_ID_FLAC) && avcodec_find_encoder(cp->codec_id) != NULL;
}
static int silencioPossivel(const Sessao *s, const Faixa *f) {
  enum AVCodecID id;
  if (CONVERTE(f->modo)) return 1;
  return silencioCodec(s->fo[f->fonte].ic->streams[f->si]->codecpar, &id);
}
// Writes silence over [de, ate) of the playing file's time into segment mux m.
// Frames that would run past `ate` are left out (a gap under one frame).
static int silencio(Sessao *s, const Faixa *f, Mux *m, double de, double ate, int *nPk, const char *onde) {
  const AVCodecParameters *cp = s->fo[f->fonte].ic->streams[f->si]->codecpar;
  enum AVCodecID id = AV_CODEC_ID_NONE;
  const AVCodec *ec;
  AVCodecContext *e = NULL;
  AVFrame *fr = NULL;
  AVPacket *pk = NULL;
  int taxa, fs, n = 0, r, pad;
  int64_t pos, fim;
  if (ate - de < SIL_MIN_S) return 0;
  if (CONVERTE(f->modo) && s->enc) id = s->enc->codec_id;
  else if (!silencioCodec(cp, &id)) {
    printf("[doador] %saudio %d: %.3f-%.3f s has no audio and %s has no silence encoder here: left empty\n", f->fonte ? "donor " : "",
           f->ord, de - tl0(s), ate - tl0(s), avcodec_get_name(cp->codec_id));
    return 0;
  }
  if (!(ec = avcodec_find_encoder(id)) || !(e = avcodec_alloc_context3(ec))) return 0;
  if (CONVERTE(f->modo) && s->enc) {
    av_channel_layout_copy(&e->ch_layout, &s->enc->ch_layout);
    e->sample_rate = s->enc->sample_rate; e->sample_fmt = s->enc->sample_fmt; e->bit_rate = s->enc->bit_rate;
    e->profile = s->enc->profile; e->bits_per_raw_sample = s->enc->bits_per_raw_sample;
    if (id == AV_CODEC_ID_FLAC) e->compression_level = 2;
  } else if (id == AV_CODEC_ID_FLAC) {
    flacPreparar(e, cp, cp->ch_layout.nb_channels > 0 ? cp->ch_layout.nb_channels : 2);
    e->sample_rate = cp->sample_rate > 0 ? cp->sample_rate : 48000;
    e->compression_level = 2;
  } else {
    if (cp->ch_layout.nb_channels > 0 && cp->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC) av_channel_layout_copy(&e->ch_layout, &cp->ch_layout);
    else av_channel_layout_default(&e->ch_layout, cp->ch_layout.nb_channels > 0 ? cp->ch_layout.nb_channels : 2);
    e->sample_rate = cp->sample_rate > 0 ? cp->sample_rate : 48000;
    e->sample_fmt = AV_SAMPLE_FMT_FLTP;   // aac, ac3 and eac3 all take it
    e->bit_rate = cp->bit_rate > 0 ? cp->bit_rate : id == AV_CODEC_ID_AAC ? 128000 : e->ch_layout.nb_channels > 2 ? 448000 : 192000;
    if (id == AV_CODEC_ID_AAC) e->profile = AV_PROFILE_AAC_LOW;
  }
  taxa = e->sample_rate;
  e->time_base = (AVRational){1, taxa};
  if ((r = avcodec_open2(e, ec, NULL)) < 0) {
    printf("[doador] %saudio %d: silence encoder %s did not open (%d): %.3f-%.3f s left empty\n", f->fonte ? "donor " : "", f->ord,
           avcodec_get_name(id), r, de - tl0(s), ate - tl0(s));
    avcodec_free_context(&e);
    return 0;
  }
  fs = e->frame_size > 0 ? e->frame_size : 1024;
  pad = e->initial_padding > 0 ? e->initial_padding : 0;   // the encoder's delay: its packets land where asked
  fr = av_frame_alloc(); pk = av_packet_alloc();
  if (!fr || !pk) goto fim;
  fr->nb_samples = fs; fr->format = e->sample_fmt; fr->sample_rate = taxa;
  av_channel_layout_copy(&fr->ch_layout, &e->ch_layout);
  if (av_frame_get_buffer(fr, 0) < 0) goto fim;
  av_samples_set_silence(fr->data, 0, fs, fr->ch_layout.nb_channels, e->sample_fmt);
  // Its time on the source's line (packet time + gDesloc = the file's time, as escreverAudio writes).
  pos = (int64_t)ceil((de - s->gDesloc) * taxa);
  fim = (int64_t)floor((ate - s->gDesloc) * taxa);
  for (;;) {
    int fimEnc = pos + fs > fim;
    if (!fimEnc) { fr->pts = pos + pad; pos += fs; }
    if (avcodec_send_frame(e, fimEnc ? NULL : fr) < 0) break;
    while (avcodec_receive_packet(e, pk) >= 0) {
      if (pk->pts != AV_NOPTS_VALUE && pk->pts + fs <= fim && escreverAudio(s, m, 0, pk, e->time_base, 0) >= 0) { n++; (*nPk)++; }
      av_packet_unref(pk);
    }
    if (fimEnc) break;
  }
  printf("[doador] %saudio %d: %s %.3f-%.3f s had no audio: %d %s frame(s) of silence\n", f->fonte ? "donor " : "", f->ord, onde,
         de - tl0(s), ate - tl0(s), n, avcodec_get_name(id));
fim:
  av_packet_free(&pk); av_frame_free(&fr); avcodec_free_context(&e);
  return n;
}



// Audio of track fi, segment `seg` (AUD_SEG_S) or -1 = the fMP4 init.
// -2 = stopped (the player hung up or moved on), nothing to send.
static int gerarAudio(Sessao *s, int fi, int seg, Resp *r, char *err, size_t en, int so, long chave) {
  const Faixa *f = &s->fx[fi];
  AVCodecParameters *par = avcodec_parameters_alloc();
  AVRational tb, srcTb;
  Mux m;
  int mp4 = EM_MP4(f->modo), saltou = 0, nPk = 0, rr = 0, ab = 0, nSil = 0;
  // FLAC/Opus (owner 17/11: a donor's FLAC had no sound): packets, their
  // lengths and sizes, and what of the segment they cover, first 3 segments.
  int diag = 0, dSem = 0, tMin = 1 << 30, tMax = 0;
  double dMin = 1e9, dMax = 0, ult = -1;
  const char *como = "";
  double t0 = agoraMs(), tAviso = agoraMs(), fs, fe, pri = -1, tl, ultFim = -1, segFim, silAte;
  if (!par) { snprintf(err, en, "out of memory"); return -1; }
  if (seg < 0) {   // init: the header alone (the converter state is not touched)
    AVStream *as = s->fo[f->fonte].ic->streams[f->si];
    AVRational itb = as->time_base;
    if (f->modo == M_FLAC) {
      // PCM made FLAC: the header a FLAC encoder with the same settings writes
      // (its STREAMINFO), from a throwaway encoder, so the session's own
      // converter is not touched.
      const AVCodec *ec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
      AVCodecContext *e = ec ? avcodec_alloc_context3(ec) : NULL;
      int ok = 0;
      if (e) {
        flacPreparar(e, as->codecpar, f->canais);
        e->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; e->compression_level = 2;
        ok = avcodec_open2(e, ec, NULL) >= 0 && avcodec_parameters_from_context(par, e) >= 0;
      }
      avcodec_free_context(&e);
      if (!ok) { avcodec_parameters_free(&par); snprintf(err, en, "fMP4 init (FLAC encoder)"); return -1; }
      itb = (AVRational){1, 48000};
    } else avcodec_parameters_copy(par, as->codecpar);
    if (muxAbrir(&m, "mp4", NULL, (AVRational){0, 1}, par, itb, f->lingua) < 0) {
      muxFechar(&m); avcodec_parameters_free(&par); snprintf(err, en, "fMP4 init (muxer)"); return -1;
    }
    { uint8_t *b = NULL; int k = avio_close_dyn_buf(m.oc->pb, &b); m.oc->pb = NULL;
      r->b = malloc((size_t)k + 1); if (r->b) { memcpy(r->b, b, (size_t)k); r->n = (size_t)k; } av_free(b); }
    muxFechar(&m);
    avcodec_parameters_free(&par);
    snprintf(r->tipo, sizeof r->tipo, "audio/mp4");
    return r->b ? 0 : -1;
  }
  if (posicionar(s, fi, seg, s->aIni, AUD_SEG_S, &saltou, &como, err, en) < 0) { avcodec_parameters_free(&par); return -1; }
  parametrosSaida(s, f, par, &tb);
  srcTb = s->dec ? (AVRational){1, 48000} : s->fo[f->fonte].ic->streams[f->si]->time_base;
  if (muxAbrir(&m, mp4 ? "mp4" : "mpegts", NULL, (AVRational){0, 1}, par, tb, f->lingua) < 0) {
    muxFechar(&m); avcodec_parameters_free(&par); snprintf(err, en, "muxer"); return -1;
  }
  avcodec_parameters_free(&par);
  fs = s->aIni[seg] + tl0(s);   // the playing file's time (a donor's packets are mapped onto it)
  fe = seg + 1 < s->naSeg ? s->aIni[seg + 1] + tl0(s) : 1e18;
  // After an edit where the file has content the donor lacks: the donor's
  // packets up to `silAte` would repeat what played before it; left out, the
  // segment's silence takes their place (a copied codec without a silence
  // encoder keeps them: a repeat is better than a hole).
  silAte = f->fonte == 1 && silencioPossivel(s, f) ? silencioAte(s, fs, AUD_SEG_S) : -1;
  if (silAte > fs)
    printf("[doador] seg %d: the file has %.2f s the donor lacks before %.1f s: silence there, no repeat\n", seg, silAte - fs, silAte);
  diag = (f->id == AV_CODEC_ID_FLAC || f->id == AV_CODEC_ID_OPUS) && !s->dec && s->diagLog < 3;
  s->lidos = s->guardados = 0;
  for (;;) {
    Item it;
    if ((ab = abandonar(s, so, chave)) != 0) { rr = -2; break; }
    // Still making it after 3 s (owner 17/11: a 4K file's donor never served
    // one segment and the log could not say why): what the reader did so far.
    if (agoraMs() - tAviso >= 3000.0) {
      const Fonte *fa = &s->fo[f->fonte];
      tAviso = agoraMs();
      printf("[doador] still making %saudio %d seg %d after %.0f ms (%s): %ld packets read, %ld of the track, %d without a timestamp, "
             "%d queued, %d out, reader %s at %.1f s\n", f->fonte ? "donor " : "", f->ord, seg, tAviso - t0, como, s->lidos, s->guardados,
             s->semPts, s->fN, nPk, fa->conectada ? "connected" : "released", s->fN ? s->fila[(s->fIni + s->fN - 1) % FILA_MAX].t : -1.0);
      fflush(stdout);
    }
    if (!s->fN) { rr = produzir(s, f, err, en); if (rr < 0) break; if (rr == 0 && !s->fN) { rr = 0; break; } continue; }
    it = s->fila[s->fIni];
    if (it.t >= fe) break;
    s->fIni = (s->fIni + 1) % FILA_MAX; s->fN--;
    if (saltou && it.t < fs - 0.001) { av_packet_free(&it.p); continue; }
    if (it.t < silAte) { av_packet_free(&it.p); continue; }
    if (pri < 0) {
      pri = it.t;
      // The source has no audio yet at the segment's start: silence up to its first packet.
      if (it.t - fs >= SIL_MIN_S) { int a = nPk; silencio(s, f, &m, fs, it.t, &nPk, "the segment's start"); nSil += nPk - a; }
    }
    if (it.p->duration > 0) ultFim = it.t + it.p->duration * av_q2d(srcTb);
    else ultFim = it.t;
    if (diag) {
      double d = it.p->duration > 0 ? it.p->duration * av_q2d(srcTb) * 1000.0 : -1;
      if (d < 0) dSem++; else { if (d < dMin) dMin = d; if (d > dMax) dMax = d; }
      if (it.p->size < tMin) tMin = it.p->size;
      if (it.p->size > tMax) tMax = it.p->size;
      if (it.t > ult) ult = it.t;
    }
    if (escreverAudio(s, &m, 0, it.p, srcTb, 0) < 0) { av_packet_free(&it.p); snprintf(err, en, "mux write"); rr = -1; break; }
    av_packet_free(&it.p);
    nPk++;
  }
  // No packet at all (before the source's start, after its end, or a gap
  // longer than the segment), or the source ended inside it: silence to its end.
  segFim = s->aIni[seg + 1] + tl0(s);
  if (rr >= 0 && rr != -2) {
    int a = nPk;
    if (!nPk) silencio(s, f, &m, fs, segFim, &nPk, "all of");
    else if (s->eof && ultFim > 0 && segFim - ultFim >= SIL_MIN_S) silencio(s, f, &m, ultFim, segFim, &nPk, "after its end,");
    nSil += nPk - a;
  }
  tl = agoraMs();
  if (rr == -2) {
    // The packets already taken belong to this segment: the next request
    // seeks (or continues past them only when it is the next segment).
    muxFechar(&m);
    if (ab == 2) s->gOk = 0;
    s->nLarg++;
    snprintf(err, en, "%s after %.0f ms", ab == 1 ? "the player hung up" : "the player asked for another segment", tl - t0);
    return -2;
  }
  if (rr < 0) {
    muxFechar(&m); s->gOk = 0;
    // The donor stopped answering: three of its segments failed in a row.
    if (f->fonte == 1 && ++s->dFalhas >= 3 && !s->dSemResposta) {
      s->dSemResposta = 1;
      printf("[doador] the donor does not respond (3 segments failed in a row)\n");
    }
    return -1;
  }
  if (f->fonte == 1) {
    s->dFalhas = 0;
    // Slower than the picture: a 2 s segment over 10 s, twice.
    if (tl - t0 > 10000.0 && ++s->dLento == 2) printf("[doador] the donor downloads too slowly (two segments over 10 s)\n");
  }
  r->b = muxFim(&m, mp4, mp4, &r->n);
  if (!r->b) { snprintf(err, en, "out of memory"); return -1; }
  snprintf(r->tipo, sizeof r->tipo, mp4 ? "audio/mp4" : "video/mp2t");
  s->gSeg = seg + 1;
  s->nSeg++; s->bytes += (long long)r->n; s->msLer += tl - t0;
  if (!strcmp(como, "seeked, connection taken again")) s->fo[f->fonte].msVolta = tl - t0;
  printf("[doador] %saudio %d seg %d (%.1f-%.1f s): %d packets%s, %zu KB, %.0f ms, %s; first audio at %.3f s\n",
         f->fonte ? "donor " : "", f->ord, seg, s->aIni[seg], s->aIni[seg + 1], nPk, nSil ? " (silence included)" : "", r->n / 1024,
         tl - t0, como, pri >= 0 ? pri - tl0(s) : -1.0);
  if (diag && nPk > 0) {
    const AVCodecParameters *cp = s->fo[f->fonte].ic->streams[f->si]->codecpar;
    s->diagLog++;
    printf("[doador] %s %saudio %d seg %d: %d packets, durations %s%.1f-%.1f ms%s, sizes %d-%d bytes, covers %.3f-%.3f s "
           "(+%d without a timestamp so far) | extradata %d bytes, %d ch %d Hz, fMP4 %s\n",
           f->id == AV_CODEC_ID_FLAC ? "FLAC" : "Opus", f->fonte ? "donor " : "", f->ord, seg, nPk, dMax > 0 ? "" : "none, ",
           dMax > 0 ? dMin : 0.0, dMax, dSem ? " (some unset)" : "", tMin, tMax, pri - tl0(s), ult - tl0(s), s->semPts,
           cp->extradata_size, f->canais, f->taxa, mp4 ? "yes" : "no");
  }
  fflush(stdout);
  return 0;
}

// ---- playlists ------------------------------------------------------------------
typedef struct { char *b; size_t n, cap; } Txt;
static void tx(Txt *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void tx(Txt *t, const char *fmt, ...) {
  va_list a;
  int k;
  if (!t->b) { t->cap = 65536; t->b = malloc(t->cap); t->n = 0; if (!t->b) return; }
  for (;;) {
    va_start(a, fmt);
    k = vsnprintf(t->b + t->n, t->cap - t->n, fmt, a);
    va_end(a);
    if (k < 0) return;
    if ((size_t)k < t->cap - t->n) { t->n += (size_t)k; return; }
    { char *g = realloc(t->b, t->cap * 2); if (!g) return; t->b = g; t->cap *= 2; }
  }
}
// One audio rendition: AUD_SEG_S segments (short: a seek reads ~2 s).
static void plAudio(Sessao *s, Txt *t, int k) {
  int i, mp4 = EM_MP4(s->fx[k].modo);
  tx(t, "#EXTM3U\n#EXT-X-VERSION:%d\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:0\n#EXT-X-PLAYLIST-TYPE:VOD\n"
        "#EXT-X-INDEPENDENT-SEGMENTS\n", mp4 ? 7 : 3, (int)ceil(AUD_SEG_S));
  if (mp4) tx(t, "#EXT-X-MAP:URI=\"a%d_init.mp4\"\n", k);
  for (i = 0; i < s->naSeg; i++) tx(t, "#EXTINF:%.6f,\na%d_%d%s\n", s->aIni[i + 1] - s->aIni[i], k, i, mp4 ? ".m4s" : ".ts");
  tx(t, "#EXT-X-ENDLIST\n");
}
static void nomeRendicao(const Sessao *s, const Faixa *f, char *o, size_t n) {
  char *c;
  if (f->fonte)   // the donor's: where it comes from, so the TV's own list tells them apart
    snprintf(o, n, "%s (%s %dch) from %.40s", f->nome[0] ? f->nome : nomeLingua(f->lingua), f->codec, f->canais,
             s->fo[1].nome[0] ? s->fo[1].nome : "another source");
  else snprintf(o, n, "%s %d (%s %dch)", f->nome[0] ? f->nome : nomeLingua(f->lingua), f->ord + 1, f->codec, f->canais);
  for (c = o; *c; c++) if (*c == '"' || *c == ',' || (unsigned char)*c < 32) *c = ' ';
}
// RFC 6381 name of what rendition f carries (the master's form 1).
static const char *codecsAudio(const Faixa *f) {
  switch (f->modo) {
    case M_AC3: return "ac-3";
    case M_AAC: return "mp4a.40.2";
    case M_MP4: return f->id == AV_CODEC_ID_OPUS ? "Opus" : "fLaC";
    case M_FLAC: return "fLaC";
    default:
      return f->id == AV_CODEC_ID_AC3 ? "ac-3" : f->id == AV_CODEC_ID_EAC3 ? "ec-3" : f->id == AV_CODEC_ID_MP3 ? "mp4a.40.34" :
             f->id == AV_CODEC_ID_MP2 ? "mp4a.40.33" : "mp4a.40.2";
  }
}
static void plMaster(Sessao *s, Txt *t) {
  int i, mp4 = 0;
  for (i = 0; i < s->nfx; i++) if (EM_MP4(s->fx[i].modo) && !s->fx[i].foraLista) mp4 = 1;
  tx(t, "#EXTM3U\n#EXT-X-VERSION:%d\n#EXT-X-INDEPENDENT-SEGMENTS\n", mp4 ? 7 : 4);
  for (i = 0; i < s->nfx; i++) {
    const Faixa *f = &s->fx[i];
    char nome[128];
    int ch = f->modo == M_AC3 ? (f->canais > 2 ? 6 : 2) : f->modo == M_AAC ? 2 : f->canais;
    if (!LISTADA(f)) continue;
    nomeRendicao(s, f, nome, sizeof nome);
    tx(t, "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud\",NAME=\"%s\"", nome);
    if (f->lingua[0]) tx(t, ",LANGUAGE=\"%s\"", f->lingua);
    tx(t, ",DEFAULT=%s,AUTOSELECT=YES", i == s->audio ? "YES" : "NO");
    if (ch > 0) tx(t, ",CHANNELS=\"%d\"", ch);
    tx(t, ",URI=\"a%d.m3u8\"\n", i);
  }
  // The video: the converter's media playlist, its segments carry no audio
  // (the TV plays the audio of the video segments when they have one, and
  // ignores the rendition: owner's S95C, 14/11). Integers only (LC_NUMERIC).
  // Form 1 (the retry after a 4K file never asked for audio, owner 17/11):
  // CODECS with the video and every audio codec of the group, BANDWIDTH at
  // most 60 Mbit/s. Form 0 is what played on the S95C (1080p).
  if (s->variante == 1) {
    char cod[256];
    size_t u = (size_t)snprintf(cod, sizeof cod, "%s", s->vCodecs);
    for (i = 0; i < s->nfx; i++) {
      const char *c = !LISTADA(&s->fx[i]) ? NULL : codecsAudio(&s->fx[i]);
      int j, dup = 0;
      if (!c) continue;
      for (j = 0; j < i; j++) if (LISTADA(&s->fx[j]) && !strcmp(codecsAudio(&s->fx[j]), c)) dup = 1;
      if (!dup && u + strlen(c) + 2 < sizeof cod) u += (size_t)snprintf(cod + u, sizeof cod - u, "%s%s", u ? "," : "", c);
    }
    tx(t, "#EXT-X-STREAM-INF:BANDWIDTH=%ld,AUDIO=\"aud\"", s->banda + 1500000L > 60000000L ? 60000000L : s->banda + 1500000L);
    if (cod[0]) tx(t, ",CODECS=\"%s\"", cod);
  } else
  tx(t, "#EXT-X-STREAM-INF:BANDWIDTH=%ld,AUDIO=\"aud\"", s->banda + 1500000L);
  if (s->largura > 0 && s->altura > 0) tx(t, ",RESOLUTION=%dx%d", s->largura, s->altura);
  if (s->fps1000 > 0) tx(t, ",FRAME-RATE=%ld.%03ld", s->fps1000 / 1000, s->fps1000 % 1000);
  if (s->faixaVideo[0]) tx(t, ",VIDEO-RANGE=%s", s->faixaVideo);
  tx(t, "\n%s\n", s->video);
}

// ---- HTTP ---------------------------------------------------------------------------
static int enviar(int so, const void *b, size_t n, Sessao *s) {
  const char *p = b;
  while (n) {
    ssize_t r = send(so, p, n, MSG_NOSIGNAL);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { if (s && s->cancel) return 0; continue; }
    if (r <= 0) return 0;
    p += r; n -= (size_t)r;
  }
  return 1;
}
static int responder(int so, int cod, const char *tipo, const void *corpo, size_t n, int manter, int head, Sessao *s) {
  char cab[400];
  const char *msg = cod == 200 ? "OK" : cod == 404 ? "Not Found" : cod == 503 ? "Service Unavailable" : "Bad Gateway";
  snprintf(cab, sizeof cab, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-cache\r\n"
           "Connection: %s\r\n\r\n", cod, msg, tipo ? tipo : "text/plain", n, manter ? "keep-alive" : "close");
  if (!enviar(so, cab, strlen(cab), s)) return 0;
  return head || !n || enviar(so, corpo, n, s);
}
static int esperarPronto(Sessao *s) {
  struct timespec ts;
  int ok;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += ESPERA_PREP_MS / 1000;
  pthread_mutex_lock(&s->m);
  while (!s->pronto && !s->falhou && !s->cancel)
    if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT) break;
  ok = s->pronto && !s->falhou;
  pthread_mutex_unlock(&s->m);
  return ok;
}
// A generated answer: -2 = stopped (close the connection, nothing sent).
static int entregar(int so, Sessao *s, const char *nome, int e, Resp *r, const char *err, int manter, int head) {
  if (e == -2) { printf("[doador] %s dropped: %s\n", nome, err); fflush(stdout); return 0; }
  if (e < 0) {
    if (!s->cancel) { s->nErro++; snprintf(s->ultErro, sizeof s->ultErro, "%s", err); printf("[doador] %s failed: %s\n", nome, err); fflush(stdout); }
    return responder(so, 502, NULL, "", 0, 0, head, s) && 0;
  }
  return responder(so, 200, r->tipo, r->b, r->n, manter, head, s);
}
// One request. Returns 0 to close the connection.
static int atender(int so, const char *metodo, const char *caminho, int manter) {
  Sessao *s = pegar();
  char nome[128], err[160] = "";
  int head = !strcmp(metodo, "HEAD"), k = -1, i = -1, ok = 1, e;
  size_t lt;
  Resp r;
  double t0 = agoraMs();
  memset(&r, 0, sizeof r);
  if (!s || caminho[0] != '/' || strncmp(caminho + 1, s->tok, (lt = strlen(s->tok))) || caminho[1 + lt] != '/') {
    soltar(s);
    return responder(so, 404, NULL, "", 0, 0, head, NULL) && 0;
  }
  snprintf(nome, sizeof nome, "%s", caminho + 2 + lt);
  // Every request the TV makes (owner 17/11: a 4K file's session served the
  // master and an audio playlist, then nothing; the log could not say whether
  // the TV ever asked for an audio segment). All of a session's first 80,
  // then everything but the audio segments (their own line says them).
  { int nr = atomic_fetch_add(&s->pedTotal, 1) + 1, seg = sscanf(nome, "a%d_%d.", &k, &i) == 2;
    if (seg || strstr(nome, "_init.mp4")) atomic_fetch_add(&s->pedAudio, 1);
    if (!strcmp(nome, "master.m3u8")) atomic_fetch_add(&s->pedVideo, 1);
    if (nr <= 80 || !seg) {
      printf("[doador] #%d -> %s %s (+%.0f ms since the session opened, %d audio request(s) so far)\n", nr, metodo, nome,
             t0 - s->abertoMs, atomic_load(&s->pedAudio));
      fflush(stdout);
    }
    k = -1; i = -1; }
  if (!esperarPronto(s)) {
    printf("[doador] request %s: the session is not ready (%s)\n", nome, s->motivo[0] ? s->motivo : "timeout");
    fflush(stdout);
    ok = responder(so, 503, NULL, "", 0, 0, head, s) && 0;
    soltar(s); return ok;
  }
  if (!strcmp(nome, "master.m3u8")) {
    Txt t = { 0 };
    plMaster(s, &t);
    ok = t.b && responder(so, 200, "application/vnd.apple.mpegurl", t.b, t.n, manter, head, s);
    printf("[doador] master.m3u8 served (%zu bytes, form %d): the converter's video + %d audio rendition(s)\n", t.n, s->variante, s->nfx);
    // The master itself, line by line (relative URIs; the video's local URL
    // carries the converter's session token: cut).
    if (t.b) {
      const char *p = t.b, *fim = t.b + t.n;
      while (p < fim) {
        const char *nl = memchr(p, '\n', (size_t)(fim - p)), *e = nl ? nl : fim, *h = strstr(p, "://");
        if (h && h < e) printf("[doador]   | <the converter's video playlist (local URL)>\n");
        else printf("[doador]   | %.*s\n", (int)(e - p), p);
        p = nl ? nl + 1 : fim;
      }
    }
    fflush(stdout);
    free(t.b);
  } else if (sscanf(nome, "a%d.m3u8", &k) == 1 && strstr(nome, ".m3u8") && !strchr(nome, '_')) {
    Txt t = { 0 };
    if (k < 0 || k >= s->nfx || s->fx[k].modo == M_FORA) { soltar(s); return responder(so, 404, NULL, "", 0, manter, head, s); }
    plAudio(s, &t, k);
    ok = t.b && responder(so, 200, "application/vnd.apple.mpegurl", t.b, t.n, manter, head, s);
    printf("[doador] audio playlist a%d (%s audio %d, %s, %d segments of %.0f s) served\n", k, s->fx[k].fonte ? "donor" : "file",
           s->fx[k].ord, MODO_NOME[s->fx[k].modo], s->naSeg, AUD_SEG_S);
    // Its head, once per track (owner's log 6: the TV took a 4K donor's
    // playlist and never asked for a segment).
    if (t.b && k < 64 && !(s->plDito & (UINT64_C(1) << k))) {
      const char *c = t.b; int l = 0;
      s->plDito |= UINT64_C(1) << k;
      while (*c && l < 9) {
        const char *e = strchr(c, '\n'); int len = e ? (int)(e - c) : (int)strlen(c);
        printf("[doador]   a%d | %.*s\n", k, len, c);
        l++; c = e ? e + 1 : c + len;
      }
    }
    fflush(stdout);
    free(t.b);
  } else if (sscanf(nome, "a%d_init.mp4", &k) == 1 && strstr(nome, "_init.mp4")) {
    if (k < 0 || k >= s->nfx || !EM_MP4(s->fx[k].modo)) { soltar(s); return responder(so, 404, NULL, "", 0, manter, head, s); }
    pthread_mutex_lock(&s->gm);
    e = gerarAudio(s, k, -1, &r, err, sizeof err, -1, -1);
    pthread_mutex_unlock(&s->gm);
    printf("[doador] a%d init: %s (%zu bytes)\n", k, e < 0 ? err : "ok", r.n);
    fflush(stdout);
    ok = entregar(so, s, nome, e, &r, err, manter, head);
  } else if (sscanf(nome, "a%d_%d.", &k, &i) == 2) {
    long chave = (long)(k + 1) * 1000000L + i;
    if (k < 0 || k >= s->nfx || s->fx[k].modo == M_FORA || i < 0 || i >= s->naSeg) {
      soltar(s); return responder(so, 404, NULL, "", 0, manter, head, s);
    }
    atomic_store(&s->espChave, chave); atomic_fetch_add(&s->espera, 1);
    pthread_mutex_lock(&s->gm);
    atomic_fetch_sub(&s->espera, 1);
    e = gerarAudio(s, k, i, &r, err, sizeof err, so, chave);
    pthread_mutex_unlock(&s->gm);
    ok = entregar(so, s, nome, e, &r, err, manter, head);
  } else ok = responder(so, 404, NULL, "", 0, manter, head, s);
  if (r.b && (agoraMs() - t0) > 4000) { printf("[doador] %s took %.0f ms (the player waited)\n", nome, agoraMs() - t0); fflush(stdout); }
  free(r.b);
  soltar(s);
  return ok;
}
static int temSemCaixa(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}
static void *conexao(void *u) {
  int so = (int)(intptr_t)u;
  char b[8192];
  size_t n = 0;
  for (;;) {
    char *fim, metodo[8], caminho[512];
    int manter;
    struct pollfd p = { so, POLLIN, 0 };
    b[n] = 0;
    while (!(fim = strstr(b, "\r\n\r\n"))) {
      ssize_t k;
      if (n >= sizeof b - 1 || poll(&p, 1, 15000) <= 0) goto sair;
      k = recv(so, b + n, sizeof b - 1 - n, 0);
      if (k <= 0) goto sair;
      n += (size_t)k; b[n] = 0;
    }
    if (sscanf(b, "%7s %511s", metodo, caminho) != 2) goto sair;
    { char *q = strchr(caminho, '?'); if (q) *q = 0; }
    manter = !temSemCaixa(b, "\r\nConnection: close");
    if (!atender(so, metodo, caminho, manter) || !manter) goto sair;
    { size_t usado = (size_t)(fim + 4 - b); memmove(b, b + usado, n - usado); n -= usado; }
  }
sair:
  close(so);
  atomic_fetch_sub(&vivos, 1);
  return NULL;
}
static void *aceitar(void *u) {
  (void)u;
  for (;;) {
    int so = accept(ouvinte, NULL, NULL);
    pthread_t t;
    if (so < 0) { if (errno != EINTR) usleep(100000); continue; }
    if (atomic_load(&vivos) >= TBA_CONEXOES) {
      const char *r = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      send(so, r, strlen(r), MSG_NOSIGNAL); close(so);
      printf("[doador] %d connections open: 503\n", TBA_CONEXOES); fflush(stdout);
      continue;
    }
    { struct timeval tv = { 1, 0 }; setsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv); }
    atomic_fetch_add(&vivos, 1);
    if (pthread_create(&t, NULL, conexao, (void *)(intptr_t)so) == 0) pthread_detach(t);
    else { close(so); atomic_fetch_sub(&vivos, 1); }
  }
  return NULL;
}
static int subirServidor(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  int um = 1, so;
  pthread_t t;
  if (ouvinte >= 0) return 1;
  so = socket(AF_INET, SOCK_STREAM, 0);
  if (so < 0) return 0;
  setsockopt(so, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(so, (struct sockaddr *)&a, sizeof a) < 0 || listen(so, 16) < 0 ||
      getsockname(so, (struct sockaddr *)&a, &al) < 0) { close(so); return 0; }
  porta = ntohs(a.sin_port);
  ouvinte = so;
  if (pthread_create(&t, NULL, aceitar, NULL) != 0) { close(so); ouvinte = -1; return 0; }
  pthread_detach(t);
  return 1;
}

// ---- API ----------------------------------------------------------------------------
int doa_disponivel(void) { return avcodec_find_encoder(AV_CODEC_ID_AC3) != NULL; }

int doa_abrir(const DoaAbrir *o, char *out, size_t n) {
  Sessao *s;
  pthread_t t;
  int doador;
  size_t lv;
  if (out && n) out[0] = 0;
  if (!o || !o->video || !*o->video || !o->orig || !*o->orig || !out) return 0;
  doador = o->doador && *o->doador;
  doa_fechar();
  pthread_mutex_lock(&trava);
  if (!subirServidor()) { pthread_mutex_unlock(&trava); printf("[doador] local server did not start\n"); return 0; }
  pthread_mutex_unlock(&trava);
  s = calloc(1, sizeof *s);
  if (!s) return 0;
  atomic_store(&s->refs, 2);   // the API's, the preparation's
  pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->c, NULL); pthread_mutex_init(&s->gm, NULL); pthread_mutex_init(&s->mm, NULL);
  pthread_mutex_init(&s->mpm, NULL); s->mapaPos = -1; s->mTrecho = -1;
  s->fo[0].k = s->fo[1].k = 1.0;
  s->ppmPedido = o->doador_ppm; s->seguir = o->seguir;
  if (o->doador_ppm) s->fo[1].k = 1.0 + o->doador_ppm / 1e6;
  atomic_store(&s->espChave, -1L);
  s->flacConv = o->flac ? 1 : 0;
  s->audio = o->audio; s->gFaixa = -1; s->gSeg = -1; s->gFonte = -1; s->proxPts = AV_NOPTS_VALUE; s->dExtra = -1;
  s->abertoMs = agoraMs();
  s->variante = o->variante;
  s->rendLimite = o->rend_limite > 0 ? o->rend_limite : 0;
  s->umaConexao = !o->duas_conexoes;
  s->nfo = doador ? 2 : 1;
  // The converter's master (".../m.m3u8") names its media playlist v.m3u8:
  // the TV gets that one straight (a master cannot point at a master).
  s->video = malloc((lv = strlen(o->video)) + 1);
  if (s->video) {
    memcpy(s->video, o->video, lv + 1);
    if (lv > 7 && !strcmp(s->video + lv - 7, "/m.m3u8")) s->video[lv - 6] = 'v';
  }
  s->fo[0].url = strdup(o->orig); s->fo[0].cab = strdup(o->cab ? o->cab : "");
  if (doador) {
    s->fo[1].url = strdup(o->doador); s->fo[1].cab = strdup(o->doador_cab ? o->doador_cab : "");
    snprintf(s->fo[1].nome, sizeof s->fo[1].nome, "%s", o->doador_nome ? o->doador_nome : "");
    s->fo[1].desloc = o->doador_ms / 1000.0; s->doadorMs = o->doador_ms;
    snprintf(s->doadorLingua, sizeof s->doadorLingua, "%s", o->doador_lingua ? o->doador_lingua : "");
    s->doadorFaixa = o->doador_faixa > 0 ? o->doador_faixa : 0;
    snprintf(s->dicaLingua, sizeof s->dicaLingua, "%s", o->dica_lingua ? o->dica_lingua : "");
    snprintf(s->dicaCodec, sizeof s->dicaCodec, "%s", o->dica_codec ? o->dica_codec : "");
    snprintf(s->dicaTitulo, sizeof s->dicaTitulo, "%s", o->dica_titulo ? o->dica_titulo : "");
    s->dicaCanais = o->dica_canais;
    s->doadorPadrao = o->doador_padrao;
  }
  gerarToken(s->tok, 28);
  if (!s->video || !s->fo[0].url || !s->fo[0].cab || (doador && (!s->fo[1].url || !s->fo[1].cab))) {
    atomic_store(&s->refs, 1); soltar(s); return 0;
  }
  pthread_mutex_lock(&trava);
  atual = s;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&t, NULL, preparar, s) != 0) { atomic_fetch_sub(&s->refs, 1); falhar(s, "no thread"); }
  else pthread_detach(t);
  snprintf(out, n, "http://127.0.0.1:%d/%s/master.m3u8", porta, s->tok);
  printf("[doador] session open: the converter's video, file audio %d, FLAC %s%s%s%s; %s; the player opens http://127.0.0.1:%d/<token>/master.m3u8\n",
         o->audio, s->flacConv ? "converted" : "as is", doador ? ", donor \"" : ", no donor", doador ? s->fo[1].nome : "",
         doador ? "\"" : "", s->umaConexao ? "one audio connection" : "a connection per audio source", porta);
  if (doador) printf("[doador] donor: offset %+d ms, starts on %s%s%s\n", o->doador_ms, s->doadorPadrao ? "its track" : "the file's track",
                     s->doadorPadrao && s->doadorLingua[0] ? " in " : "", s->doadorPadrao ? s->doadorLingua : "");
  fflush(stdout);
  return 1;
}
void doa_fechar(void) {
  Sessao *s;
  pthread_mutex_lock(&trava);
  s = atual; atual = NULL;
  pthread_mutex_unlock(&trava);
  if (!s) return;
  s->cancel = 1;
  { int k; for (k = 0; k < TBA_FONTES; k++) if (s->fo[k].eng) dts_engine_interrupt(s->fo[k].eng, 1); }
  pthread_mutex_lock(&s->m); if (s->mEng) dts_engine_cancel(s->mEng); pthread_mutex_unlock(&s->m);
  dts_remux_set_tap(NULL, s, -1);
  pthread_mutex_lock(&s->m); pthread_cond_broadcast(&s->c); pthread_mutex_unlock(&s->m);
  printf("[doador] session closed: %ld audio segments, %lld KB, %ld dropped, %ld failed%s%s\n",
         s->nSeg, s->bytes / 1024, s->nLarg, s->nErro, s->ultErro[0] ? "; last error: " : "", s->ultErro);
  fflush(stdout);
  soltar(s);
}
int doa_ativo(void) { Sessao *s = pegar(); int v = s != NULL; soltar(s); return v; }
int doa_falhou(char *m, size_t n) {
  Sessao *s = pegar();
  int v = 0;
  if (m && n) m[0] = 0;
  if (s) { pthread_mutex_lock(&s->m); v = s->falhou; if (v && m && n) snprintf(m, n, "%s", s->motivo); pthread_mutex_unlock(&s->m); }
  soltar(s);
  return v;
}
int doa_pronto(void) { Sessao *s = pegar(); int v = s && s->pronto; soltar(s); return v; }
int doa_rendicao_de(int ord) {
  Sessao *s = pegar();
  int v = -1, i, k = 0;
  if (s && s->pronto)
    for (i = 0; i < s->nfx; i++) {
      if (!LISTADA(&s->fx[i])) continue;
      if (s->fx[i].chave == ord) { v = k; break; }
      k++;
    }
  soltar(s);
  return v;
}
void doa_definir_audio(int ord) {
  Sessao *s = pegar();
  int i;
  if (s && s->pronto) {
    pthread_mutex_lock(&s->gm);
    for (i = 0; i < s->nfx; i++) if (s->fx[i].chave == ord && LISTADA(&s->fx[i])) s->audio = i;
    pthread_mutex_unlock(&s->gm);
    if (ord >= DOA_DOADOR(0)) printf("[doador] donor audio %d is the session's track now\n", ord - DOA_DOADOR(0));
    else printf("[doador] audio %d is the session's track now\n", ord);
    fflush(stdout);
  }
  soltar(s);
}
int doa_audio(void) { Sessao *s = pegar(); int v = s && s->pronto && s->audio >= 0 ? s->fx[s->audio].chave : -1; soltar(s); return v; }
// The playing file's own tracks (doador_tpk.h): its codec and channels as in
// the file (the Scan again list names them as the file has them).
int doa_faixas_arquivo(DoaRendicao *out, int max) {
  Sessao *s = pegar();
  int i, k = 0;
  if (s && s->pronto)
    for (i = 0; i < s->nfx && k < max; i++) {
      const Faixa *f = &s->fx[i];
      if (f->fonte != 0 || !avcodec_find_decoder(f->id)) continue;
      if (out) {
        DoaRendicao *r = &out[k];
        memset(r, 0, sizeof *r);
        r->chave = f->chave; r->fonte = 0; r->ord = f->ord; r->si = f->si; r->canais = f->canais;
        snprintf(r->codecFonte, sizeof r->codecFonte, "%s", f->codec);
        snprintf(r->codec, sizeof r->codec, "%s", f->codec);
        snprintf(r->lingua, sizeof r->lingua, "%s", f->lingua);
        snprintf(r->nome, sizeof r->nome, "%s", f->nome);
      }
      k++;
    }
  soltar(s);
  return k;
}
int doa_rendicoes(DoaRendicao *out, int max) {
  Sessao *s = pegar();
  int i, k = 0;
  if (s && s->pronto)
    for (i = 0; i < s->nfx && k < max; i++) {
      const Faixa *f = &s->fx[i];
      if (!LISTADA(f)) continue;
      if (out) {
        DoaRendicao *r = &out[k];
        memset(r, 0, sizeof *r);
        r->chave = f->chave; r->fonte = f->fonte; r->ord = f->ord; r->si = f->si;
        snprintf(r->codecFonte, sizeof r->codecFonte, "%s", f->codec);
        r->canais = f->modo == M_AC3 ? (f->canais > 2 ? 6 : 2) : f->modo == M_AAC ? 2 : f->canais;
        snprintf(r->lingua, sizeof r->lingua, "%s", f->lingua);
        snprintf(r->nome, sizeof r->nome, "%s", f->nome);
        snprintf(r->codec, sizeof r->codec, "%s", f->modo == M_AC3 ? "ac3" : f->modo == M_AAC ? "aac" : f->modo == M_FLAC ? "flac" : f->codec);
      }
      k++;
    }
  soltar(s);
  return k;
}
void doa_desloc(int ms) {
  Sessao *s = pegar();
  if (s && s->pronto && s->nfo > 1 && !s->fo[1].falhou && s->fo[1].ic) {
    double antes;
    // THE EDIT MAP (owner 18/11): the app's offset is the playhead stretch's.
    // A change (by hand, Scan again's result, the first audio refine) shifts
    // every stretch by it; the next segments carry it.
    pthread_mutex_lock(&s->mpm);
    if (s->mapaN > 0) {
      int d = ms - s->doadorMs, i, primeiro = s->fino1Pend;
      for (i = 0; i < s->mapaN; i++) s->mapa[i].ms += d;
      if (primeiro && s->mapaPos >= 0 && s->mapaPos < s->mapaN) s->mapa[s->mapaPos].refinado = 1;
      s->fino1Pend = 0;
      s->doadorMs = ms;
      s->fo[1].desloc = s->fo[0].v0 - s->fo[1].v0 * s->fo[1].k + ms / 1000.0;
      pthread_mutex_unlock(&s->mpm);
      printf("[doador] map shifted by %+d ms (%s): the playhead's stretch %+d ms, the next segments carry it\n", d,
             primeiro ? "first refine" : "manual or a match", ms);
      fflush(stdout);
      soltar(s);
      return;
    }
    s->fino1Pend = 0;
    pthread_mutex_unlock(&s->mpm);
    pthread_mutex_lock(&s->gm);
    antes = s->fo[1].desloc;
    s->fo[1].desloc = s->fo[0].v0 - s->fo[1].v0 * s->fo[1].k + ms / 1000.0;
    s->doadorMs = ms;
    if (s->gFonte == 1) s->gOk = 0;   // the next segment repositions with the new offset
    pthread_mutex_unlock(&s->gm);
    printf("[doador] donor offset %+d ms (its time %+.3f s = the file's, was %+.3f s): the next segments carry it\n", ms,
           s->fo[1].desloc, antes);
    fflush(stdout);
  }
  soltar(s);
}
int doa_desloc_ms(void) { Sessao *s = pegar(); int v = s ? s->doadorMs : 0; soltar(s); return v; }
int doa_estado(char *m, size_t n) {
  Sessao *s = pegar();
  int v = 0;
  if (m && n) m[0] = 0;
  if (s && s->nfo > 1) {
    if (s->fo[1].falhou) { v = -1; if (m && n) snprintf(m, n, "%s", s->fo[1].motivo); }
    else if (s->pronto) v = 1;
  }
  soltar(s);
  return v;
}
int doa_diferenca(int *corte, int *velocidade, int *ppm) {
  Sessao *s = pegar();
  int v = 0;
  if (s) {
    pthread_mutex_lock(&s->m);
    if (s->difOk && !s->fo[1].falhou) {
      v = 1;
      if (corte) *corte = s->difCorte;
      if (velocidade) *velocidade = s->difVel;
      if (ppm) *ppm = s->difPpm;
    }
    pthread_mutex_unlock(&s->m);
  }
  soltar(s);
  return v;
}
int doa_sondar(const char *url, const char *cab, DoaSonda *out, int max) {
  DtsEngine *e;
  AVFormatContext *ic;
  int i, n = 0;
  if (!url || !*url || !out || max <= 0 || !(e = dts_engine_create())) return -1;
  if (dts_engine_open_demux(e, url, cab ? cab : "") < 0) { dts_engine_destroy(e); return -1; }
  ic = dts_engine_avformat(e);
  if (ic->duration > 0 && ic->duration < (int64_t)DOA_CLIP_S * AV_TIME_BASE) {
    printf("[doador] tracks: %.0f s file, the debrid placeholder clip\n", ic->duration / 1e6);
    fflush(stdout);
    dts_engine_destroy(e);
    return -3;
  }
  for (i = 0; i < (int)ic->nb_streams && n < max; i++) {
    AVStream *st = ic->streams[i];
    AVDictionaryEntry *l, *t;
    Faixa f;
    if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
    memset(&f, 0, sizeof f);
    f.id = st->codecpar->codec_id; f.canais = st->codecpar->ch_layout.nb_channels;
    if (modoDe(&f, 0) == M_FORA) continue;   // not offered by the server either
    l = av_dict_get(st->metadata, "language", NULL, 0);
    t = av_dict_get(st->metadata, "title", NULL, 0);
    memset(&out[n], 0, sizeof out[n]);
    out[n].si = i; out[n].canais = f.canais;
    snprintf(out[n].codec, sizeof out[n].codec, "%s", avcodec_get_name(f.id));
    snprintf(out[n].lingua, sizeof out[n].lingua, "%s", l && strcmp(l->value, "und") ? l->value : "");
    snprintf(out[n].titulo, sizeof out[n].titulo, "%s", t ? t->value : "");
    n++;
  }
  printf("[doador] tracks of another version: %d audio track(s) offered\n", n);
  fflush(stdout);
  dts_engine_destroy(e);
  return n;
}
typedef struct {
  pthread_mutex_t m;
  pthread_cond_t c;
  DtsEngine *e;
  int feito, vencido, prazo;
  atomic_int refs;
} PreVigia;
static void preSoltar(PreVigia *v) {
  if (atomic_fetch_sub(&v->refs, 1) != 1) return;
  pthread_mutex_destroy(&v->m); pthread_cond_destroy(&v->c);
  free(v);
}
static void *preVigiar(void *u) {
  PreVigia *v = u;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += v->prazo / 1000;
  ts.tv_nsec += (long)(v->prazo % 1000) * 1000000L;
  if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
  pthread_mutex_lock(&v->m);
  while (!v->feito)
    if (pthread_cond_timedwait(&v->c, &v->m, &ts) == ETIMEDOUT) break;
  if (!v->feito && v->e) { v->vencido = 1; dts_engine_cancel(v->e); }
  pthread_mutex_unlock(&v->m);
  preSoltar(v);
  return NULL;
}
int doa_preabrir(const char *url, const char *cab, int prazo_ms, char *motivo, size_t n) {
  PreVigia *v;
  pthread_t t;
  AVFormatContext *ic;
  double t0 = agoraMs();
  int r = 0, vigia;
  if (motivo && n) motivo[0] = 0;
  if (!url || !*url) { if (motivo && n) snprintf(motivo, n, "no address"); return 0; }
  if (!(v = calloc(1, sizeof *v))) { if (motivo && n) snprintf(motivo, n, "out of memory"); return 0; }
  pthread_mutex_init(&v->m, NULL); pthread_cond_init(&v->c, NULL);
  v->prazo = prazo_ms > 1000 ? prazo_ms : 1000;
  atomic_init(&v->refs, 1);
  if (!(v->e = dts_engine_create())) { preSoltar(v); if (motivo && n) snprintf(motivo, n, "out of memory"); return 0; }
  atomic_fetch_add(&v->refs, 1);
  vigia = pthread_create(&t, NULL, preVigiar, v) == 0;
  if (vigia) pthread_detach(t); else atomic_fetch_sub(&v->refs, 1);
  if (dts_engine_open_demux(v->e, url, cab ? cab : "") < 0) {
    pthread_mutex_lock(&v->m);
    if (motivo && n) {
      if (v->vencido) snprintf(motivo, n, "no answer in %d s", v->prazo / 1000);
      else snprintf(motivo, n, "%s", dts_engine_error(v->e));
    }
    pthread_mutex_unlock(&v->m);
  } else {
    ic = dts_engine_avformat(v->e);
    if (ic && ic->duration > 0 && ic->duration < (int64_t)DOA_CLIP_S * AV_TIME_BASE) {
      if (motivo && n) snprintf(motivo, n, "External audio: this version is not ready on the debrid service yet (placeholder clip)");
      r = -3;
    } else r = 1;
  }
  pthread_mutex_lock(&v->m);
  v->feito = 1;
  pthread_cond_broadcast(&v->c);
  { DtsEngine *e = v->e; v->e = NULL;   // the watchdog no longer touches it
    pthread_mutex_unlock(&v->m);
    dts_engine_destroy(e); }
  printf("[doador] pre-open: %s in %.0f ms%s%s\n", r == 1 ? "it answered" : r == -3 ? "the placeholder clip" : "no",
         agoraMs() - t0, r == 1 ? "" : ": ", r == 1 ? "" : (motivo ? motivo : ""));
  fflush(stdout);
  preSoltar(v);
  return r;
}
void doa_dopasuj(int automatico) {
  Sessao *s = pegar();
  if (s) matchIniciar(s, automatico, 0);
  soltar(s);
}
void doa_dopasuj_fino(void) {
  Sessao *s = pegar();
  if (s) matchIniciarF(s, 1, 0, 1);
  soltar(s);
}
int doa_pedidos_audio(void) { Sessao *s = pegar(); int v = s ? atomic_load(&s->pedAudio) : 0; soltar(s); return v; }
int doa_pedidos(void) { Sessao *s = pegar(); int v = s ? atomic_load(&s->pedTotal) : 0; soltar(s); return v; }
int doa_legendas(int fonte, DoaLegInfo *out, int max) {
  Sessao *s = pegar();
  int n = 0, i;
  if (s && s->pronto && fonte >= 0 && fonte < s->nfo && !s->fo[fonte].falhou)
    for (i = 0; i < s->fo[fonte].nLeg && n < max; i++) out[n++] = s->fo[fonte].leg[i];
  soltar(s);
  return n;
}
int doa_legenda_tempos(int fonte, int k, double *t, int max) {
  Sessao *s = pegar();
  int n = 0;
  if (s && s->pronto && fonte >= 0 && fonte < s->nfo && !s->fo[fonte].falhou && k >= 0 && k < s->fo[fonte].nLeg) {
    n = s->fo[fonte].leg[k].n < max ? s->fo[fonte].leg[k].n : max;
    memcpy(t, s->fo[fonte].legT[k], sizeof *t * (size_t)n);
  }
  soltar(s);
  return n;
}
// The subtitle match's line (file = k * donor + desloc) handed to the app as a
// match result: ms against the session's base at that speed, ppm the speed.
void doa_aplicar_legendas(double k, double desloc, double conf) {
  Sessao *s = pegar();
  if (s && s->pronto && s->nfo > 1 && !s->fo[1].falhou && k > 0.9 && k < 1.1) {
    DoaDopasuj r;
    memset(&r, 0, sizeof r);
    r.ms = msDe(s, desloc, k);
    r.ppm = fabs(k - 1.0) > 0.0002 ? (int)lround((k - 1.0) * 1e6) : 0;
    r.como = 6;
    pthread_mutex_lock(&s->m);
    s->legConf = conf > 0 ? conf : 0.5; s->legDesloc = baseDe(s, k) + r.ms / 1000.0;
    // The subtitles placed it: the edit map does what the follow did (owner 18/11).
    s->legMs = r.ms; s->legOk = 1; s->segAtivo = 0;
    if (s->mEstado != 1) {
      s->mRes = r; s->mEstado = 2; s->mNovo = 1; s->mAuto = 1; s->mFino = 0;
      printf("[doador] subtitle match: file = %.6f x donor %+.3f s -> offset %+d ms, speed %+d ppm (was %+d ms)\n", k, desloc, r.ms,
             r.ppm, s->doadorMs);
    } else printf("[doador] subtitle match: an audio match is running, its result waits for it\n");
    pthread_mutex_unlock(&s->m);
    fflush(stdout);
  }
  soltar(s);
}
// THE EDIT MAP (owner 18/11; doador_tpk.h).
void doa_mapa_definir(const DoaMapa *m, double pos) {
  Sessao *s = pegar();
  if (s && s->pronto && s->nfo > 1 && !s->fo[1].falhou) {
    int i, i0, d = 0, ancora;
    double k = s->fo[1].k;
    pthread_mutex_lock(&s->mpm);
    if (!m || m->n < 2 || m->n > DOA_MAPA_MAX || fabs(m->k - k) > 1e-6) {
      if (s->mapaN) printf("[doador] edit map cleared: one offset (%+d ms)\n", s->doadorMs);
      else if (m && m->n >= 2) printf("[doador] edit map not used: its speed %.6f is not the donor's %.6f\n", m->k, k);
      s->mapaN = 0; s->mapaPos = -1; s->fino1Pend = 0;
      pthread_mutex_unlock(&s->mpm);
      fflush(stdout);
      soltar(s);
      return;
    }
    s->mapaN = m->n;
    for (i = 0; i < m->n; i++) {
      s->mapa[i].ini = i ? m->t[i].ini : -1e18; s->mapa[i].fim = i + 1 < m->n ? m->t[i].fim : 1e18;
      s->mapa[i].ms = s->mapa[i].legMs = msDe(s, m->t[i].desloc, k); s->mapa[i].refinado = 0;
    }
    i0 = trechoEm(s, pos);
    // The offset in force is not the subtitle answer any more (an audio refine
    // or a change by hand came first): the whole map follows it.
    ancora = s->legOk && s->doadorMs != s->legMs;
    if (ancora) {
      d = s->doadorMs - s->mapa[i0].ms;
      for (i = 0; i < s->mapaN; i++) s->mapa[i].ms += d;
      s->mapa[i0].refinado = 1;
    }
    s->fino1Pend = 0;
    s->mapaPos = i0; s->posApp = pos;
    if (s->mapa[i0].ms != s->doadorMs) { s->doadorMs = s->mapa[i0].ms; s->segEvento = 5; s->segMs = s->doadorMs; }
    s->fo[1].desloc = baseDe(s, k) + s->doadorMs / 1000.0;
    s->segAtivo = 0;
    printf("[doador] edit map in force: %d stretches", s->mapaN);
    for (i = 0; i < s->mapaN; i++)
      printf("%s%.0f-%.0f s %+d ms", i ? ", " : ": ", i ? s->mapa[i].ini : 0.0, i + 1 < s->mapaN ? s->mapa[i].fim : s->fo[0].dur, s->mapa[i].ms);
    printf("; the playhead (%.1f s) in stretch %d\n", pos, i0 + 1);
    if (ancora) printf("[doador] map shifted by %+d ms (the offset in force, %+d ms, came after the subtitle answer %+d ms)\n", d, s->doadorMs, s->legMs);
    pthread_mutex_unlock(&s->mpm);
    fflush(stdout);
  }
  soltar(s);
}
void doa_posicao(double pos) {
  Sessao *s = pegar();
  if (s) {
    pthread_mutex_lock(&s->mpm);
    s->posApp = pos;
    if (s->mapaN > 0) {
      int i = trechoEm(s, pos);
      if (i != s->mapaPos) {
        s->mapaPos = i;
        printf("[doador] the playhead (%.1f s) is in stretch %d (%.0f-%.0f s): %+d ms%s\n", pos, i + 1, i ? s->mapa[i].ini : 0.0,
               i + 1 < s->mapaN ? s->mapa[i].fim : s->fo[0].dur, s->mapa[i].ms, s->mapa[i].refinado > 0 ? ", refined" : "");
        if (s->mapa[i].ms != s->doadorMs) {
          s->doadorMs = s->mapa[i].ms; s->segEvento = 5; s->segMs = s->doadorMs;
          s->fo[1].desloc = baseDe(s, s->fo[1].k) + s->doadorMs / 1000.0;
        }
        fflush(stdout);
      }
    }
    pthread_mutex_unlock(&s->mpm);
  }
  soltar(s);
}
int doa_mapa_trecho(double pos, double *ini, double *fim, int *refinado) {
  Sessao *s = pegar();
  int v = -1;
  if (s) {
    pthread_mutex_lock(&s->mpm);
    if (s->mapaN > 0) {
      v = trechoEm(s, pos);
      if (ini) *ini = v ? s->mapa[v].ini : -1e18;
      if (fim) *fim = v + 1 < s->mapaN ? s->mapa[v].fim : s->fo[0].dur;
      if (refinado) *refinado = s->mapa[v].refinado;
    }
    pthread_mutex_unlock(&s->mpm);
  }
  soltar(s);
  return v;
}
void doa_dopasuj_fino_trecho(int trecho) {
  Sessao *s = pegar();
  if (s && trecho >= 0) matchIniciarT(s, 1, 0, 1, trecho);
  soltar(s);
}
int doa_dopasuj_resultado(DoaDopasuj *out, int *automatico) {
  Sessao *s = pegar();
  int v = 0;
  if (s) {
    pthread_mutex_lock(&s->m);
    if (s->mNovo) {
      s->mNovo = 0;
      v = s->mEstado == 2 ? 1 : -1;
      if (out) *out = s->mRes;
      if (automatico) *automatico = s->mAuto;
      s->mEstado = 0;
    }
    pthread_mutex_unlock(&s->m);
  }
  soltar(s);
  return v;
}
int doa_doador_saude(int *lento) {
  Sessao *s = pegar();
  int v = 0;
  if (lento) *lento = 0;
  if (s) {
    pthread_mutex_lock(&s->m);
    v = s->dSemResposta == 1; if (v) s->dSemResposta = 2;
    if (lento && s->dLento >= 2 && !s->dLentoDito) { *lento = 1; s->dLentoDito = 1; }
    pthread_mutex_unlock(&s->m);
  }
  soltar(s);
  return v;
}
int doa_dopasuj_ativo(void) { Sessao *s = pegar(); int v = s && s->mEstado == 1; soltar(s); return v; }
int doa_seguir_evento(int *ms) {
  Sessao *s = pegar();
  int v = 0;
  if (s) {
    pthread_mutex_lock(&s->m);
    v = s->segEvento; s->segEvento = 0;
    if (v && ms) *ms = s->segMs;
    pthread_mutex_unlock(&s->m);
  }
  soltar(s);
  return v;
}
void doa_log(void) {
  Sessao *s = pegar();
  if (s) {
    DtsEngineRings an;
    int meus = -1, por[4], socks = skContar(&meus, por), k;
    dts_engine_rings(&an, 0);
    printf("[doador] 10s: %s, audio segments %ld (%lld KB, avg %.0f ms), dropped %ld, failed %ld%s%s\n",
           s->pronto ? "ready" : s->falhou ? "failed" : "preparing", s->nSeg, s->bytes / 1024, s->nSeg ? s->msLer / s->nSeg : 0.0,
           s->nLarg, s->nErro, s->ultErro[0] ? "; last: " : "", s->ultErro);
    // The spike's questions (owner 15/11): connections to the files' hosts
    // (all of Nuvio's, the converter's included, and the TV player's), the
    // readers' memory, and what switching between the sources costs.
    printf("[doador] 10s connections: %d to the watched hosts (%d of them Nuvio's: file host %d, donor host %d)%s; "
           "requests %d (audio %d); reader rings alive %d = %lld MiB (peak %d = %lld MiB)",
           socks, meus, por[0], por[1], socks < 0 ? " (not readable here)" : "", atomic_load(&s->pedTotal), atomic_load(&s->pedAudio),
           an.alive, an.bytes >> 20, an.peak, an.peak_bytes >> 20);
    for (k = 0; k < s->nfo; k++)
      printf("; %s %s, gave back %ld, took again %ld", k ? "donor" : "file",
             s->fo[k].falhou ? "failed" : !s->fo[k].ic ? "closed" : s->fo[k].conectada ? "connected" : "released",
             s->fo[k].nSoltas, s->fo[k].nVoltas);
    for (k = 0; k < s->nfo; k++)
      if (s->fo[k].msVolta > 0) printf("; %s's last reconnection to its segment out %.0f ms", k ? "donor" : "file", s->fo[k].msVolta);
    if (s->nfo > 1) printf("; donor offset %+d ms", s->doadorMs);
    printf("\n");
    fflush(stdout);
  }
  soltar(s);
}
#endif
