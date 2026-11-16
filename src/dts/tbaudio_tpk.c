// Original audio with TorBox burn-in: see tbaudio_tpk.h. Owner 13/11/2026.
#include "tbaudio_tpk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>

// ---- pure: playlists, MPEG-TS and fMP4 timestamps ---------------------------

int tba_url_absoluta(const char *base, const char *rel, char *out, size_t n) {
  const char *h, *fimOrigem, *q;
  size_t no, nd;
  if (!out || n < 2) return 0;
  out[0] = 0;
  if (!rel) return 0;
  while (*rel == ' ' || *rel == '\t') rel++;
  if (!strncasecmp(rel, "http://", 7) || !strncasecmp(rel, "https://", 8)) {
    snprintf(out, n, "%s", rel);
    return strlen(rel) < n;
  }
  if (!base || !(h = strstr(base, "://"))) return 0;
  if (rel[0] == '/' && rel[1] == '/') {   // same scheme
    return (size_t)snprintf(out, n, "%.*s:%s", (int)(h - base), base, rel) < n;
  }
  fimOrigem = h + 3 + strcspn(h + 3, "/?#");
  no = (size_t)(fimOrigem - base);
  if (rel[0] == '/') return (size_t)snprintf(out, n, "%.*s%s", (int)no, base, rel) < n;
  // Directory of the base path (query and fragment dropped).
  q = fimOrigem + strcspn(fimOrigem, "?#");
  nd = (size_t)(q - base);
  while (nd > no && base[nd - 1] != '/') nd--;
  if (nd <= no) {   // "https://host" or "https://host?x": the root
    if ((size_t)snprintf(out, n, "%.*s/", (int)no, base) >= n) return 0;
  } else if ((size_t)snprintf(out, n, "%.*s", (int)nd, base) >= n) return 0;
  while (rel[0] == '.' && (rel[1] == '/' || (rel[1] == '.' && rel[2] == '/'))) {
    if (rel[1] == '/') { rel += 2; continue; }
    rel += 3;   // "../": drop the last directory, never past the origin
    { size_t k = strlen(out);
      if (k > no + 1) { k--; while (k > no && out[k - 1] != '/') k--; out[k] = 0; } }
  }
  if (strlen(out) + strlen(rel) + 1 > n) return 0;
  strcat(out, rel);
  return 1;
}

static char *dup0(const char *s, size_t k) {
  char *d = malloc(k + 1);
  if (d) { memcpy(d, s, k); d[k] = 0; }
  return d;
}
// Value of ATTR=... in an attribute list (quoted or not).
static int atributo(const char *linha, const char *nome, char *out, size_t n) {
  size_t k = strlen(nome);
  const char *p = linha;
  out[0] = 0;
  while ((p = strstr(p, nome)) != NULL) {
    if ((p == linha || p[-1] == ':' || p[-1] == ',') && p[k] == '=') {
      const char *v = p + k + 1, *f;
      if (*v == '"') { v++; f = strchr(v, '"'); if (!f) f = v + strlen(v); }
      else f = v + strcspn(v, ",\r\n");
      snprintf(out, n, "%.*s", (int)(f - v), v);
      return 1;
    }
    p += k;
  }
  return 0;
}

void tba_pl_liberar(TbaPl *pl) {
  int i;
  if (!pl) return;
  for (i = 0; i < pl->nseg; i++) free(pl->seg[i].uri);
  free(pl->seg);
  memset(pl, 0, sizeof *pl);
}

int tba_pl_ler(const char *texto, const char *base, TbaPl *pl) {
  const char *p = texto;
  int cap = 0, esperaUri = 0, esperaVar = 0;
  double dur = 0;
  long bandaVar = 0;
  char resVar[24] = "";
  if (!pl) return 0;
  memset(pl, 0, sizeof *pl);
  if (!texto || strncmp(texto, "#EXTM3U", 7)) return 0;
  while (*p) {
    size_t k = strcspn(p, "\r\n");
    char linha[4096];
    snprintf(linha, sizeof linha, "%.*s", (int)(k < sizeof linha - 1 ? k : sizeof linha - 1), p);
    p += k; while (*p == '\r' || *p == '\n') p++;
    if (!linha[0]) continue;
    if (linha[0] == '#') {
      if (!strncmp(linha, "#EXTINF:", 8)) { dur = strtod(linha + 8, NULL); esperaUri = 1; }
      else if (!strncmp(linha, "#EXT-X-STREAM-INF:", 18)) {
        char v[32];
        bandaVar = atributo(linha + 18, "BANDWIDTH", v, sizeof v) ? atol(v) : 0;
        if (!atributo(linha + 18, "RESOLUTION", resVar, sizeof resVar)) resVar[0] = 0;
        esperaVar = 1; pl->master = 1;
      } else if (!strncmp(linha, "#EXT-X-MAP:", 11)) {
        char u[4096];
        if (atributo(linha + 11, "URI", u, sizeof u)) tba_url_absoluta(base, u, pl->mapa, sizeof pl->mapa);
      } else if (!strncmp(linha, "#EXT-X-ENDLIST", 14)) pl->fim = 1;
      else if (!strncmp(linha, "#EXT-X-TARGETDURATION:", 22)) pl->alvo = strtod(linha + 22, NULL);
      else if (!strncmp(linha, "#EXT-X-VERSION:", 15)) pl->versao = atoi(linha + 15);
      continue;
    }
    if (esperaVar) {
      // The best variant: the highest BANDWIDTH (TorBox lists one).
      if (!pl->variante[0] || bandaVar > pl->banda) {
        tba_url_absoluta(base, linha, pl->variante, sizeof pl->variante);
        pl->banda = bandaVar;
        snprintf(pl->resolucao, sizeof pl->resolucao, "%s", resVar);
      }
      esperaVar = 0;
      continue;
    }
    if (esperaUri) {
      char abs[4096];
      if (pl->nseg == cap) {
        TbaSeg *n = realloc(pl->seg, sizeof *n * (size_t)(cap ? cap * 2 : 256));
        if (!n) { tba_pl_liberar(pl); return 0; }
        pl->seg = n; cap = cap ? cap * 2 : 256;
      }
      if (!tba_url_absoluta(base, linha, abs, sizeof abs)) snprintf(abs, sizeof abs, "%s", linha);
      pl->seg[pl->nseg].uri = dup0(abs, strlen(abs));
      pl->seg[pl->nseg].dur = dur > 0 ? dur : 0;
      if (!pl->seg[pl->nseg].uri) { tba_pl_liberar(pl); return 0; }
      pl->nseg++;
      esperaUri = 0;
    }
  }
  return pl->master ? pl->variante[0] != 0 : pl->nseg > 0;
}

double tba_pl_ultimo(const TbaPl *pl, double durArquivo) {
  double tot = 0, ult, sobra;
  int i;
  if (!pl || pl->nseg < 1) return 0;
  ult = pl->seg[pl->nseg - 1].dur;
  if (durArquivo <= 0 || !pl->fim) return ult;
  for (i = 0; i < pl->nseg; i++) tot += pl->seg[i].dur;
  sobra = tot - durArquivo;
  if (sobra > 0.01 && sobra < ult - 0.05) return ult - sobra;
  return ult;
}

char *tba_pl_espelho(const char *texto, const char *base, double ultimo) {
  const char *p;
  size_t cap, k = 0;
  int nInf = 0, iInf = 0;
  char *o;
  if (!texto) return NULL;
  for (p = texto; (p = strstr(p, "#EXTINF:")) != NULL; p += 8) nInf++;
  cap = strlen(texto) * 2 + 65536;
  o = malloc(cap);
  if (!o) return NULL;
  for (p = texto; *p; ) {
    size_t n = strcspn(p, "\r\n");
    char linha[4096], abs[4096];
    const char *u;
    snprintf(linha, sizeof linha, "%.*s", (int)(n < sizeof linha - 1 ? n : sizeof linha - 1), p);
    p += n; while (*p == '\r' || *p == '\n') p++;
    if (k + 8300 > cap) { char *g = realloc(o, cap * 2); if (!g) { free(o); return NULL; } o = g; cap *= 2; }
    if (!linha[0]) continue;
    if (linha[0] != '#') {
      if (!tba_url_absoluta(base, linha, abs, sizeof abs)) snprintf(abs, sizeof abs, "%s", linha);
      k += (size_t)snprintf(o + k, cap - k, "%s\n", abs);
    } else if (!strncmp(linha, "#EXTINF:", 8)) {
      iInf++;
      if (iInf == nInf && ultimo >= 0) {
        const char *c = strchr(linha, ',');
        k += (size_t)snprintf(o + k, cap - k, "#EXTINF:%.6f,%s\n", ultimo, c ? c + 1 : "");
      } else k += (size_t)snprintf(o + k, cap - k, "%s\n", linha);
    } else if ((u = strstr(linha, "URI=\"")) != NULL) {
      const char *f = strchr(u + 5, '"');
      char rel[4096];
      if (!f) { k += (size_t)snprintf(o + k, cap - k, "%s\n", linha); continue; }
      snprintf(rel, sizeof rel, "%.*s", (int)(f - (u + 5)), u + 5);
      if (!tba_url_absoluta(base, rel, abs, sizeof abs)) snprintf(abs, sizeof abs, "%s", rel);
      k += (size_t)snprintf(o + k, cap - k, "%.*sURI=\"%s\"%s\n", (int)(u - linha), linha, abs, f + 1);
    } else k += (size_t)snprintf(o + k, cap - k, "%s\n", linha);
  }
  o[k] = 0;
  return o;
}

int tba_ts_primeiro_pts(const unsigned char *b, size_t n, double *s) {
  size_t o = 0;
  if (!b || !s) return 0;
  while (o + 188 * 2 <= n && !(b[o] == 0x47 && b[o + 188] == 0x47)) o++;
  for (; o + 188 <= n; o += 188) {
    const unsigned char *t = b + o, *pl;
    int af, pusi;
    size_t ini = 4;
    if (t[0] != 0x47) break;
    pusi = (t[1] & 0x40) != 0;
    af = (t[3] >> 4) & 3;
    if (!pusi || !(af & 1)) continue;
    if (af & 2) ini += 1 + (size_t)t[4];
    if (ini + 14 > 188) continue;
    pl = t + ini;
    if (pl[0] || pl[1] || pl[2] != 1 || pl[3] < 0xE0 || pl[3] > 0xEF) continue;
    if (!(pl[7] & 0x80)) continue;   // no PTS
    {
      unsigned long long v = ((unsigned long long)((pl[9] >> 1) & 7) << 30) | ((unsigned long long)pl[10] << 22) |
                             ((unsigned long long)(pl[11] >> 1) << 15) | ((unsigned long long)pl[12] << 7) |
                             (unsigned long long)(pl[13] >> 1);
      *s = (double)v / 90000.0;
      return 1;
    }
  }
  return 0;
}

static unsigned long long be(const unsigned char *p, int k) {
  unsigned long long v = 0;
  int i;
  for (i = 0; i < k; i++) v = (v << 8) | p[i];
  return v;
}
// Next box in [o, fim): its type and payload. 0 at the end or on junk.
static int caixa(const unsigned char *b, size_t fim, size_t *o, char tipo[5], size_t *ini, size_t *tam) {
  unsigned long long t;
  size_t h = 8;
  if (*o + 8 > fim) return 0;
  t = be(b + *o, 4);
  memcpy(tipo, b + *o + 4, 4); tipo[4] = 0;
  if (t == 1) { if (*o + 16 > fim) return 0; t = be(b + *o + 8, 8); h = 16; }
  else if (t == 0) t = fim - *o;
  if (t < h || *o + t > fim) return 0;
  *ini = *o + h; *tam = (size_t)t - h;
  *o += (size_t)t;
  return 1;
}
static int achar(const unsigned char *b, size_t ini, size_t tam, const char *quer, size_t *pi, size_t *pt) {
  size_t o = ini;
  char tp[5];
  size_t ci, ct;
  while (caixa(b, ini + tam, &o, tp, &ci, &ct))
    if (!strcmp(tp, quer)) { *pi = ci; *pt = ct; return 1; }
  return 0;
}
int tba_mp4_escala(const unsigned char *b, size_t n, unsigned *escala) {
  size_t mi, mt, o, ti, tt;
  char tp[5];
  if (!b || !escala || !achar(b, 0, n, "moov", &mi, &mt)) return 0;
  o = mi;
  while (caixa(b, mi + mt, &o, tp, &ti, &tt)) {
    size_t di, dt, hi, ht, ki, kt;
    if (strcmp(tp, "trak") || !achar(b, ti, tt, "mdia", &di, &dt)) continue;
    if (!achar(b, di, dt, "hdlr", &hi, &ht) || ht < 12 || memcmp(b + hi + 8, "vide", 4)) continue;
    if (!achar(b, di, dt, "mdhd", &ki, &kt) || kt < 24) continue;
    *escala = (unsigned)(b[ki] == 1 ? be(b + ki + 20, 4) : be(b + ki + 12, 4));
    return *escala > 0;
  }
  return 0;
}
int tba_mp4_tfdt(const unsigned char *b, size_t n, unsigned long long *t) {
  size_t mi, mt, fi, ft, di, dt;
  if (!b || !t || !achar(b, 0, n, "moof", &mi, &mt) || !achar(b, mi, mt, "traf", &fi, &ft) ||
      !achar(b, fi, ft, "tfdt", &di, &dt) || dt < 8) return 0;
  *t = b[di] == 1 ? (dt >= 12 ? be(b + di + 4, 8) : 0) : be(b + di + 4, 4);
  return 1;
}

// MPEG-TS without its audio (Passthrough): the audio elementary streams
// leave the PMT and their packets are dropped; nothing is decoded.
static unsigned crc32mpeg(const unsigned char *b, size_t n) {
  unsigned c = 0xFFFFFFFFu;
  size_t i;
  int k;
  for (i = 0; i < n; i++) {
    c ^= (unsigned)b[i] << 24;
    for (k = 0; k < 8; k++) c = c & 0x80000000u ? (c << 1) ^ 0x04C11DB7u : c << 1;
  }
  return c;
}
static int esAudio(int tipo, const unsigned char *d, int n) {
  int i;
  switch (tipo) {
    case 0x03: case 0x04: case 0x0F: case 0x11: case 0x81: case 0x82: case 0x83: case 0x84:
    case 0x85: case 0x86: case 0x87: case 0xA1: case 0xA2: return 1;
    case 0x06:   // private PES: audio only by its descriptor
      for (i = 0; i + 2 <= n; i += 2 + d[i + 1]) {
        int t = d[i], l = d[i + 1];
        if (i + 2 + l > n) break;
        if (t == 0x6A || t == 0x7A || t == 0x7B || t == 0x7C) return 1;   // AC-3, E-AC-3, DTS, AAC
        if (t == 0x05 && l >= 4 && (!memcmp(d + i + 2, "AC-3", 4) || !memcmp(d + i + 2, "EAC3", 4) ||
                                    !memcmp(d + i + 2, "Opus", 4) || !memcmp(d + i + 2, "DTS", 3))) return 1;
        if (t == 0x7F && l >= 1 && (d[i + 2] == 0x80 || d[i + 2] == 0x15)) return 1;    // Opus, AC-4
      }
      return 0;
    default: return 0;
  }
}
static size_t tsInicio(const unsigned char *t) {   // payload offset of a TS packet, 0 = none
  int af = (t[3] >> 4) & 3;
  size_t o = 4;
  if (!(af & 1)) return 0;
  if (af & 2) o += 1 + (size_t)t[4];
  return o < 188 ? o : 0;
}
int tba_ts_sem_audio(const unsigned char *in, size_t n, unsigned char *out, size_t *nout, int *removidas) {
  size_t o = 0, k = 0;
  int pmt = -1, pcr = -1, nA = 0, i;
  int audio[16];
  unsigned char sec[1024];
  size_t nsec = 0;
  if (removidas) *removidas = 0;
  if (!in || !out || !nout) return 0;
  while (o + 188 * 2 <= n && !(in[o] == 0x47 && in[o + 188] == 0x47)) o++;
  // PAT -> PMT PID
  for (size_t q = o; q + 188 <= n && pmt < 0; q += 188) {
    const unsigned char *t = in + q;
    size_t pi = tsInicio(t);
    if (t[0] != 0x47 || ((t[1] & 0x1F) << 8 | t[2]) != 0 || !(t[1] & 0x40) || !pi) continue;
    pi += 1 + t[pi];
    if (pi + 12 > 188 || t[pi] != 0x00) continue;
    { int len = (t[pi + 1] & 0x0F) << 8 | t[pi + 2];
      size_t e = pi + 3 + (size_t)len - 4, j;
      for (j = pi + 8; j + 4 <= e && e <= 188; j += 4) {
        int prog = t[j] << 8 | t[j + 1];
        if (prog) { pmt = (t[j + 2] & 0x1F) << 8 | t[j + 3]; break; }
      } }
  }
  if (pmt < 0) return 0;
  // PMT: drop the audio streams, rebuild the section
  for (size_t q = o; q + 188 <= n && !nsec; q += 188) {
    const unsigned char *t = in + q;
    size_t pi = tsInicio(t), e, j;
    int len, pil;
    if (t[0] != 0x47 || ((t[1] & 0x1F) << 8 | t[2]) != pmt || !(t[1] & 0x40) || !pi) continue;
    pi += 1 + t[pi];
    if (pi + 12 > 188 || t[pi] != 0x02) return 0;
    len = (t[pi + 1] & 0x0F) << 8 | t[pi + 2];
    if (pi + 3 + (size_t)len > 188) return 0;   // a PMT spanning packets: not handled
    pcr = (t[pi + 8] & 0x1F) << 8 | t[pi + 9];
    pil = (t[pi + 10] & 0x0F) << 8 | t[pi + 11];
    if (12 + (size_t)pil + 4 > 3 + (size_t)len) return 0;   // program info past the section (and past `sec`)
    memcpy(sec, t + pi, 12 + (size_t)pil); nsec = 12 + (size_t)pil;
    e = pi + 3 + (size_t)len - 4;
    for (j = pi + 12 + (size_t)pil; j + 5 <= e; ) {
      int tipo = t[j], pid = (t[j + 1] & 0x1F) << 8 | t[j + 2], el = (t[j + 3] & 0x0F) << 8 | t[j + 4];
      if (j + 5 + (size_t)el > e) return 0;
      if (esAudio(tipo, t + j + 5, el)) { if (nA < 16) audio[nA++] = pid; }
      else { memcpy(sec + nsec, t + j, 5 + (size_t)el); nsec += 5 + (size_t)el; }
      j += 5 + (size_t)el;
    }
  }
  if (!nsec) return 0;
  for (i = 0; i < nA; i++) if (audio[i] == pcr) return 0;   // the clock rides on audio: cannot drop it
  { int nl = (int)nsec + 4 - 3;
    unsigned c;
    sec[1] = (unsigned char)((sec[1] & 0xF0) | ((nl >> 8) & 0x0F)); sec[2] = (unsigned char)(nl & 0xFF);
    c = crc32mpeg(sec, nsec);
    sec[nsec] = (unsigned char)(c >> 24); sec[nsec + 1] = (unsigned char)(c >> 16);
    sec[nsec + 2] = (unsigned char)(c >> 8); sec[nsec + 3] = (unsigned char)c; nsec += 4; }
  for (; o + 188 <= n; o += 188) {
    const unsigned char *t = in + o;
    int pid = (t[1] & 0x1F) << 8 | t[2], drop = 0;
    if (t[0] != 0x47) break;
    for (i = 0; i < nA; i++) if (audio[i] == pid) drop = 1;
    if (drop) continue;
    memcpy(out + k, t, 188);
    if (pid == pmt && (t[1] & 0x40)) {
      size_t pi = tsInicio(t);
      if (pi && pi + 1 + t[pi] + nsec <= 188) {
        size_t s0 = pi + 1 + t[pi];
        memcpy(out + k + s0, sec, nsec);
        memset(out + k + s0 + nsec, 0xFF, 188 - s0 - nsec);
      }
    }
    k += 188;
  }
  *nout = k;
  if (removidas) *removidas = nA;
  return 1;
}

// ---- S2: sockets to TorBox in /proc/net/tcp (logging only) -----------------
#if (defined(NV_TPK) || defined(NV_TBAUDIO_TEST)) && !defined(__EMSCRIPTEN__)
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define SK_IPS 24
static pthread_mutex_t skTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned skIp[SK_IPS];
static int skIpUrl[SK_IPS];          // which watched URL (0-3) the address came from
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
  printf("[tbaudio] sockets: watching %d address(es) of TorBox's hosts\n", n);
  fflush(stdout);
  return NULL;
}
void tba_sockets_vigiar(const char *u1, const char *u2, const char *u3, const char *u4) {
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
int tba_sockets_contar(int *nuvio) { return tba_sockets_contar_por(nuvio, NULL); }
int tba_sockets_contar_por(int *nuvio, int por[4]) {
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
#else
void tba_sockets_vigiar(const char *u1, const char *u2, const char *u3, const char *u4) { (void)u1; (void)u2; (void)u3; (void)u4; }
int tba_sockets_contar(int *nuvio) { if (nuvio) *nuvio = -1; return -1; }
int tba_sockets_contar_por(int *nuvio, int por[4]) { if (nuvio) *nuvio = -1; if (por) memset(por, 0, 4 * sizeof *por); return -1; }
#endif

// ---- the server (Samsung .tpk with FFmpeg, and the host test) ---------------
#if !defined(NV_DTS_FFMPEG) || !(defined(NV_TPK) || defined(NV_TBAUDIO_TEST))
int  tba_disponivel(void) { return 0; }
int  tba_abrir(const char *t, const char *o, const char *c, int a, int m, char *out, size_t n) {
  (void)t; (void)o; (void)c; (void)a; (void)m; if (out && n) out[0] = 0; return 0;
}
void tba_fechar(void) {}
int  tba_ativo(void) { return 0; }
int  tba_modo(void) { return -1; }
int  tba_falhou(char *m, size_t n) { if (m && n) m[0] = 0; return 0; }
int  tba_pronto(void) { return 0; }
int  tba_n_rendicoes(void) { return 0; }
int  tba_rendicao_de(int o) { (void)o; return -1; }
void tba_definir_audio(int o) { (void)o; }
int  tba_audio(void) { return -1; }
void tba_log(void) {}
#else

#include "dts_engine.h"
#include "../rede.h"
#include "../tbrelogio.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
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
#define TBA_CONEXOES   24            // the player may open every rendition at once
#define ESPERA_PREP_MS 30000
#define TB_CABECA      (256 * 1024)      // head of TorBox segment 0: its first PTS
#define TB_SEG_MAX     (30u * 1024u * 1024u)
#define FILA_MAX       512
#define AUD_SEG_S      2.0               // audio segments: a seek reads ~2 s, not 5 (like DTS)

enum { M_TS = 0, M_MP4, M_AC3, M_AAC, M_FORA };
static const char *modoNome(int m) { return m == TBA_PASSTHROUGH ? "Passthrough" : "TorBox audio"; }
static const char *const MODO_NOME[] = { "copied (MPEG-TS)", "copied (fMP4)", "to AC3", "to AAC", "not offered" };

typedef struct {
  int si, ord, canais, taxa, modo;
  enum AVCodecID id;
  char lingua[16], nome[96], codec[24];
} Faixa;

typedef struct { AVPacket *p; double t; } Item;   // t: source time (s) of the packet

typedef struct Sessao {
  atomic_int refs;
  volatile int cancel;
  pthread_mutex_t m;                   // state below
  pthread_cond_t c;
  int pronto, falhou, modo, audio;   // audio: index in fx[]
  int opusLog;                         // Opus packet lines logged (first segments only)
  int64_t proxPts;                     // the reading track's next timestamp (laced frames carry none)
  int semPts, semPtsLog;               // packets that came without a timestamp (filled in) / lines logged
  unsigned tbrGer;                     // the burned stream this session serves (tbrelogio.h)
  char motivo[160], tok[40];
  char *tbUrl, *orig, *cab;
  // TorBox
  char *plTexto, plBase[4096], *espelho;
  TbaPl pl;
  double *ini, ultimo, dur;            // segment starts on TorBox's timeline, last length, total
  double *aIni; int naSeg;             // audio segments (AUD_SEG_S) on the same timeline
  int tbMp4;
  unsigned char *tbInit; size_t tbInitN;
  double tbOff; int tbOffOk;
  long banda; char resolucao[24];
  // original file
  DtsEngine *eng;
  AVFormatContext *ic;
  double origV0, durArq;
  int sBusca;                          // the stream seeks go through (the one the index covers)
  Faixa fx[TBA_FAIXAS]; int nfx;
  // generator (gm); TorBox video downloads (vm)
  pthread_mutex_t gm, vm;
  int gOk, gFaixa, gSeg, gModo;
  const double *gIni;                  // the segment list gSeg counts in
  atomic_int espera;                   // requests waiting for gm
  atomic_long espChave;                // the last one: (track+1) * 1000000 + segment
  int stripAviso;
  Item fila[FILA_MAX]; int fIni, fN;
  AVPacket *pk;
  AVCodecContext *dec, *enc;
  SwrContext *swr;
  AVAudioFifo *fifo;
  AVFrame *fr, *ef;
  int64_t base48, amostras;
  int eof, leu;   // leu: the demuxer has read packets (a seek is needed for a fresh start)
  // log
  long nSeg, nErro, nLarg, nVid; long long bytes; double msLer, msVid;
  char ultErro[160];
} Sessao;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Sessao *atual;
static int ouvinte = -1, porta;
static atomic_int vivos;
static int hostAudioN = -1;

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
static void soltar(Sessao *s) {
  if (!s || atomic_fetch_sub(&s->refs, 1) != 1) return;
  filaLimpar(s); codecLiberar(s);
  av_packet_free(&s->pk); av_frame_free(&s->fr); av_frame_free(&s->ef);
  if (s->eng) { dts_engine_cancel(s->eng); dts_engine_destroy(s->eng); }
  tba_pl_liberar(&s->pl);
  free(s->ini); free(s->aIni); free(s->plTexto); free(s->espelho); free(s->tbInit);
  free(s->tbUrl); free(s->orig); free(s->cab);
  pthread_mutex_destroy(&s->m); pthread_cond_destroy(&s->c); pthread_mutex_destroy(&s->gm); pthread_mutex_destroy(&s->vm);
  free(s);
}
static void falhar(Sessao *s, const char *porque) {
  pthread_mutex_lock(&s->m);
  if (!s->pronto && !s->falhou) {
    s->falhou = 1;
    snprintf(s->motivo, sizeof s->motivo, "%s", porque);
    printf("[tbaudio] session failed: %s\n", porque);
    fflush(stdout);
  }
  pthread_cond_broadcast(&s->c);
  pthread_mutex_unlock(&s->m);
}

// ---- network (TorBox) ----------------------------------------------------------
typedef struct { unsigned char *b; size_t n, max; } Acum;
static int acumular(const void *d, size_t n, void *u) {
  Acum *a = u;
  size_t k = a->n + n > a->max ? a->max - a->n : n;
  if (k) { memcpy(a->b + a->n, d, k); a->n += k; }
  return a->n < a->max;   // 0 stops the transfer once the cap is reached
}
typedef struct { Sessao *s; volatile int *mais; } Parar;
static int pararSe(void *u) { Parar *p = u; return p->s->cancel || (p->mais && *p->mais); }
// GET into memory. `max` caps the body (a capped body is still returned).
// `final` (may be NULL) gets the final URL. Returns the body or NULL.
static unsigned char *baixarP(Sessao *s, const char *url, size_t max, size_t *n, char *final, size_t fn,
                              int *status, double *ms, volatile int *mais) {
  RedePedido p; RedeResposta r; Acum a;
  Parar pp = { s, mais };
  char faixa[64];
  const char *cabs[2] = { NULL, NULL };
  int cabeca = max <= TB_CABECA;
  double t0 = agoraMs();
  memset(&p, 0, sizeof p); memset(&r, 0, sizeof r);
  a.b = malloc(max + 1); a.n = 0; a.max = max;
  if (!a.b) return NULL;
  p.url = url; p.prazo_ms = 30000; p.seguir = 1; p.rebaixar_midia = 1;
  // rede refuses a body whose Content-Length passes max_bytes before reading
  // it: the cap is ours (acumular), the transfer's is the largest allowed. A
  // short read (segment 0's head) asks for that Range only.
  p.max_bytes = REDE_CORPO_MAXIMO;
  if (cabeca) { snprintf(faixa, sizeof faixa, "Range: bytes=0-%zu", max - 1); cabs[0] = faixa; p.cabecalhos = cabs; }
  p.receber_corpo = acumular; p.corpo_usuario = &a;
  p.parar = pararSe; p.parar_usuario = &pp;
  int st;
  rede_pedir(&p, &r);
  st = r.status;
  if (status) *status = st;
  if (final && fn) snprintf(final, fn, "%s", r.final[0] ? r.final : url);
  rede_resposta_limpar(&r);
  if (ms) *ms = agoraMs() - t0;
  // A capped body ends the transfer on purpose (curl 23): what arrived counts.
  if (s->cancel || (mais && *mais) || st < 200 || st >= 300 || !a.n) { free(a.b); return NULL; }
  a.b[a.n] = 0;
  *n = a.n;
  return a.b;
}
static unsigned char *baixar(Sessao *s, const char *url, size_t max, size_t *n, char *final, size_t fn,
                             int *status, double *ms) {
  return baixarP(s, url, max, n, final, fn, status, ms, NULL);
}

// ---- preparation ---------------------------------------------------------------
static const char *nomeLingua(const char *l) {
  static const struct { const char *c, *n; } T[] = {
    {"eng","English"},{"en","English"},{"jpn","Japanese"},{"ja","Japanese"},{"por","Portuguese"},{"pt","Portuguese"},
    {"spa","Spanish"},{"es","Spanish"},{"fre","French"},{"fra","French"},{"fr","French"},{"ger","German"},{"deu","German"},
    {"de","German"},{"ita","Italian"},{"it","Italian"},{"swe","Swedish"},{"sv","Swedish"},{"kor","Korean"},{"ko","Korean"},
    {"chi","Chinese"},{"zho","Chinese"},{"zh","Chinese"},{"rus","Russian"},{"ru","Russian"},{"hin","Hindi"},{"ara","Arabic"}};
  unsigned i;
  for (i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcasecmp(l, T[i].c)) return T[i].n;
  return l[0] ? l : "Audio";
}
// Opus (owner 16/11): copied into MPEG-TS the TV player never started (two
// Opus files, both stuck before the first frame). It goes as is in fMP4, like
// FLAC; both play that way on the S95C (the AAC/AC3 conversion was never used).
static int modoDe(const Faixa *f) {
  int ok = avcodec_find_decoder(f->id) != NULL;
  switch (f->id) {
    case AV_CODEC_ID_OPUS: case AV_CODEC_ID_FLAC: return M_MP4;
    case AV_CODEC_ID_AAC: case AV_CODEC_ID_AC3: case AV_CODEC_ID_EAC3:
    case AV_CODEC_ID_MP3: case AV_CODEC_ID_MP2: return M_TS;
    case AV_CODEC_ID_DTS: case AV_CODEC_ID_TRUEHD: case AV_CODEC_ID_MLP: return ok ? M_AC3 : M_FORA;
    default: if (!ok) return M_FORA; return f->canais > 2 ? M_AC3 : M_AAC;
  }
}
static void modosRecalcular(Sessao *s) {
  int i;
  for (i = 0; i < s->nfx; i++) s->fx[i].modo = modoDe(&s->fx[i]);
}

static int prepTorbox(Sessao *s) {
  size_t n = 0;
  int st = 0;
  double ms = 0;
  char final[4096];
  unsigned char *b = baixar(s, s->tbUrl, 4u * 1024u * 1024u, &n, final, sizeof final, &st, &ms);
  int i;
  if (!b) { falhar(s, st ? "TorBox's playlist answered with an error" : "TorBox's playlist did not download"); return 0; }
  if (!tba_pl_ler((char *)b, final, &s->pl)) { free(b); falhar(s, "TorBox's playlist is not HLS"); return 0; }
  printf("[tbaudio] TorBox playlist: %s in %.0f ms (%zu bytes)\n", s->pl.master ? "master" : "media", ms, n);
  if (s->pl.master) {
    char var[4096];
    s->banda = s->pl.banda;
    snprintf(s->resolucao, sizeof s->resolucao, "%s", s->pl.resolucao);
    snprintf(var, sizeof var, "%s", s->pl.variante);
    tba_pl_liberar(&s->pl); free(b);
    b = baixar(s, var, 4u * 1024u * 1024u, &n, final, sizeof final, &st, &ms);
    if (!b || !tba_pl_ler((char *)b, final, &s->pl) || s->pl.master || !s->pl.nseg) {
      free(b); falhar(s, "TorBox's media playlist did not load"); return 0;
    }
    printf("[tbaudio] TorBox media playlist in %.0f ms: bandwidth %ld, resolution %s\n", ms, s->banda,
           s->resolucao[0] ? s->resolucao : "?");
  }
  s->plTexto = (char *)b;
  snprintf(s->plBase, sizeof s->plBase, "%s", final);
  s->ini = malloc(sizeof *s->ini * (size_t)(s->pl.nseg + 1));
  if (!s->ini) { falhar(s, "out of memory"); return 0; }
  s->ini[0] = 0;
  for (i = 0; i < s->pl.nseg; i++) s->ini[i + 1] = s->ini[i] + s->pl.seg[i].dur;
  s->tbMp4 = s->pl.mapa[0] != 0;
  // The subtitle clock (tbrelogio.h): the segment lengths the TV sees.
  { double *d = malloc(sizeof *d * (size_t)s->pl.nseg);
    if (d) { for (i = 0; i < s->pl.nseg; i++) d[i] = s->pl.seg[i].dur; tbr_segmentos(s->tbrGer, d, s->pl.nseg); free(d); } }
  printf("[tbaudio] TorBox: %d segments of %.1f s (target %.0f), %s, %s, version %d\n", s->pl.nseg,
         s->pl.nseg ? s->pl.seg[0].dur : 0, s->pl.alvo, s->tbMp4 ? "fMP4 (EXT-X-MAP)" : "MPEG-TS",
         s->pl.fim ? "VOD (ENDLIST)" : "no ENDLIST (still growing?)", s->pl.versao);
  // TorBox's first video PTS: the audio goes onto that timeline.
  if (s->tbMp4) {
    unsigned esc = 0; unsigned long long t = 0;
    s->tbInit = baixar(s, s->pl.mapa, 4u * 1024u * 1024u, &s->tbInitN, NULL, 0, &st, &ms);
    if (s->tbInit && tba_mp4_escala(s->tbInit, s->tbInitN, &esc)) {
      unsigned char *h = baixar(s, s->pl.seg[0].uri, TB_CABECA, &n, NULL, 0, &st, &ms);
      if (h && tba_mp4_tfdt(h, n, &t)) { s->tbOff = (double)t / esc; s->tbOffOk = 1; }
      free(h);
    }
  } else {
    unsigned char *h = baixar(s, s->pl.seg[0].uri, TB_CABECA, &n, NULL, 0, &st, &ms);
    if (h && tba_ts_primeiro_pts(h, n, &s->tbOff)) s->tbOffOk = 1;
    if (!h) printf("[tbaudio] TorBox segment 0 head did not download (HTTP %d)\n", st);
    if (h) printf("[tbaudio] TorBox segment 0 head: %zu bytes in %.0f ms\n", n, ms);
    free(h);
  }
  printf("[tbaudio] TorBox's first video PTS: %s%.3f s\n", s->tbOffOk ? "" : "UNKNOWN, using ", s->tbOffOk ? s->tbOff : 0.0);
  if (s->tbOffOk) tbr_origem(s->tbrGer, s->tbOff);
  // S2: the hosts the TV player and Nuvio talk to (playlist, segments, file).
  tba_sockets_vigiar(s->tbUrl, final, s->pl.seg[0].uri, s->orig);
  fflush(stdout);
  return 1;
}

static int prepOriginal(Sessao *s, int ordPedido) {
  double t0 = agoraMs();
  AVFormatContext *ic;
  int i, v = -1, nA = 0;
  s->eng = dts_engine_create();
  if (!s->eng) { falhar(s, "out of memory"); return 0; }
  if (s->cancel) return 0;
  if (dts_engine_open_demux(s->eng, s->orig, s->cab) < 0) {
    char e[160]; snprintf(e, sizeof e, "original file: %s", dts_engine_error(s->eng)); falhar(s, e); return 0;
  }
  ic = s->ic = dts_engine_avformat(s->eng);
  for (i = 0; i < (int)ic->nb_streams; i++) {
    AVStream *st = ic->streams[i];
    AVCodecParameters *p = st->codecpar;
    if (p->codec_type == AVMEDIA_TYPE_VIDEO && v < 0 && !(st->disposition & AV_DISPOSITION_ATTACHED_PIC)) v = i;
    if (p->codec_type == AVMEDIA_TYPE_AUDIO && s->nfx < TBA_FAIXAS) {
      Faixa *f = &s->fx[s->nfx++];
      AVDictionaryEntry *l = av_dict_get(st->metadata, "language", NULL, 0);
      AVDictionaryEntry *t = av_dict_get(st->metadata, "title", NULL, 0);
      f->si = i; f->ord = nA; f->id = p->codec_id;
      f->canais = p->ch_layout.nb_channels; f->taxa = p->sample_rate;
      snprintf(f->lingua, sizeof f->lingua, "%s", l && strcmp(l->value, "und") ? l->value : "");
      snprintf(f->nome, sizeof f->nome, "%s", t ? t->value : "");
      snprintf(f->codec, sizeof f->codec, "%s", avcodec_get_name(p->codec_id));
    }
    if (p->codec_type == AVMEDIA_TYPE_AUDIO) nA++;
  }
  for (i = 0; i < (int)ic->nb_streams; i++) ic->streams[i]->discard = AVDISCARD_ALL;
  s->durArq = ic->duration > 0 ? ic->duration / (double)AV_TIME_BASE : 0;
  s->origV0 = 0;
  if (v >= 0 && ic->streams[v]->start_time != AV_NOPTS_VALUE)
    s->origV0 = ic->streams[v]->start_time * av_q2d(ic->streams[v]->time_base);
  else if (ic->start_time != AV_NOPTS_VALUE) s->origV0 = ic->start_time / (double)AV_TIME_BASE;
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
    s->sBusca = v >= 0 ? v : 0;
    if (s->durArq > 10)
      r = av_seek_frame(ic, s->sBusca, av_rescale_q((int64_t)(s->durArq * 0.5 * AV_TIME_BASE), AV_TIME_BASE_Q,
                                                    ic->streams[s->sBusca]->time_base), AVSEEK_FLAG_BACKWARD);
    if (v < 0 || avformat_index_get_entries_count(ic->streams[v]) <= 0)
      for (i = 0; i < (int)ic->nb_streams; i++) {
        int n = avformat_index_get_entries_count(ic->streams[i]);
        if (ic->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
        if (n > mais) { mais = n; if (n > 0) s->sBusca = i; }
      }
    s->leu = 1;
    printf("[tbaudio] seek index %s in %.0f ms: seeks go through stream %d (%d entries)\n", r >= 0 ? "read" : "NOT read",
           agoraMs() - t1, s->sBusca, avformat_index_get_entries_count(ic->streams[s->sBusca])); }
  modosRecalcular(s);
  s->audio = -1;
  for (i = 0; i < s->nfx; i++) if (s->fx[i].ord == ordPedido && s->fx[i].modo != M_FORA) s->audio = i;
  for (i = 0; i < s->nfx && s->audio < 0; i++) if (s->fx[i].modo != M_FORA) s->audio = i;
  printf("[tbaudio] original file open in %.0f ms (%s): %.1f s, video starts at %.3f s, %d audio track(s)\n",
         agoraMs() - t0, ic->iformat ? ic->iformat->name : "?", s->durArq, s->origV0, s->nfx);
  for (i = 0; i < s->nfx; i++)
    printf("[tbaudio]   audio %d: %s %dch %d Hz, %s%s%s -> %s%s\n", s->fx[i].ord, s->fx[i].codec, s->fx[i].canais,
           s->fx[i].taxa, s->fx[i].lingua[0] ? s->fx[i].lingua : "und", s->fx[i].nome[0] ? ", " : "", s->fx[i].nome,
           MODO_NOME[s->fx[i].modo], i == s->audio ? " (playing)" : "");
  fflush(stdout);
  if (s->audio < 0) { falhar(s, "no audio track Nuvio can serve"); return 0; }
  s->pk = av_packet_alloc(); s->fr = av_frame_alloc(); s->ef = av_frame_alloc();
  if (!s->pk || !s->fr || !s->ef) { falhar(s, "out of memory"); return 0; }
  return 1;
}

static void *preparar(void *u) {
  Sessao *s = u;
  double t0 = agoraMs();
  int ord = s->audio;
  if (prepTorbox(s) && !s->cancel && prepOriginal(s, ord) && !s->cancel) {
    s->ultimo = tba_pl_ultimo(&s->pl, s->durArq);
    s->dur = s->ini[s->pl.nseg] - s->pl.seg[s->pl.nseg - 1].dur + s->ultimo;
    if (s->ultimo != s->pl.seg[s->pl.nseg - 1].dur)
      printf("[tbaudio] last segment %.3f s -> %.3f s: total %.3f s = the file's length (TorBox said %.3f s)\n",
             s->pl.seg[s->pl.nseg - 1].dur, s->ultimo, s->dur, s->ini[s->pl.nseg]);
    else
      printf("[tbaudio] total %.3f s kept (file %.3f s%s)\n", s->dur, s->durArq, s->pl.fim ? "" : ", no ENDLIST");
    s->espelho = tba_pl_espelho(s->plTexto, s->plBase, s->ultimo != s->pl.seg[s->pl.nseg - 1].dur ? s->ultimo : -1);
    s->naSeg = (int)floor(s->dur / AUD_SEG_S);   // a tail under 0.5 s joins the last segment
    if (s->dur - s->naSeg * AUD_SEG_S > 0.5) s->naSeg++;
    if (s->naSeg < 1) s->naSeg = 1;
    s->aIni = malloc(sizeof *s->aIni * (size_t)(s->naSeg + 1));
    if (s->aIni) { int k; for (k = 0; k < s->naSeg; k++) s->aIni[k] = k * AUD_SEG_S; s->aIni[s->naSeg] = s->dur; }
    else { free(s->espelho); s->espelho = NULL; }
    pthread_mutex_lock(&s->m);
    if (!s->falhou && s->espelho) {
      s->pronto = 1;
      printf("[tbaudio] ready in %.0f ms: %s, audio %d\n", agoraMs() - t0, modoNome(s->modo),
             s->fx[s->audio].ord);
    } else if (!s->falhou) { s->falhou = 1; snprintf(s->motivo, sizeof s->motivo, "out of memory"); }
    pthread_cond_broadcast(&s->c);
    pthread_mutex_unlock(&s->m);
    fflush(stdout);
  } else if (!s->falhou) falhar(s, s->cancel ? "closed" : "preparation failed");
  soltar(s);
  return NULL;
}

// ---- generator: packets of the active track, in source time --------------------
static int filaPor(Sessao *s, AVPacket *p, double t) {
  int k;
  if (s->fN == FILA_MAX) { av_packet_free(&p); return 0; }
  k = (s->fIni + s->fN) % FILA_MAX;
  s->fila[k].p = p; s->fila[k].t = t; s->fN++;
  return 1;
}
static int abrirConversor(Sessao *s, const Faixa *f, char *err, size_t en) {
  AVStream *st = s->ic->streams[f->si];
  const AVCodec *dc = avcodec_find_decoder(f->id);
  const AVCodec *ec = avcodec_find_encoder(f->modo == M_AC3 ? AV_CODEC_ID_AC3 : AV_CODEC_ID_AAC);
  AVChannelLayout alvo;
  int r, ch = f->modo == M_AC3 && f->canais > 2 ? 6 : 2;
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
  av_channel_layout_copy(&s->enc->ch_layout, &alvo);
  s->enc->sample_rate = 48000; s->enc->time_base = (AVRational){1, 48000};
  s->enc->sample_fmt = AV_SAMPLE_FMT_FLTP;
  s->enc->bit_rate = f->modo == M_AC3 ? (ch == 6 ? 640000 : 192000) : 192000;
  if (f->modo == M_AAC) { s->enc->profile = AV_PROFILE_AAC_LOW; s->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; }
  if ((r = avcodec_open2(s->enc, ec, NULL)) < 0) { snprintf(err, en, "audio encoder open (%d)", r); return -1; }
  s->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, ch, 8192);
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
    filaPor(s, o, o->pts / 48000.0);
  }
}
static int drenarFifo(Sessao *s, int tudo) {
  int fs = s->enc->frame_size > 0 ? s->enc->frame_size : 1536;
  while (av_audio_fifo_size(s->fifo) >= fs || (tudo && av_audio_fifo_size(s->fifo) > 0)) {
    int n = av_audio_fifo_size(s->fifo) < fs ? av_audio_fifo_size(s->fifo) : fs;
    av_frame_unref(s->ef);
    s->ef->nb_samples = fs; s->ef->format = AV_SAMPLE_FMT_FLTP; s->ef->sample_rate = 48000;
    av_channel_layout_copy(&s->ef->ch_layout, &s->enc->ch_layout);
    if (av_frame_get_buffer(s->ef, 0) < 0) return -1;
    if (n < fs) av_samples_set_silence(s->ef->data, 0, fs, s->ef->ch_layout.nb_channels, AV_SAMPLE_FMT_FLTP);
    av_audio_fifo_read(s->fifo, (void **)s->ef->data, n);
    s->ef->pts = s->base48 + s->amostras;
    s->amostras += fs;
    if (encodar(s, s->ef) < 0) return -1;
  }
  return 0;
}
// A packet's length in its stream's time base: the container's or the
// parser's, else FFmpeg's frame size for the codec. 0 = unknown.
static int64_t pacoteDur(const AVStream *st, const AVPacket *p) {
  int a;
  if (p->duration > 0) return p->duration;
  a = av_get_audio_frame_duration2(st->codecpar, p->size);
  return a > 0 && st->codecpar->sample_rate > 0 ? av_rescale_q(a, (AVRational){1, st->codecpar->sample_rate}, st->time_base) : 0;
}
// One more step: >0 progressed, 0 end of file, -1 error.
static int produzir(Sessao *s, const Faixa *f, char *err, size_t en) {
  AVStream *st = s->ic->streams[f->si];
  for (;;) {
    int r;
    if (s->cancel) { snprintf(err, en, "closed"); return -1; }
    if (s->eof) return 0;
    r = av_read_frame(s->ic, s->pk);
    s->leu = 1;
    if (r < 0) {
      if (r != AVERROR_EOF) { snprintf(err, en, "reading the original file (%d)", r); return -1; }
      s->eof = 1;
      if (s->enc) { drenarFifo(s, 1); encodar(s, NULL); }
      return 0;
    }
    if (s->pk->stream_index != f->si) { av_packet_unref(s->pk); continue; }
    if (s->pk->pts == AV_NOPTS_VALUE) s->pk->pts = s->pk->dts;
    // Matroska lacing without BlockDuration/DefaultDuration (owner 16/11, an
    // mkvmerge Opus track): only the block's first frame has a timestamp.
    // FFmpeg's parser for the codec gives the rest theirs (the Opus parser is
    // in the .tpk's FFmpeg for this: tools/build-dts-ffmpeg.sh). Safety net
    // for a codec without a parser: the packet follows the one before it
    // instead of being dropped (dropped = 14 of 100 Opus packets: no sound).
    {
      int64_t d = pacoteDur(st, s->pk);
      if (s->pk->pts == AV_NOPTS_VALUE && s->proxPts != AV_NOPTS_VALUE) {
        s->pk->pts = s->pk->dts = s->proxPts;
        s->semPts++;
        if (s->semPtsLog < 3) {
          s->semPtsLog++;
          printf("[tbaudio] audio %d: a packet without a timestamp (laced frame) follows the one before (%.3f s, %.1f ms)\n",
                 f->ord, s->proxPts * av_q2d(st->time_base), d * av_q2d(st->time_base) * 1000.0);
        }
      }
      if (s->pk->pts == AV_NOPTS_VALUE) { av_packet_unref(s->pk); continue; }
      if (s->pk->duration <= 0 && d > 0) s->pk->duration = d;
      s->proxPts = d > 0 ? s->pk->pts + d : AV_NOPTS_VALUE;
    }
    if (!s->dec) {
      AVPacket *c = av_packet_clone(s->pk);
      double t = s->pk->pts * av_q2d(st->time_base);
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
        if (swr_alloc_set_opts2(&s->swr, &s->enc->ch_layout, AV_SAMPLE_FMT_FLTP, 48000, &s->fr->ch_layout,
                                s->fr->format, s->fr->sample_rate, 0, NULL) < 0 || swr_init(s->swr) < 0) {
          snprintf(err, en, "resampler"); av_frame_unref(s->fr); return -1;
        }
      }
      if (s->base48 == AV_NOPTS_VALUE) {
        int64_t p = s->fr->pts != AV_NOPTS_VALUE ? s->fr->pts : s->fr->best_effort_timestamp;
        s->base48 = p != AV_NOPTS_VALUE ? av_rescale_q(p, st->time_base, (AVRational){1, 48000}) : 0;
      }
      { int max = swr_get_out_samples(s->swr, s->fr->nb_samples);
        uint8_t **buf = NULL;
        if (max > 0 && av_samples_alloc_array_and_samples(&buf, NULL, s->enc->ch_layout.nb_channels, max,
                                                          AV_SAMPLE_FMT_FLTP, 0) >= 0) {
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
  double alvo = ini[seg] + s->origV0;
  int mesmo = s->gOk && s->gFaixa == fi && s->gModo == f->modo && s->gIni == ini && s->gSeg >= 0;
  *saltou = 0; *como = "continued";
  if (mesmo && s->gSeg == seg) return 0;
  if (mesmo && seg > s->gSeg && ini[seg] - ini[s->gSeg] <= len + 0.5) { *saltou = 1; *como = "read ahead"; return 0; }
  *saltou = 1; *como = "seeked";
  filaLimpar(s);
  s->eof = 0;
  s->proxPts = AV_NOPTS_VALUE;   // a seek: the next timestamp comes from the file again
  { int i; for (i = 0; i < (int)s->ic->nb_streams; i++) s->ic->streams[i]->discard = i == f->si ? AVDISCARD_DEFAULT : AVDISCARD_ALL; }
  if (f->modo == M_AC3 || f->modo == M_AAC) { if (abrirConversor(s, f, err, en) < 0) { s->gOk = 0; return -1; } }
  else codecLiberar(s);
  dts_engine_interrupt(s->eng, 0);
  if (seg > 0 || s->leu) {
    // Straight to the segment: the index gives the keyframe at or before it.
    AVStream *b = s->ic->streams[s->sBusca];
    int64_t ts = av_rescale_q((int64_t)(alvo * AV_TIME_BASE), AV_TIME_BASE_Q, b->time_base);
    // Before the index's first entry Matroska does not move at all (it
    // returns 0 and the reader stays where it was): ask for the first entry.
    if (avformat_index_get_entries_count(b) > 0 && av_index_search_timestamp(b, ts, AVSEEK_FLAG_BACKWARD) < 0) {
      const AVIndexEntry *e0 = avformat_index_get_entry(b, 0);
      if (e0 && e0->timestamp > ts) {
        printf("[tbaudio] seek to %.3f s is before the index (first entry %.3f s): going to the first entry\n", alvo,
               e0->timestamp * av_q2d(b->time_base));
        ts = e0->timestamp;
      }
    }
    if (av_seek_frame(s->ic, s->sBusca, ts, AVSEEK_FLAG_BACKWARD) < 0 &&
        av_seek_frame(s->ic, -1, (int64_t)(alvo * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD) < 0) {
      snprintf(err, en, "seek in the original file to %.1f s", alvo); s->gOk = 0; return -1;
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
  if (s->enc && (f->modo == M_AC3 || f->modo == M_AAC)) {
    avcodec_parameters_from_context(p, s->enc);
    *tb = (AVRational){1, 48000};
  } else {
    avcodec_parameters_copy(p, s->ic->streams[f->si]->codecpar);
    *tb = s->ic->streams[f->si]->time_base;
  }
  return 0;
}
// Writes one audio packet (source time base `tb`) shifted onto TorBox's timeline.
static int escreverAudio(Sessao *s, Mux *m, int idx, AVPacket *p, AVRational tb, int interl) {
  AVStream *st = m->oc->streams[idx];
  double desloc = (s->tbOffOk ? s->tbOff : 0) - s->origV0;
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

// Audio of track fi, segment `seg` (AUD_SEG_S) or -1 = the fMP4 init.
// -2 = stopped (the player hung up or moved on), nothing to send.
static int gerarAudio(Sessao *s, int fi, int seg, Resp *r, char *err, size_t en, int so, long chave) {
  const Faixa *f = &s->fx[fi];
  AVCodecParameters *par = avcodec_parameters_alloc();
  AVRational tb, srcTb;
  Mux m;
  int mp4 = f->modo == M_MP4, saltou = 0, nPk = 0, rr = 0, ab = 0;
  // Opus diagnostic (owner 16/11: 13-15 packets per 2 s where 20 ms frames
  // give 100): durations, sizes and the first packet's TOC, first segments only.
  int opusDiag = 0, toc = -1, tocN = 0, opSemDur = 0;   // set once positioned (a converter may still be open before)
  double opDurMin = 1e9, opDurMax = 0; int opTamMin = 1 << 30, opTamMax = 0;
  const char *como = "";
  double t0 = agoraMs(), fs, fe, pri = -1, tl;
  if (!par) { snprintf(err, en, "out of memory"); return -1; }
  if (seg < 0) {   // init: the header alone (the converter state is not touched)
    avcodec_parameters_copy(par, s->ic->streams[f->si]->codecpar);
    if (muxAbrir(&m, "mp4", NULL, (AVRational){0, 1}, par, s->ic->streams[f->si]->time_base, f->lingua) < 0) {
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
  opusDiag = f->id == AV_CODEC_ID_OPUS && !s->dec && s->opusLog < 3;
  parametrosSaida(s, f, par, &tb);
  srcTb = s->dec ? (AVRational){1, 48000} : s->ic->streams[f->si]->time_base;
  if (muxAbrir(&m, mp4 ? "mp4" : "mpegts", NULL, (AVRational){0, 1}, par, tb, f->lingua) < 0) {
    muxFechar(&m); avcodec_parameters_free(&par); snprintf(err, en, "muxer"); return -1;
  }
  avcodec_parameters_free(&par);
  fs = s->aIni[seg] + s->origV0;
  fe = seg + 1 < s->naSeg ? s->aIni[seg + 1] + s->origV0 : 1e18;
  for (;;) {
    Item it;
    if ((ab = abandonar(s, so, chave)) != 0) { rr = -2; break; }
    if (!s->fN) { rr = produzir(s, f, err, en); if (rr < 0) break; if (rr == 0 && !s->fN) { rr = 0; break; } continue; }
    it = s->fila[s->fIni];
    if (it.t >= fe) break;
    s->fIni = (s->fIni + 1) % FILA_MAX; s->fN--;
    if (saltou && it.t < fs - 0.001) { av_packet_free(&it.p); continue; }
    if (pri < 0) pri = it.t;
    if (opusDiag) {
      double d = it.p->duration > 0 ? it.p->duration * av_q2d(srcTb) * 1000.0 : -1;
      if (d < 0) opSemDur++; else { if (d < opDurMin) opDurMin = d; if (d > opDurMax) opDurMax = d; }
      if (it.p->size < opTamMin) opTamMin = it.p->size;
      if (it.p->size > opTamMax) opTamMax = it.p->size;
      if (toc < 0 && it.p->size > 0) { toc = it.p->data[0]; tocN = it.p->size > 1 ? it.p->data[1] : 0; }
    }
    if (escreverAudio(s, &m, 0, it.p, srcTb, 0) < 0) { av_packet_free(&it.p); snprintf(err, en, "mux write"); rr = -1; break; }
    av_packet_free(&it.p);
    nPk++;
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
  if (rr < 0) { muxFechar(&m); s->gOk = 0; return -1; }
  r->b = muxFim(&m, mp4, mp4, &r->n);
  if (!r->b) { snprintf(err, en, "out of memory"); return -1; }
  snprintf(r->tipo, sizeof r->tipo, mp4 ? "audio/mp4" : "video/mp2t");
  s->gSeg = seg + 1;
  s->nSeg++; s->bytes += (long long)r->n; s->msLer += tl - t0;
  printf("[tbaudio] audio %d seg %d (%.1f-%.1f s): %d packets, %zu KB, %.0f ms, %s; first audio at %.3f s\n",
         f->ord, seg, s->aIni[seg], s->aIni[seg + 1], nPk, r->n / 1024, tl - t0, como, pri >= 0 ? pri - s->origV0 : -1.0);
  if (opusDiag && nPk > 0) {
    // TOC: config (frame length), code 0 = 1 frame, 1/2 = 2 frames, 3 = N frames (byte 2).
    static const double DUR[32] = { 10, 20, 40, 60, 10, 20, 40, 60, 10, 20, 40, 60, 10, 20, 10, 20,
                                    2.5, 5, 10, 20, 2.5, 5, 10, 20, 2.5, 5, 10, 20, 2.5, 5, 10, 20 };
    int cfg = toc >> 3, code = toc & 3, quadros = code == 0 ? 1 : code < 3 ? 2 : (tocN & 0x3f);
    s->opusLog++;
    printf("[tbaudio] Opus audio %d seg %d: %d packets, durations %s%.1f-%.1f ms%s, sizes %d-%d bytes | first packet TOC: "
           "config %d (%.1f ms frames), code %d, %d frame(s) = %.1f ms | extradata %d bytes, %d ch\n",
           f->ord, seg, nPk, opDurMax > 0 ? "" : "none, ", opDurMax > 0 ? opDurMin : 0.0, opDurMax, opSemDur ? " (some unset)" : "",
           opTamMin, opTamMax, cfg, DUR[cfg & 31], code, quadros, DUR[cfg & 31] * quadros,
           s->ic->streams[f->si]->codecpar->extradata_size, f->canais);
  }
  fflush(stdout);
  return 0;
}

static double durSeg(const Sessao *s, int i);
// Passthrough: TorBox's video segment without its audio. vm, not gm: the
// video never waits behind the audio generator.
static int gerarVideo(Sessao *s, int seg, Resp *r, char *err, size_t en) {
  double ms = 0, t0 = agoraMs();
  size_t n = 0, k = 0;
  int st = 0, nA = 0;
  unsigned char *b;
  tbr_servido(s->tbrGer, seg);   // the first one after an open/seek: the segment the player landed in
  b = baixar(s, s->pl.seg[seg].uri, TB_SEG_MAX, &n, NULL, 0, &st, &ms);
  if (!b) { snprintf(err, en, "TorBox segment %d did not download (HTTP %d)", seg, st); return -1; }
  if (s->tbMp4) {
    if (!s->stripAviso++) { printf("[tbaudio] TorBox's segments are fMP4: passed as they are (only MPEG-TS is stripped)\n"); fflush(stdout); }
    r->b = b; r->n = n; snprintf(r->tipo, sizeof r->tipo, "video/mp4");
    return 0;
  }
  r->b = malloc(n + 1);
  if (r->b && tba_ts_sem_audio(b, n, r->b, &k, &nA)) {
    free(b); r->n = k;
  } else {
    if (!s->stripAviso++) { printf("[tbaudio] TorBox's audio could not be removed from segment %d (the clock is on it, or a PMT over 1 packet): passed as it is\n", seg); fflush(stdout); }
    free(r->b); r->b = b; r->n = n;
  }
  snprintf(r->tipo, sizeof r->tipo, "video/mp2t");
  s->nVid++; s->msVid += agoraMs() - t0;
  printf("[tbaudio] video seg %d: TorBox %zu KB in %.0f ms, %d audio stream(s) removed, %zu KB out\n", seg, n / 1024, ms, nA, r->n / 1024);
  /* Burn-in subtitle timing (owner 14/11): the TV's position on Nuvio's
   * playlist is the sum of the segment lengths before this one; the picture
   * is stamped with its first video PTS. A gap between them moves the
   * subtitles drawn over the burned picture. */
  { double pts = 0, ini = 0;
    int i;
    for (i = 0; i < seg && i < s->pl.nseg; i++) ini += durSeg(s, i);
    if (tba_ts_primeiro_pts(r->b, r->n, &pts)) {
      tbr_pts(s->tbrGer, seg, pts);
      printf("[tbaudio] video seg %d timing: playlist start %.3f s, first video PTS %.3f s (PTS - start %+.3f s; TorBox's first PTS %.3f)\n",
             seg, ini, pts, pts - ini, s->tbOffOk ? s->tbOff : -1.0);
    }
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
static double durSeg(const Sessao *s, int i) { return i == s->pl.nseg - 1 ? s->ultimo : s->pl.seg[i].dur; }
// TorBox's video segments, without its audio.
static void plMidia(Sessao *s, Txt *t, const char *pref, const char *ext) {
  double max = 0;
  int i;
  for (i = 0; i < s->pl.nseg; i++) if (durSeg(s, i) > max) max = durSeg(s, i);
  tx(t, "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:0\n#EXT-X-PLAYLIST-TYPE:VOD\n"
        "#EXT-X-INDEPENDENT-SEGMENTS\n", (int)ceil(max));
  for (i = 0; i < s->pl.nseg; i++) tx(t, "#EXTINF:%.6f,\n%s%d%s\n", durSeg(s, i), pref, i, ext);
  tx(t, "#EXT-X-ENDLIST\n");
}
// One audio rendition: AUD_SEG_S segments (short: a seek reads ~2 s).
static void plAudio(Sessao *s, Txt *t, int k) {
  int i, mp4 = s->fx[k].modo == M_MP4;
  tx(t, "#EXTM3U\n#EXT-X-VERSION:%d\n#EXT-X-TARGETDURATION:%d\n#EXT-X-MEDIA-SEQUENCE:0\n#EXT-X-PLAYLIST-TYPE:VOD\n"
        "#EXT-X-INDEPENDENT-SEGMENTS\n", mp4 ? 7 : 3, (int)ceil(AUD_SEG_S));
  if (mp4) tx(t, "#EXT-X-MAP:URI=\"a%d_init.mp4\"\n", k);
  for (i = 0; i < s->naSeg; i++) tx(t, "#EXTINF:%.6f,\na%d_%d%s\n", s->aIni[i + 1] - s->aIni[i], k, i, mp4 ? ".m4s" : ".ts");
  tx(t, "#EXT-X-ENDLIST\n");
}
static void nomeRendicao(const Faixa *f, char *o, size_t n) {
  char *c;
  snprintf(o, n, "%s %d (%s %dch)", f->nome[0] ? f->nome : nomeLingua(f->lingua), f->ord + 1, f->codec, f->canais);
  for (c = o; *c; c++) if (*c == '"' || *c == ',' || (unsigned char)*c < 32) *c = ' ';
}
static void plMaster(Sessao *s, Txt *t) {
  int i, mp4 = 0;
  for (i = 0; i < s->nfx; i++) if (s->fx[i].modo == M_MP4) mp4 = 1;
  tx(t, "#EXTM3U\n#EXT-X-VERSION:%d\n#EXT-X-INDEPENDENT-SEGMENTS\n", mp4 ? 7 : 4);
  for (i = 0; i < s->nfx; i++) {
    const Faixa *f = &s->fx[i];
    char nome[128];
    int ch = f->modo == M_AC3 ? (f->canais > 2 ? 6 : 2) : f->modo == M_AAC ? 2 : f->canais;
    if (f->modo == M_FORA) continue;
    nomeRendicao(f, nome, sizeof nome);
    tx(t, "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud\",NAME=\"%s\"", nome);
    if (f->lingua[0]) tx(t, ",LANGUAGE=\"%s\"", f->lingua);
    tx(t, ",DEFAULT=%s,AUTOSELECT=YES", i == s->audio ? "YES" : "NO");
    if (ch > 0) tx(t, ",CHANNELS=\"%d\"", ch);
    tx(t, ",URI=\"a%d.m3u8\"\n", i);
  }
  tx(t, "#EXT-X-STREAM-INF:BANDWIDTH=%ld,AUDIO=\"aud\"", s->banda > 0 ? s->banda + 1500000 : 12000000L);
  if (s->resolucao[0]) tx(t, ",RESOLUTION=%s", s->resolucao);
  // Passthrough: the video through Nuvio without TorBox's audio. fMP4 TorBox
  // segments are not stripped: the TV takes them from TorBox (v.m3u8).
  tx(t, "\n%s\n", s->tbMp4 ? "v.m3u8" : "vs.m3u8");
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
  if (e == -2) { printf("[tbaudio] %s dropped: %s\n", nome, err); fflush(stdout); return 0; }
  if (e < 0) {
    if (!s->cancel) { s->nErro++; snprintf(s->ultErro, sizeof s->ultErro, "%s", err); printf("[tbaudio] %s failed: %s\n", nome, err); fflush(stdout); }
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
  if (!esperarPronto(s)) {
    printf("[tbaudio] request %s: the session is not ready (%s)\n", nome, s->motivo[0] ? s->motivo : "timeout");
    fflush(stdout);
    ok = responder(so, 503, NULL, "", 0, 0, head, s) && 0;
    soltar(s); return ok;
  }
  if (!strcmp(nome, "master.m3u8") || !strcmp(nome, "vs.m3u8")) {
    Txt t = { 0 };
    if (nome[0] == 'm') plMaster(s, &t); else plMidia(s, &t, "v_", ".ts");
    ok = t.b && responder(so, 200, "application/vnd.apple.mpegurl", t.b, t.n, manter, head, s);
    printf("[tbaudio] %s served (%zu bytes)%s\n", nome, t.n, nome[0] == 'v' ? ": TorBox's video through Nuvio, its audio removed" : "");
    fflush(stdout);
    free(t.b);
  } else if (!strcmp(nome, "v.m3u8")) {
    ok = responder(so, 200, "application/vnd.apple.mpegurl", s->espelho, strlen(s->espelho), manter, head, s);
    printf("[tbaudio] v.m3u8 served: TorBox's playlist, URIs absolute (the TV fetches TorBox's video itself)\n");
    fflush(stdout);
  } else if (sscanf(nome, "a%d.m3u8", &k) == 1 && strstr(nome, ".m3u8") && !strchr(nome, '_')) {
    Txt t = { 0 };
    if (k < 0 || k >= s->nfx || s->fx[k].modo == M_FORA) { soltar(s); return responder(so, 404, NULL, "", 0, manter, head, s); }
    plAudio(s, &t, k);
    ok = t.b && responder(so, 200, "application/vnd.apple.mpegurl", t.b, t.n, manter, head, s);
    printf("[tbaudio] audio playlist a%d (audio %d, %s, %d segments of %.0f s) served\n", k, s->fx[k].ord, MODO_NOME[s->fx[k].modo],
           s->naSeg, AUD_SEG_S);
    fflush(stdout);
    free(t.b);
  } else if (sscanf(nome, "a%d_init.mp4", &k) == 1 && strstr(nome, "_init.mp4")) {
    if (k < 0 || k >= s->nfx || s->fx[k].modo != M_MP4) { soltar(s); return responder(so, 404, NULL, "", 0, manter, head, s); }
    pthread_mutex_lock(&s->gm);
    e = gerarAudio(s, k, -1, &r, err, sizeof err, -1, -1);
    pthread_mutex_unlock(&s->gm);
    printf("[tbaudio] a%d init: %s (%zu bytes)\n", k, e < 0 ? err : "ok", r.n);
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
  } else if (sscanf(nome, "v_%d.ts", &i) == 1) {
    if (i < 0 || i >= s->pl.nseg) { soltar(s); return responder(so, 404, NULL, "", 0, manter, head, s); }
    pthread_mutex_lock(&s->vm);
    e = gerarVideo(s, i, &r, err, sizeof err);
    pthread_mutex_unlock(&s->vm);
    ok = entregar(so, s, nome, e, &r, err, manter, head);
  } else ok = responder(so, 404, NULL, "", 0, manter, head, s);
  if (r.b && (agoraMs() - t0) > 4000) { printf("[tbaudio] %s took %.0f ms (the player waited)\n", nome, agoraMs() - t0); fflush(stdout); }
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
      printf("[tbaudio] %d connections open: 503\n", TBA_CONEXOES); fflush(stdout);
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
int tba_disponivel(void) { return avcodec_find_encoder(AV_CODEC_ID_AC3) != NULL; }

int tba_abrir(const char *tbUrl, const char *orig, const char *cab, int ordinal, int modo,
              char *out, size_t n) {
  Sessao *s;
  pthread_t t;
  if (out && n) out[0] = 0;
  if (!tbUrl || !*tbUrl || !orig || !*orig || !out) return 0;
  if (modo != TBA_PASSTHROUGH) return 0;   // TorBox audio: no session
  tba_fechar();
  pthread_mutex_lock(&trava);
  if (!subirServidor()) { pthread_mutex_unlock(&trava); printf("[tbaudio] local server did not start\n"); return 0; }
  pthread_mutex_unlock(&trava);
  s = calloc(1, sizeof *s);
  if (!s) return 0;
  atomic_store(&s->refs, 2);   // the API's, the preparation's
  pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->c, NULL); pthread_mutex_init(&s->gm, NULL);
  pthread_mutex_init(&s->vm, NULL);
  atomic_store(&s->espChave, -1L);
  s->modo = modo;
  s->audio = ordinal; s->gFaixa = -1; s->gSeg = -1; s->proxPts = AV_NOPTS_VALUE;
  s->tbrGer = tbr_geracao();
  s->tbUrl = strdup(tbUrl); s->orig = strdup(orig); s->cab = strdup(cab ? cab : "");
  gerarToken(s->tok, 28);
  if (!s->tbUrl || !s->orig || !s->cab) { atomic_store(&s->refs, 1); soltar(s); return 0; }
  pthread_mutex_lock(&trava);
  atual = s; hostAudioN = -1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&t, NULL, preparar, s) != 0) { atomic_fetch_sub(&s->refs, 1); falhar(s, "no thread"); }
  else pthread_detach(t);
  snprintf(out, n, "http://127.0.0.1:%d/%s/master.m3u8", porta, s->tok);
  printf("[tbaudio] session open: %s, audio %d, FLAC and Opus as is (fMP4); the player opens http://127.0.0.1:%d/<token>/master.m3u8\n",
         modoNome(s->modo), ordinal, porta);
  fflush(stdout);
  return 1;
}
void tba_fechar(void) {
  Sessao *s;
  pthread_mutex_lock(&trava);
  s = atual; atual = NULL;
  pthread_mutex_unlock(&trava);
  if (!s) return;
  s->cancel = 1;
  if (s->eng) dts_engine_interrupt(s->eng, 1);
  pthread_mutex_lock(&s->m); pthread_cond_broadcast(&s->c); pthread_mutex_unlock(&s->m);
  printf("[tbaudio] session closed: %ld audio + %ld video segments, %lld KB of audio, %ld dropped, %ld failed%s%s\n",
         s->nSeg, s->nVid, s->bytes / 1024, s->nLarg, s->nErro, s->ultErro[0] ? "; last error: " : "", s->ultErro);
  fflush(stdout);
  soltar(s);
}
int tba_ativo(void) { Sessao *s = pegar(); int v = s != NULL; soltar(s); return v; }
int tba_modo(void) { Sessao *s = pegar(); int v = s ? s->modo : -1; soltar(s); return v; }
int tba_falhou(char *m, size_t n) {
  Sessao *s = pegar();
  int v = 0;
  if (m && n) m[0] = 0;
  if (s) { pthread_mutex_lock(&s->m); v = s->falhou; if (v && m && n) snprintf(m, n, "%s", s->motivo); pthread_mutex_unlock(&s->m); }
  soltar(s);
  return v;
}
int tba_pronto(void) { Sessao *s = pegar(); int v = s && s->pronto; soltar(s); return v; }
int tba_n_rendicoes(void) {
  Sessao *s = pegar();
  int v = 0, i;
  if (s && s->pronto) for (i = 0; i < s->nfx; i++) if (s->fx[i].modo != M_FORA) v++;
  soltar(s);
  return v;
}
int tba_rendicao_de(int ord) {
  Sessao *s = pegar();
  int v = -1, i, k = 0;
  if (s && s->pronto)
    for (i = 0; i < s->nfx; i++) {
      if (s->fx[i].modo == M_FORA) continue;
      if (s->fx[i].ord == ord) { v = k; break; }
      k++;
    }
  soltar(s);
  return v;
}
void tba_definir_audio(int ord) {
  Sessao *s = pegar();
  int i;
  if (s && s->pronto) {
    pthread_mutex_lock(&s->gm);
    for (i = 0; i < s->nfx; i++) if (s->fx[i].ord == ord && s->fx[i].modo != M_FORA) s->audio = i;
    pthread_mutex_unlock(&s->gm);
    printf("[tbaudio] audio %d is the session's track now\n", ord);
    fflush(stdout);
  }
  soltar(s);
}
int tba_audio(void) { Sessao *s = pegar(); int v = s && s->pronto && s->audio >= 0 ? s->fx[s->audio].ord : -1; soltar(s); return v; }
void tba_log(void) {
  Sessao *s = pegar();
  if (s) {
    printf("[tbaudio] 10s: %s, %s, audio segments %ld (%lld KB, avg %.0f ms), video segments %ld (avg %.0f ms), "
           "dropped %ld, failed %ld%s%s\n", modoNome(s->modo), s->pronto ? "ready" : s->falhou ? "failed" : "preparing",
           s->nSeg, s->bytes / 1024, s->nSeg ? s->msLer / s->nSeg : 0.0, s->nVid, s->nVid ? s->msVid / s->nVid : 0.0,
           s->nLarg, s->nErro, s->ultErro[0] ? "; last: " : "", s->ultErro);
    fflush(stdout);
  }
  soltar(s);
}
#endif
