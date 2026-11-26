// tests/tbaudio.sh: original audio with TorBox burn-in (src/dts/tbaudio_tpk.c).
//   tbaudio puros <ts> <init.mp4> <seg.m4s> <stripped.ts>   pure helpers on real files
//   tbaudio sessao <tb m3u8> <orig> <modo> <ordinal> <flac> <urlfile>
//       opens a session, writes the player URL to <urlfile>, serves until killed.
#include "../src/dts/tbaudio_tpk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int falhas;
static void ok(const char *o, int c) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static unsigned char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb");
  unsigned char *b;
  long k;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); k = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)k + 1);
  if (b && fread(b, 1, (size_t)k, f) != (size_t)k) { free(b); b = NULL; }
  fclose(f);
  *n = (size_t)k;
  return b;
}

static int puros(const char *ts, const char *init, const char *seg, const char *semAudio) {
  char o[512];
  TbaPl pl;
  size_t n;
  unsigned char *b;
  double s;
  unsigned esc;
  unsigned long long t;
  char *e;
  ok("absolute stays", tba_url_absoluta("https://a.b/x/y.m3u8", "https://c.d/z.ts?t=1", o, sizeof o) && !strcmp(o, "https://c.d/z.ts?t=1"));
  ok("relative to the directory", tba_url_absoluta("https://a.b/x/y.m3u8?tok=9", "seg_1.ts?k=2", o, sizeof o) && !strcmp(o, "https://a.b/x/seg_1.ts?k=2"));
  ok("root relative", tba_url_absoluta("https://a.b:8/x/y.m3u8", "/s/1.ts", o, sizeof o) && !strcmp(o, "https://a.b:8/s/1.ts"));
  ok("scheme relative", tba_url_absoluta("https://a.b/x/y.m3u8", "//c.d/1.ts", o, sizeof o) && !strcmp(o, "https://c.d/1.ts"));
  ok("../ climbs one directory", tba_url_absoluta("https://a.b/x/y/z.m3u8", "../1.ts", o, sizeof o) && !strcmp(o, "https://a.b/x/1.ts"));
  ok("bare host", tba_url_absoluta("https://a.b", "1.ts", o, sizeof o) && !strcmp(o, "https://a.b/1.ts"));
  {
    const char *m = "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=900000,RESOLUTION=640x360\nlow/p.m3u8\n"
                    "#EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080\nhi/p.m3u8?x=1\n";
    ok("master: the best variant", tba_pl_ler(m, "https://t.b/s/1/playlist.m3u8", &pl) && pl.master &&
       !strcmp(pl.variante, "https://t.b/s/1/hi/p.m3u8?x=1") && pl.banda == 5000000 && !strcmp(pl.resolucao, "1920x1080"));
    tba_pl_liberar(&pl);
  }
  {
    const char *m = "#EXTM3U\r\n#EXT-X-VERSION:3\r\n#EXT-X-TARGETDURATION:5\r\n#EXTINF:5.000,\r\nseg0.ts\r\n"
                    "#EXTINF:5.000,\r\nseg1.ts\r\n#EXTINF:5.000,\r\nseg2.ts\r\n#EXT-X-ENDLIST\r\n";
    ok("media playlist (CRLF)", tba_pl_ler(m, "https://t.b/s/playlist.m3u8?tok=1", &pl) && !pl.master && pl.nseg == 3 &&
       pl.fim && !strcmp(pl.seg[2].uri, "https://t.b/s/seg2.ts") && pl.seg[1].dur == 5.0);
    ok("S3: 15 s listed, file 11.2 s: last 1.2 s", pl.nseg == 3 && tba_pl_ultimo(&pl, 11.2) > 1.19 && tba_pl_ultimo(&pl, 11.2) < 1.21);
    ok("S3: file longer: kept", tba_pl_ultimo(&pl, 16) == 5.0);
    ok("S3: overreport beyond the last segment: kept", tba_pl_ultimo(&pl, 9) == 5.0);
    e = tba_pl_espelho(m, "https://t.b/s/playlist.m3u8?tok=1", 1.2);
    ok("mirror: URIs absolute", e && strstr(e, "\nhttps://t.b/s/seg0.ts\n") && strstr(e, "\nhttps://t.b/s/seg2.ts\n"));
    ok("mirror: only the last EXTINF changes", e && strstr(e, "#EXTINF:5.000,\nhttps://t.b/s/seg1.ts") && strstr(e, "#EXTINF:1.200000,\nhttps://t.b/s/seg2.ts"));
    ok("mirror: tags kept", e && strstr(e, "#EXT-X-TARGETDURATION:5\n") && strstr(e, "#EXT-X-ENDLIST\n"));
    free(e);
    tba_pl_liberar(&pl);
  }
  {
    const char *m = "#EXTM3U\n#EXT-X-MAP:URI=\"init.mp4\"\n#EXTINF:4,\ns0.m4s\n";
    e = tba_pl_espelho(m, "https://t.b/a/p.m3u8", -1);
    ok("mirror: EXT-X-MAP URI absolute", e && strstr(e, "#EXT-X-MAP:URI=\"https://t.b/a/init.mp4\"\n"));
    ok("fMP4 playlist: map read", tba_pl_ler(m, "https://t.b/a/p.m3u8", &pl) && !strcmp(pl.mapa, "https://t.b/a/init.mp4"));
    free(e); tba_pl_liberar(&pl);
  }
  b = ler(ts, &n);
  ok("TS: first video PTS of the fixture (1.4 s, ffmpeg's default offset)", b && tba_ts_primeiro_pts(b, n, &s) && s > 1.39 && s < 1.45);
  printf("     (TS first PTS %.4f)\n", b ? s : -1.0);
  if (b) {
    unsigned char *o = malloc(n);
    size_t k = 0;
    int rem = 0;
    FILE *f;
    ok("TS strip: audio removed (1 stream), smaller, whole packets", o && tba_ts_sem_audio(b, n, o, &k, &rem) && rem == 1 && k < n && k % 188 == 0);
    if (o && (f = fopen(semAudio, "wb"))) { fwrite(o, 1, k, f); fclose(f); }
    ok("TS strip: nothing to strip in non-TS bytes", !tba_ts_sem_audio((const unsigned char *)"not a transport stream at all", 30, o, &k, &rem));
    free(o);
    { // A PMT whose program_info_length (0xFFF) runs past its section: refused,
      // nothing copied past the section buffer (ASan: SANITIZE=1).
      size_t tn = 188 * 30, q;
      unsigned char *ti = malloc(tn), *to = malloc(tn);
      static const unsigned char pat[] = { 0x47, 0x40, 0x00, 0x10, 0x00, 0x00, 0xB0, 0x0D, 0x00, 0x01, 0xC1, 0x00, 0x00, 0x00, 0x01, 0xF0, 0x00, 0, 0, 0, 0 };
      static const unsigned char pmt[] = { 0x47, 0x50, 0x00, 0x10, 0x00, 0x02, 0xB0, 0x17, 0x00, 0x01, 0xC1, 0x00, 0x00, 0xE1, 0x00,
                                           0xFF, 0xFF, 0x1B, 0xE1, 0x00, 0xF0, 0x00, 0x0F, 0xE1, 0x01, 0xF0, 0x00, 0, 0, 0, 0 };
      if (ti && to) {
        memset(ti, 0xFF, tn);
        for (q = 0; q < tn; q += 188) { ti[q] = 0x47; ti[q + 1] = 0x1F; ti[q + 2] = 0xFF; ti[q + 3] = 0x10; }
        memcpy(ti, pat, sizeof pat); memcpy(ti + 188, pmt, sizeof pmt);
        ok("TS strip: program info longer than the PMT section is refused", !tba_ts_sem_audio(ti, tn, to, &k, &rem));
      }
      free(ti); free(to);
    }
  }
  free(b);
  b = ler(init, &n);
  ok("fMP4 init: video timescale", b && tba_mp4_escala(b, n, &esc) && esc > 0);
  printf("     (timescale %u)\n", b ? esc : 0);
  free(b);
  b = ler(seg, &n);
  ok("fMP4 segment: tfdt", b && tba_mp4_tfdt(b, n, &t));
  printf("     (tfdt %llu)\n", b ? t : 0ULL);
  free(b);
  return falhas ? 1 : 0;
}

int main(int argc, char **argv) {
  if (argc == 6 && !strcmp(argv[1], "puros")) return puros(argv[2], argv[3], argv[4], argv[5]);
  if (argc == 7 && !strcmp(argv[1], "sessao")) {
    char url[256];
    FILE *f;
    if (!tba_abrir(argv[2], argv[3], "", atoi(argv[5]), atoi(argv[4]), url, sizeof url)) {
      fprintf(stderr, "tba_abrir failed (disponivel=%d)\n", tba_disponivel());
      return 1;
    }
    f = fopen(argv[6], "w");
    if (!f) return 1;
    fprintf(f, "%s\n", url);
    fclose(f);
    for (;;) { sleep(10); tba_log(); }
  }
  fprintf(stderr, "usage\n");
  return 2;
}
