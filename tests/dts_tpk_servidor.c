// The REAL loopback server of the Samsung DTS conversion (src/dts/dts_hls_tpk.c
// with dts_remux.c, dts_engine.c and FFmpeg) on the host, against the local
// origin of tests/dts_tpk_servidor_origem.py. Run by tests/dts_tpk_servidor.sh,
// which also checks the log lines this prints to stdout.
//
//   dts_tpk_servidor <port> padrao <dir>   HLS: 2 s segments, 1 s prebuffer
//   dts_tpk_servidor <port> cancelar|fila|escuta|faixa <dir>   review blockers
#include "../src/dts/dts_hls_tpk.h"
#include <libavformat/avformat.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// --- a small HTTP client -------------------------------------------------------
static int falhas;
#define CONFERE(c, ...) do { if (c) { printf("ok   "); printf(__VA_ARGS__); printf("\n"); } \
                             else { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); falhas++; } fflush(stdout); } while (0)

typedef struct { int status; char cab[2048]; unsigned char *corpo; size_t n; } Resp;

static int conectar(int porta) {
  struct sockaddr_in a;
  int s = socket(AF_INET, SOCK_STREAM, 0);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_port = htons((unsigned short)porta);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (s < 0 || connect(s, (struct sockaddr *)&a, sizeof a) < 0) { if (s >= 0) close(s); return -1; }
  { struct timeval tv = { 60, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv); }
  return s;
}

// Whole response of one request on a fresh connection (Connection: close),
// chunked bodies decoded. extra: more header lines ("Range: ...\r\n").
static int pedir(int porta, const char *caminho, const char *extra, Resp *r) {
  char req[1024];
  unsigned char *b = NULL;
  size_t n = 0, cap = 0;
  int s = conectar(porta);
  memset(r, 0, sizeof *r);
  if (s < 0) return -1;
  snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n%s\r\n", caminho, extra ? extra : "");
  if (send(s, req, strlen(req), 0) < 0) { close(s); return -1; }
  for (;;) {
    ssize_t k;
    if (n + 65536 > cap) { cap = cap ? cap * 2 : 1 << 20; b = realloc(b, cap); }
    k = recv(s, b + n, cap - n, 0);
    if (k <= 0) break;
    n += (size_t)k;
  }
  close(s);
  if (!b) return -1;
  {
    unsigned char *fim = NULL;
    for (size_t i = 0; i + 3 < n; i++) if (!memcmp(b + i, "\r\n\r\n", 4)) { fim = b + i; break; }
    if (!fim) { free(b); return -1; }
    size_t h = (size_t)(fim - b);
    snprintf(r->cab, sizeof r->cab, "%.*s", (int)(h < sizeof r->cab - 1 ? h : sizeof r->cab - 1), (char *)b);
    sscanf(r->cab, "HTTP/1.%*d %d", &r->status);
    unsigned char *c = fim + 4;
    size_t cn = n - (size_t)(c - b);
    if (strstr(r->cab, "Transfer-Encoding: chunked")) {
      unsigned char *o = malloc(cn + 1);
      size_t on = 0, i = 0;
      while (o && i < cn) {
        unsigned long t = strtoul((char *)c + i, NULL, 16);
        while (i + 1 < cn && !(c[i] == '\r' && c[i + 1] == '\n')) i++;
        i += 2;
        if (!t || i + t > cn) break;
        memcpy(o + on, c + i, t); on += t; i += t + 2;
      }
      r->corpo = o; r->n = on;
    } else {
      r->corpo = malloc(cn + 1);
      if (r->corpo) memcpy(r->corpo, c, cn);
      r->n = cn;
    }
    if (r->corpo) r->corpo[r->n] = 0;
  }
  free(b);
  return 0;
}

static int salvar(const char *dir, const char *nome, const Resp *r) {
  char c[700];
  FILE *f;
  snprintf(c, sizeof c, "%s/%s", dir, nome);
  if (!(f = fopen(c, "wb"))) return 0;
  fwrite(r->corpo, 1, r->n, f);
  return fclose(f) == 0;
}

// "http://127.0.0.1:P/tok/m.m3u8" -> port and "/tok/" prefix.
static int partes(const char *url, int *porta, char *pref, size_t n) {
  const char *p = strstr(url, "127.0.0.1:"), *b;
  if (!p) return 0;
  *porta = atoi(p + 10);
  b = strchr(p, '/');
  if (!b) return 0;
  const char *z = strchr(b + 1, '/');
  if (!z) return 0;
  snprintf(pref, n, "%.*s", (int)(z - b + 1), b);
  return 1;
}

static int abrirHls(int origem, const char *arq, double inicio, int *porta, char *pref, size_t n) {
  char fonte[200], url[300];
  snprintf(fonte, sizeof fonte, "http://127.0.0.1:%d/%s", origem, arq);
  if (!dts_tpk_abrir(fonte, "", -1, 0, DTSTPK_HLS, inicio, url, sizeof url)) return 0;
  return partes(url, porta, pref, n);
}

// The fixed delivery (picked on the S95C): 2 s segments in the playlist and on
// the wire, no start tag, and the 1 s prebuffer the player gets with the URL.
static void testeHls(int origem, const char *dir) {
  int porta;
  char pref[64], c[128];
  Resp r;
  CONFERE(abrirHls(origem, "fx.mkv", 12.0, &porta, pref, sizeof pref), "session opens at 12 s");
  CONFERE(dts_tpk_prebuffer_ms() == 1000, "prebuffer for the player: %d ms", dts_tpk_prebuffer_ms());
  snprintf(c, sizeof c, "%sm.m3u8", pref);
  CONFERE(!pedir(porta, c, NULL, &r) && r.status == 200 && strstr((char *)r.corpo, "v.m3u8") &&
          !strstr((char *)r.corpo, "EXT-X-START"), "master playlist 200, no start tag");
  free(r.corpo);
  snprintf(c, sizeof c, "%sv.m3u8", pref);
  CONFERE(!pedir(porta, c, NULL, &r) && r.status == 200 && strstr((char *)r.corpo, "#EXT-X-TARGETDURATION:2\n") &&
          strstr((char *)r.corpo, "seg14.ts") && !strstr((char *)r.corpo, "seg15.ts") && !strstr((char *)r.corpo, "EXT-X-START"),
          "media playlist: target 2 s, 15 segments for 30 s, no start tag");
  free(r.corpo);
  snprintf(c, sizeof c, "%sseg0.ts", pref);
  CONFERE(!pedir(porta, c, NULL, &r) && r.status == 200 && r.n > 1000 && salvar(dir, "seg0.ts", &r), "seg0 200 (%zu B)", r.n);
  free(r.corpo);
  snprintf(c, sizeof c, "%sseg1.ts", pref);
  CONFERE(!pedir(porta, c, NULL, &r) && r.status == 200 && r.n > 1000 && salvar(dir, "seg1.ts", &r), "seg1 200 (%zu B)", r.n);
  free(r.corpo);
  dts_tpk_fechar();
  sleep(1);   // the session is destroyed on its own thread
}

static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

// Stop/back while the source is still being probed (review blocker 1): the
// origin holds the request without answering; closing the session must hang
// up on it right away. The shell compares "closed-at" with the origin's log.
static void testeCancelar(int origem) {
  int porta;
  char pref[64];
  CONFERE(abrirHls(origem, "hold/fx.mkv", 0, &porta, pref, sizeof pref), "session opens on a source that never answers");
  sleep(2);
  CONFERE(dts_tpk_estado() == DTSTPK_SONDANDO, "still probing after 2 s");
  printf("closed-at %.3f\n", agora());
  fflush(stdout);
  dts_tpk_fechar();
  sleep(4);
}

// Rewrites `in` so each audio packet sits in the file next to the video of
// `lead` seconds EARLIER: the audio runs that far ahead of its video (a badly
// interleaved file). Timestamps are untouched; only the order changes.
typedef struct { AVPacket *p; double k; } Ord;
static int cmpOrd(const void *a, const void *b) {
  double x = ((const Ord *)a)->k, y = ((const Ord *)b)->k;
  return x < y ? -1 : x > y;
}
// The .tpk FFmpeg has no protocols at all (no "file"): plain stdio here.
static int fLer(void *f, uint8_t *b, int n) { size_t k = fread(b, 1, (size_t)n, f); return k ? (int)k : AVERROR_EOF; }
static int fEscrever(void *f, const uint8_t *b, int n) { return fwrite(b, 1, (size_t)n, f) == (size_t)n ? n : -1; }
static int64_t fBuscar(void *f, int64_t o, int w) {
  if (w == AVSEEK_SIZE) { long a = ftell(f), z; fseek(f, 0, SEEK_END); z = ftell(f); fseek(f, a, SEEK_SET); return z; }
  return fseek(f, (long)o, w & ~AVSEEK_FORCE) ? -1 : ftell(f);
}
static int adiantar(const char *in, const char *out, double lead) {
  AVFormatContext *ic = avformat_alloc_context(), *oc = NULL;
  Ord *v = NULL;
  int n = 0, cap = 0, rr = -1;
  FILE *fi = fopen(in, "rb"), *fo = fopen(out, "w+b");
  av_log_set_level(AV_LOG_ERROR);
  if (!ic || !fi || !fo) return -1;
  ic->pb = avio_alloc_context(av_malloc(65536), 65536, 0, fi, fLer, NULL, fBuscar);
  if (avformat_open_input(&ic, NULL, NULL, NULL) < 0 || avformat_find_stream_info(ic, NULL) < 0) return -1;
  if (avformat_alloc_output_context2(&oc, NULL, "matroska", NULL) < 0) return -1;
  for (unsigned i = 0; i < ic->nb_streams; i++) {
    AVStream *o = avformat_new_stream(oc, NULL);
    avcodec_parameters_copy(o->codecpar, ic->streams[i]->codecpar);
    o->codecpar->codec_tag = 0;
    o->time_base = ic->streams[i]->time_base;
  }
  for (;;) {
    AVPacket *p = av_packet_alloc();
    if (av_read_frame(ic, p) < 0) { av_packet_free(&p); break; }
    if (n == cap) { cap = cap ? cap * 2 : 4096; v = realloc(v, (size_t)cap * sizeof *v); }
    int64_t ts = p->dts != AV_NOPTS_VALUE ? p->dts : p->pts;
    double t = ts != AV_NOPTS_VALUE ? ts * av_q2d(ic->streams[p->stream_index]->time_base) : 0;
    if (ic->streams[p->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) t -= lead;
    v[n].p = p; v[n].k = t; n++;
  }
  qsort(v, (size_t)n, sizeof *v, cmpOrd);
  oc->pb = avio_alloc_context(av_malloc(65536), 65536, 1, fo, NULL, fEscrever, fBuscar);
  if (oc->pb && avformat_write_header(oc, NULL) >= 0) {
    rr = 0;
    for (int i = 0; i < n; i++) {
      av_packet_rescale_ts(v[i].p, ic->streams[v[i].p->stream_index]->time_base, oc->streams[v[i].p->stream_index]->time_base);
      if (av_write_frame(oc, v[i].p) < 0) rr = -1;
    }
    av_write_trailer(oc);
    avio_flush(oc->pb);
  }
  fclose(fo); fclose(fi);
  for (int i = 0; i < n; i++) av_packet_free(&v[i].p);
  free(v);
  avformat_free_context(oc);
  avformat_close_input(&ic);
  return rr;
}

// Audio 70 s ahead of the video (review blocker 2): the converter keeps the
// AC3 of later segments queued until their video comes. The queue has a cap:
// past it the segment fails (500) instead of growing until out of memory.
static void testeFila(int origem) {
  int porta, erro = 0, k;
  char pref[64], c[128];
  Resp r;
  CONFERE(abrirHls(origem, "adiantado.mkv", 0, &porta, pref, sizeof pref), "session on the audio-ahead file");
  for (k = 0; k < 40 && !erro; k++) {
    snprintf(c, sizeof c, "%sseg%d.ts", pref, k);
    if (pedir(porta, c, NULL, &r) < 0) { erro = -1; break; }
    if (r.status == 404) { free(r.corpo); break; }   // past the last segment
    if (r.status != 200) erro = r.status;
    free(r.corpo);
  }
  printf("fila: stopped at seg%d with %d\n", k, erro);
  CONFERE(erro == 500, "a segment fails with 500 once the audio queue passes its cap (got %d at seg%d)", erro, k);
  dts_tpk_fechar();
  sleep(1);
}

static int fios(void) {   // threads of this process
  int n = 0;
  DIR *d = opendir("/proc/self/task");
  struct dirent *e;
  while (d && (e = readdir(d))) if (e->d_name[0] != '.') n++;
  if (d) closedir(d);
  return n;
}
static int conectarEm(const char *ip, int porta) {
  struct sockaddr_in a;
  int s = socket(AF_INET, SOCK_STREAM, 0);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_port = htons((unsigned short)porta);
  inet_pton(AF_INET, ip, &a.sin_addr);
  if (s < 0 || connect(s, (struct sockaddr *)&a, sizeof a) < 0) { if (s >= 0) close(s); return -1; }
  return s;
}
static int ipNaoLoopback(char *ip, size_t n) {
  struct ifaddrs *l = NULL, *i;
  int ok = 0;
  if (getifaddrs(&l)) return 0;
  for (i = l; i && !ok; i = i->ifa_next)
    if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && (i->ifa_flags & IFF_UP) && !(i->ifa_flags & IFF_LOOPBACK))
      ok = inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, ip, (socklen_t)n) != NULL;
  freeifaddrs(l);
  return ok;
}
// Connections that never send a request, and requests with a wrong token: no
// thread for either (the porteiro holds or answers them); the live token still
// gets its answer meanwhile.
static void semFios(const char *nome, const char *ip, int porta, const char *bom, const char *ruim) {
  int ociosas[12], f0, f1, f2, i;
  char req[256];
  Resp r;
  f0 = fios();
  for (i = 0; i < 12; i++) ociosas[i] = conectarEm(ip, porta);
  usleep(400 * 1000);
  f1 = fios();
  CONFERE(f1 <= f0, "%s: 12 idle connections start no thread (%d -> %d)", nome, f0, f1);
  for (i = 0; i < 12; i++) {
    int s = conectarEm(ip, porta), st = 0;
    char b[256] = "";
    snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: x\r\n\r\n", ruim);
    if (s >= 0 && send(s, req, strlen(req), 0) > 0 && recv(s, b, sizeof b - 1, 0) > 0) sscanf(b, "HTTP/1.%*d %d", &st);
    if (s >= 0) close(s);
    if (st != 404) { CONFERE(0, "%s: wrong token answered %d", nome, st); break; }
  }
  f2 = fios();
  CONFERE(f2 <= f0, "%s: 12 wrong-token requests get 404 and start no thread (%d -> %d)", nome, f0, f2);
  { int s = conectarEm(ip, porta), st = 0;
    char b[512] = "";
    snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", bom);
    if (s >= 0 && send(s, req, strlen(req), 0) > 0 && recv(s, b, sizeof b - 1, 0) > 0) sscanf(b, "HTTP/1.%*d %d", &st);
    if (s >= 0) close(s);
    CONFERE(st == 200, "%s: the live token is served meanwhile (%d)", nome, st); }
  for (i = 0; i < 12; i++) if (ociosas[i] >= 0) close(ociosas[i]);
  (void)r;
}

// Review blocker 3: the HLS server listens on 127.0.0.1 only and checks the
// token before starting a thread.
static void testeEscuta(int origem) {
  int porta, s;
  char pref[64], ip[64], c[128], bom[128];
  Resp r;
  CONFERE(abrirHls(origem, "fx.mkv", 0, &porta, pref, sizeof pref), "session opens");
  snprintf(c, sizeof c, "%sv.m3u8", pref);
  CONFERE(!pedir(porta, c, NULL, &r) && r.status == 200, "playlist on 127.0.0.1");
  free(r.corpo);
  if (ipNaoLoopback(ip, sizeof ip)) {
    s = conectarEm(ip, porta);
    CONFERE(s < 0, "the HLS port is closed on this machine's network address %s", ip);
    if (s >= 0) close(s);
  } else printf("skip no network address here\n");
  snprintf(bom, sizeof bom, "%sv.m3u8", pref);
  semFios("hls", "127.0.0.1", porta, bom, "/abcdefgh/v.m3u8");
  dts_tpk_fechar();
  sleep(1);
}

// Review blocker 4 (an inverted Range used to read past a segment in memory):
// the body is always chunked now and a Range is ignored: the whole segment.
static void testeFaixa(int origem) {
  char pref[64], c[128];
  int porta;
  Resp r;
  size_t total;
  CONFERE(abrirHls(origem, "fx.mkv", 0, &porta, pref, sizeof pref), "session opens");
  snprintf(c, sizeof c, "%sseg1.ts", pref);
  CONFERE(!pedir(porta, c, NULL, &r) && r.status == 200 && r.n > 1000, "seg1 whole: 200 (%zu B)", r.n);
  total = r.n; free(r.corpo);
  CONFERE(!pedir(porta, c, "Range: bytes=8-0\r\n", &r) && r.status == 200 && r.n == total &&
          strstr(r.cab, "Transfer-Encoding: chunked"), "bytes=8-0: 200, the whole segment, chunked (got %d, %zu B)", r.status, r.n);
  free(r.corpo);
  CONFERE(!pedir(porta, c, "Range: bytes=10-19\r\n", &r) && r.status == 200 && r.n == total, "bytes=10-19: 200 whole (got %d, %zu B)", r.status, r.n);
  free(r.corpo);
  dts_tpk_fechar();
  sleep(1);
}

int main(int argc, char **argv) {
  if (argc == 5 && !strcmp(argv[1], "adiantar")) return adiantar(argv[2], argv[3], atof(argv[4])) ? 1 : 0;
  if (argc < 4) { fprintf(stderr, "usage: %s <origin port> <test> <dir>\n", argv[0]); return 2; }
  int origem = atoi(argv[1]);
  const char *t = argv[2];
  CONFERE(dts_tpk_disponivel(), "converter available in this build");
  if (!strcmp(t, "padrao")) testeHls(origem, argv[3]);
  else if (!strcmp(t, "cancelar")) testeCancelar(origem);
  else if (!strcmp(t, "fila")) testeFila(origem);
  else if (!strcmp(t, "escuta")) testeEscuta(origem);
  else if (!strcmp(t, "faixa")) testeFaixa(origem);
  else { fprintf(stderr, "unknown test %s\n", t); return 2; }
  printf("%s: %d failure(s)\n", t, falhas);
  return falhas ? 1 : 0;
}
