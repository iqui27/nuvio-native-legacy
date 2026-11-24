// A Tracks element of 4 MB or more (log 12, 23/11/2026, Fate/strange Fake PMR
// BD Remux: 4.4 MB of Tracks, fonts in CodecPrivate). One Range of that size
// is refused by rede_baixar_trecho64_vigiado (< 4 MB only), so the probe left
// the ASS tracks to the TV ("nothing hidden: opening the file directly").
// The probe now reads the Tracks in pieces. End to end: the real local server
// (mkvfix.c + rede.c) against tests/mkvfix_adiantar_server.py.
#include "mkvfix.h"
#include "rede.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  (void)usenet; (void)item; (void)file; if (n) dst[0] = 0; return 0;
}

static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok  " : "FAIL", m); fflush(stdout); if (!c) falhas++; }

// EBML element with an 8-byte size (any length).
static long el8(unsigned char *o, uint32_t id, const unsigned char *p, long n) {
  long k = 0;
  int w = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  for (int i = w - 1; i >= 0; i--) o[k++] = (unsigned char)(id >> (8 * i));
  o[k++] = 0x01;
  for (int i = 6; i >= 0; i--) o[k++] = (unsigned char)((unsigned long long)n >> (8 * i));
  if (p) memcpy(o + k, p, (size_t)n);
  return k + n;
}
static long seek(unsigned char *o, uint32_t alvo, uint32_t pos) {
  unsigned char a[4] = { alvo >> 24, alvo >> 16, alvo >> 8, alvo }, b[4] = { pos >> 24, pos >> 16, pos >> 8, pos };
  unsigned char v[64]; long n = 0;
  n += el8(v + n, 0x53AB, a, 4);
  n += el8(v + n, 0x53AC, b, 4);
  return el8(o, 0x4DBB, v, n);
}
static long entrada(unsigned char *o, unsigned num, unsigned tipo, const char *codec, long privN) {
  unsigned char *v = malloc((size_t)privN + 256), nb = (unsigned char)num, tb = (unsigned char)tipo;
  long n = 0, r;
  n += el8(v + n, 0xD7, &nb, 1);
  n += el8(v + n, 0x83, &tb, 1);
  n += el8(v + n, 0x86, (const unsigned char *)codec, (long)strlen(codec));
  if (privN > 0) { n += el8(v + n, 0x63A2, NULL, privN); memset(v + n - privN, 'f', (size_t)privN); }
  r = el8(o, 0xAE, v, n);
  free(v);
  return r;
}

int main(int argc, char **argv) {
  const long PRIV = 4400L * 1024;   // fonts in CodecPrivate, like the PMR release
  unsigned char *head = malloc((size_t)PRIV + 65536), *trk = malloc((size_t)PRIV + 4096), sh[128];
  long k = 0, nt = 0, segIni, shIni, trkRel;
  char orig[256], p[512];
  FILE *f;
  if (argc < 3) return 2;
  snprintf(orig, sizeof orig, "http://127.0.0.1:%s/f.mkv", argv[2]);

  nt += entrada(trk + nt, 1, 1, "V_MPEG4/ISO/AVC", 0);
  nt += entrada(trk + nt, 2, 0x11, "S_TEXT/ASS", PRIV);
  nt += entrada(trk + nt, 3, 0x11, "S_TEXT/ASS", 64);

  k += el8(head, 0x1A45DFA3, (const unsigned char *)"\x42\x82\x88matroska", 11);
  head[k++] = 0x18; head[k++] = 0x53; head[k++] = 0x80; head[k++] = 0x67; head[k++] = 0x01;
  memset(head + k, 0xFF, 7); k += 7;
  segIni = k;
  shIni = k;
  // SeekHead: Tracks right after it (size known after), Cues far in the file.
  { long m = 0, shn;
    trkRel = 200;   // SeekHead + Void fill the first 200 bytes of the Segment
    m += seek(sh + m, 0x1654AE6Bu, (uint32_t)trkRel);
    m += seek(sh + m, 0x1C53BB6Bu, (uint32_t)(60L * 1024 * 1024));
    shn = el8(head + k, 0x114D9B74u, sh, m); k += shn;
    { long vz = trkRel - (k - shIni) - 9; k += el8(head + k, 0xEC, NULL, vz); memset(head + k - vz, 0, (size_t)vz); } }
  if (k - segIni != trkRel) { printf("FAIL layout (%ld)\n", k - segIni); return 1; }
  k += el8(head + k, 0x1654AE6Bu, trk, nt);
  snprintf(p, sizeof p, "%s/head.bin", argv[1]);
  f = fopen(p, "wb"); fwrite(head, 1, (size_t)k, f); fclose(f);
  printf("  (Tracks: %ld bytes)\n", nt);

  mkvfix_sondar(orig, "", NULL);
  for (int i = 0; i < 300 && mkvfix_estado() == MKVFIX_SONDANDO; i++) usleep(50000);
  ok(mkvfix_estado() != MKVFIX_SONDANDO, "probe finished");
  ok(mkvfix_ocultas() == 2, "a 4.4 MB Tracks is read (in pieces): both ASS tracks hidden from the TV");
  free(head); free(trk);
  printf(falhas ? "mkvfix_tracks_grande: %d FAIL\n" : "mkvfix_tracks_grande: ok\n", falhas);
  return falhas != 0;
}
