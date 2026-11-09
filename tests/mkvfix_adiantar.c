// PREFETCH ON LEAVING THE APP (mkvfix.h), end to end with a local fake CDN.
//
// The TV plays an MKV through Nuvio's local server. When Nuvio leaves the
// screen at a point, the server downloads the start of that point's Cluster
// (found in the Cues it already has in memory) so the reopen's seek gets those
// bytes locally and only the rest from the CDN. This drives the real server
// (mkvfix.c + rede.c) against tests/mkvfix_adiantar_server.py and acts as the
// TV, checking every byte and which ranges reached the CDN.
#include "mkvfix.h"
#include "rede.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  (void)usenet; (void)item; (void)file; if (n) dst[0] = 0; return 0;
}

#define TOTAL (80LL * 1024 * 1024)
#define MIB (1024LL * 1024)
static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok  " : "FAIL", m); fflush(stdout); if (!c) falhas++; }
static const char *dir;

static long el(unsigned char *o, uint32_t id, const unsigned char *p, long n) {
  long k = 0;
  int w = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  for (int i = w - 1; i >= 0; i--) o[k++] = (unsigned char)(id >> (8 * i));
  if (n < 127) o[k++] = (unsigned char)(0x80 | n);
  else { o[k++] = 0x40 | (unsigned char)(n >> 8); o[k++] = (unsigned char)n; }
  memcpy(o + k, p, (size_t)n);
  return k + n;
}
static long seek(unsigned char *o, uint32_t alvo, uint32_t pos) {
  unsigned char a[4] = { alvo >> 24, alvo >> 16, alvo >> 8, alvo }, b[4] = { pos >> 24, pos >> 16, pos >> 8, pos };
  unsigned char v[32]; long n = 0;
  n += el(v + n, 0x53AB, a, 4);
  n += el(v + n, 0x53AC, b, 4);
  return el(o, 0x4DBB, v, n);
}
static unsigned char padrao(long long off) { return (unsigned char)((off * 7 + 3) % 251); }
static void gravar(const char *nome, const unsigned char *b, long n) {
  char p[512]; FILE *f;
  snprintf(p, sizeof p, "%s/%s", dir, nome);
  f = fopen(p, "wb"); fwrite(b, 1, (size_t)n, f); fclose(f);
}
// How many CDN requests started exactly at `ini`.
static int pedidosEm(long long ini) {
  char p[512], l[128]; FILE *f; int n = 0;
  snprintf(p, sizeof p, "%s/pedidos.txt", dir);
  if (!(f = fopen(p, "r"))) return 0;
  while (fgets(l, sizeof l, f)) if (atoll(l) == ini) n++;
  fclose(f);
  return n;
}
static uint64_t msAgora(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}
// The fake CDN accepts a request starting at x and never answers (-1 = none).
static void pendurar(long long x) {
  char p[512]; FILE *f;
  snprintf(p, sizeof p, "%s/pendurar", dir);
  if ((f = fopen(p, "w"))) { fprintf(f, "%lld", x); fclose(f); }
}
// The TV: one long read "bytes=a-" through the local server, as its player
// does after a seek; keeps b - a + 1 bytes and checks the pattern.
typedef struct { char *b; long n, max; } Tv;
static int tvRalo(void *u, const char *d, size_t n) {
  Tv *t = u;
  size_t k = n > (size_t)(t->max - t->n) ? (size_t)(t->max - t->n) : n;
  memcpy(t->b + t->n, d, k); t->n += (long)k;
  return t->n < t->max;
}
static int tvLer(const char *local, long long a, long long b, int *certo) {
  Tv t = { malloc((size_t)(b - a + 1)), 0, (long)(b - a + 1) };
  int st = 0, x = 0; int64_t total = -1, veio = 0;
  long bad = -1;
  rede_fluxo64(local, "", a, tvRalo, &t, &total, &st, NULL, NULL, &x, 10000, &veio);
  *certo = t.b && t.n == t.max;
  for (long i = 0; *certo && i < t.n; i++) if ((unsigned char)t.b[i] != padrao(a + i)) { *certo = 0; bad = i; }
  if (!*certo) printf("  (TV read %lld-%lld: HTTP %d, %ld bytes, first wrong byte at %ld)\n", a, b, st, t.n, bad);
  free(t.b);
  usleep(300000);   // the server logs/cleans after the TV closes
  return st;
}

int main(int argc, char **argv) {
  unsigned char head[512], tail[4096], cues[3000], sh[64], z[128] = {0}, cab[128];
  long k = 0, nc = 0, ns, segIni, tailIni, cuesRel, sh2Rel;
  long long clusterRel[32];
  char orig[256], local[256];
  int certo, st;
  if (argc < 3) return 2;
  dir = argv[1];
  snprintf(orig, sizeof orig, "http://127.0.0.1:%s/f.mkv", argv[2]);

  // Cues: video track 1, a point every 2 s for 60 s (1 MiB/s), then one at
  // 62 s 14 MiB later (a dense stretch); Clusters spread in the file.
  for (int i = 0; i <= 31; i++) {
    unsigned char cp[32], tp[16]; long m = 0, t = 0;
    unsigned tempo = (unsigned)i * 2000u;
    clusterRel[i] = i < 31 ? 1 * MIB + (long long)i * 2 * MIB : clusterRel[30] + 14 * MIB;
    unsigned char tv[4] = { tempo >> 24, tempo >> 16, tempo >> 8, tempo };
    unsigned char pv[4] = { (unsigned char)(clusterRel[i] >> 24), (unsigned char)(clusterRel[i] >> 16),
                            (unsigned char)(clusterRel[i] >> 8), (unsigned char)clusterRel[i] };
    unsigned char faixa = 1;
    t += el(tp + t, 0xF7, &faixa, 1);
    t += el(tp + t, 0xF1, pv, 4);
    m += el(cp + m, 0xB3, tv, 4);
    m += el(cp + m, 0xB7, tp, t);
    nc += el(cues + nc, 0xBB, cp, m);
  }
  // Tail = second SeekHead (lists the Cues) + Cues, at the end of the file.
  { unsigned char cuesEl[3100]; long nce = el(cuesEl, 0x1C53BB6B, cues, nc);
    // EBML header + Segment (unknown size); first SeekHead -> second SeekHead only.
    k += el(head, 0x1A45DFA3, (const unsigned char *)"\x42\x82\x88matroska", 11);
    head[k++] = 0x18; head[k++] = 0x53; head[k++] = 0x80; head[k++] = 0x67; head[k++] = 0x01;
    memset(head + k, 0xFF, 7); k += 7;
    segIni = k;
    ns = 64;   // second SeekHead size, fixed below
    tailIni = (long)(TOTAL - (ns + nce));
    sh2Rel = tailIni - segIni;
    cuesRel = sh2Rel + ns;
    { long m = seek(sh, 0x114D9B74u, (uint32_t)sh2Rel), shn;
      shn = el(head + k, 0x114D9B74u, sh, m); k += shn;
      k += el(head + k, 0xEC, z, 70 - shn - 2);
      k += el(head + k, 0x1549A966u, z, 8); }
    // Second SeekHead padded with a Void to exactly ns bytes.
    { long m = seek(cab, 0x1C53BB6Bu, (uint32_t)cuesRel), t2;
      t2 = el(tail, 0x114D9B74u, cab, m);
      t2 += el(tail + t2, 0xEC, z, ns - t2 - 2);
      memcpy(tail + t2, cuesEl, (size_t)nce);
      gravar("tail.bin", tail, t2 + nce); }
    gravar("head.bin", head, k); }

  mkvfix_sondar(orig, "", NULL);
  for (int i = 0; i < 100 && mkvfix_estado() == MKVFIX_SONDANDO; i++) usleep(50000);
  ok(mkvfix_estado() == MKVFIX_PRECISA, "probe: the hidden index is found (file plays through the local server)");
  ok(mkvfix_url(local, sizeof local), "probe: local URL");

  // The TV opens: header, then the index at the end (a short read: kept in memory).
  { long n = 0; int64_t tot = -1;
    char *r = rede_baixar_trecho64_final(local, "", 0, 16383, &n, &tot, &st, NULL, NULL, NULL);
    ok(r && n == 16384, "TV: header read through the server"); free(r);
    r = rede_baixar_trecho64_final(local, "", tailIni, TOTAL - 1, &n, &tot, &st, NULL, NULL, NULL);
    ok(r && n == TOTAL - tailIni, "TV: index read through the server"); free(r);
    usleep(300000); }   // the server keeps it right after sending

  // Leave the app at 31.0 s: the point's Cluster is the one of the 30 s cue.
  { long long x = segIni + clusterRel[15];
    mkvfix_adiantar(31.0, 60.0);
    for (int i = 0; i < 60 && !pedidosEm(x); i++) usleep(50000);
    ok(pedidosEm(x) == 1, "prefetch: one CDN request at the 30 s cue's Cluster");
    sleep(3);   // 8 MiB at most 4 MiB/s
    // The reopen's seek: from the Cluster, 10 MiB.
    st = tvLer(local, x, x + 10 * MIB - 1, &certo);
    ok(certo, "reopen seek: all 10 MiB correct (8 from memory, the rest from the CDN)");
    ok(pedidosEm(x) == 1, "reopen seek: the start was NOT asked from the CDN again");
    ok(pedidosEm(x + 8 * MIB) == 1, "reopen seek: the CDN was asked only from the end of the prefetch");
    // Used once: the next identical read goes to the CDN.
    st = tvLer(local, x, x + 1 * MIB + 4 * MIB, &certo);
    ok(certo && pedidosEm(x) == 2, "used once: a second read goes to the CDN"); }

  // Dropped (low memory / player closed / another video): nothing served from memory.
  { long long x = segIni + clusterRel[5];
    mkvfix_adiantar(10.5, 60.0);
    for (int i = 0; i < 60 && !pedidosEm(x); i++) usleep(50000);
    sleep(3);
    mkvfix_adiantar_soltar("test");
    st = tvLer(local, x, x + 5 * MIB - 1, &certo);
    ok(certo && pedidosEm(x) == 2, "dropped: the read goes to the CDN"); }

  // Seek elsewhere (the guess missed): correct bytes from the CDN.
  { long long x = segIni + clusterRel[20], y = segIni + clusterRel[25];
    mkvfix_adiantar(40.2, 60.0);
    for (int i = 0; i < 60 && !pedidosEm(x); i++) usleep(50000);
    sleep(3);
    st = tvLer(local, y, y + 5 * MIB - 1, &certo);
    ok(certo && pedidosEm(y) == 1, "missed guess: other bytes come right from the CDN");
    mkvfix_adiantar_soltar("test"); }

  // Return before the prefetch is done: no brake, the seek waits for it (<= 2 s).
  { long long x = segIni + clusterRel[10];
    mkvfix_adiantar(20.0, 60.0);
    for (int i = 0; i < 60 && !pedidosEm(x); i++) usleep(50000);
    mkvfix_adiantar_pressa();
    st = tvLer(local, x, x + 10 * MIB - 1, &certo);
    ok(certo && pedidosEm(x) == 1, "early return: the seek still gets the prefetch, nothing asked twice"); }

  // Dense stretch: the window starts at the estimate of the second, not at the
  // cue (60 s cue at 61 MiB, 62 s cue 14 MiB later; 61.0 s -> estimate 68 MiB,
  // window from 65 MiB). The TV asking from 68 MiB is served from memory.
  { long long cue = segIni + clusterRel[30], jan = cue + 4 * MIB, tv = cue + 7 * MIB;
    mkvfix_adiantar(61.0, 64.0);
    for (int i = 0; i < 60 && !pedidosEm(jan); i++) usleep(50000);
    ok(pedidosEm(jan) == 1 && pedidosEm(cue) == 0, "dense stretch: the window starts at the estimate, not at the cue");
    sleep(3);
    st = tvLer(local, tv, tv + 8 * MIB - 1, &certo);
    ok(certo && pedidosEm(tv) == 0 && pedidosEm(jan + 8 * MIB) == 1,
       "dense stretch: the TV's request inside the window is served from memory, the rest from the CDN"); }

  // A new request replaces a running one (leaving again; a moved point): no wait.
  { long long a1 = segIni + clusterRel[3], a2 = segIni + clusterRel[8];
    mkvfix_adiantar(6.0, 64.0);
    for (int i = 0; i < 60 && !pedidosEm(a1); i++) usleep(50000);
    mkvfix_adiantar(16.0, 64.0);
    for (int i = 0; i < 80 && !pedidosEm(a2); i++) usleep(50000);
    sleep(3);
    st = tvLer(local, a2, a2 + 9 * MIB, &certo);
    ok(certo && pedidosEm(a2) == 1, "replaced: the newer point is served from memory");
    st = tvLer(local, a1, a1 + 5 * MIB, &certo);
    ok(certo && pedidosEm(a1) == 2, "replaced: the older point is not kept"); }

  // Wrong time scale (index says 60 s, file says 10 min): no prefetch.
  { long long x = segIni + clusterRel[13];   // a byte no earlier case asked for
    mkvfix_adiantar(26.0, 600.0);
    usleep(1500000);
    ok(pedidosEm(x) == 0, "index times that do not match the duration: no prefetch"); }

  // A CDN that never answers the prefetch (accepts, no headers). The reopen's
  // seek elsewhere must not wait for it, and dropping it must free the way for
  // the next prefetch within the network loop's check (before: ~2 s and ~5 s,
  // the stall limit).
  { long long x1 = segIni + clusterRel[15], y = segIni + clusterRel[25];
    uint64_t t0; long dt;
    pendurar(x1);
    mkvfix_adiantar(31.0, 60.0);
    for (int i = 0; i < 60 && !pedidosEm(x1); i++) usleep(50000);
    mkvfix_adiantar_pressa();
    t0 = msAgora(); st = tvLer(local, y, y + 2 * MIB - 1, &certo); dt = (long)(msAgora() - t0) - 300;
    printf("  (seek elsewhere: %ld ms)\n", dt);
    ok(certo && dt < 1000, "silent CDN: the reopen's seek elsewhere does not wait for the prefetch"); }
  { long long x1 = segIni + clusterRel[15], x2 = segIni + clusterRel[11];
    int antes = pedidosEm(x1); uint64_t t0; long dt;
    mkvfix_adiantar(31.0, 60.0);
    for (int i = 0; i < 60 && pedidosEm(x1) == antes; i++) usleep(50000);
    mkvfix_adiantar_soltar("test");
    t0 = msAgora(); mkvfix_adiantar(22.0, 60.0);
    for (int i = 0; i < 300 && !pedidosEm(x2); i++) usleep(25000);
    dt = (long)(msAgora() - t0);
    printf("  (next prefetch reached the CDN after %ld ms)\n", dt);
    ok(pedidosEm(x2) == 1 && dt < 2500, "silent CDN: dropping the prefetch frees the way for the next one (no stall wait)");
    pendurar(-1);
    mkvfix_adiantar_soltar("test"); }

  mkvfix_parar();
  printf(falhas ? "mkvfix_adiantar: %d FAIL(S)\n" : "mkvfix_adiantar: ok\n", falhas);
  return falhas != 0;
}
