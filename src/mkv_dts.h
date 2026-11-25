#ifndef NV_MKV_DTS_H
#define NV_MKV_DTS_H
/* DTS kind of a Matroska audio track, from its first frames in the bytes the
 * header probe already read (no extra request). A_DTS marks plain DTS and
 * DTS-HD alike and the TV reports both as "dts", so the audio menu said
 * "DTS 5.1" for both (tester, 15/11). A DTS-HD frame carries an extension
 * substream (sync 0x64582025) after the core; a lossless one (Master Audio)
 * an XLL asset in it (sync 0x41A29547). DTS:X is Master Audio here. */
#include <stddef.h>
#include <string.h>

enum { NV_DTS_KIND_UNKNOWN = 0, NV_DTS_KIND_CORE, NV_DTS_KIND_HD, NV_DTS_KIND_MA };

static inline int nv_mkv_dts_vint(const unsigned char *p, long n, int id, long long *v) {
  int len = 1;
  unsigned char mask = 0x80;
  unsigned long long x;
  if (n < 1) return 0;
  while (len <= 8 && !(p[0] & mask)) { mask >>= 1; len++; }
  if (len > 8 || len > n) return 0;
  x = id ? p[0] : (unsigned long long)(p[0] & (mask - 1));
  for (int i = 1; i < len; i++) x = (x << 8) | p[i];
  if (!id && x == ((1ULL << (7 * len)) - 1)) *v = -1;   /* unknown size */
  else *v = (long long)x;
  return len;
}
static inline long nv_mkv_dts_find(const unsigned char *p, long n, long from,
                                   unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
  for (long i = from; i + 4 <= n; i++)
    if (p[i] == a && p[i + 1] == b && p[i + 2] == c && p[i + 3] == d) return i;
  return -1;
}
/* One block's frame data: the kind it shows, or UNKNOWN when it shows none
 * (a frame cut by the end of the probe can only prove HD, never core only). */
static inline int nv_mkv_dts_frame(const unsigned char *p, long n, int complete) {
  long ext = nv_mkv_dts_find(p, n, 0, 0x64, 0x58, 0x20, 0x25);
  if (ext >= 0)
    return nv_mkv_dts_find(p, n, ext + 4, 0x41, 0xA2, 0x95, 0x47) >= 0 ? NV_DTS_KIND_MA
         : complete ? NV_DTS_KIND_HD : NV_DTS_KIND_UNKNOWN;
  if (complete && nv_mkv_dts_find(p, n, 0, 0x7F, 0xFE, 0x80, 0x01) >= 0) return NV_DTS_KIND_CORE;
  return NV_DTS_KIND_UNKNOWN;
}
/* Elements from p[0] (a Cluster's or BlockGroup's children, or the top
 * level); kinds[i] for the track numbers numbers[i]. Returns how many are
 * still unknown. */
static inline int nv_mkv_dts_scan_level(const unsigned char *p, long n, const int *numbers,
                                        int *kinds, int count, int depth) {
  long o = 0;
  int left = 0;
  for (int i = 0; i < count; i++) left += kinds[i] == NV_DTS_KIND_UNKNOWN;
  while (left && o < n && depth < 4) {
    long long id, size;
    int ui = nv_mkv_dts_vint(p + o, n - o, 1, &id), us;
    if (!ui) break;
    us = nv_mkv_dts_vint(p + o + ui, n - o - ui, 0, &size);
    if (!us) break;
    o += ui + us;
    long avail = size < 0 || size > n - o ? n - o : (long)size;
    if (id == 0x18538067LL) continue;                              /* Segment: its children */
    if (id == 0x1F43B675LL || id == 0xA0LL) {                      /* Cluster, BlockGroup */
      left = nv_mkv_dts_scan_level(p + o, avail, numbers, kinds, count, depth + 1);
    } else if (id == 0xA3LL || id == 0xA1LL) {                     /* SimpleBlock, Block */
      long long track;
      int ut = nv_mkv_dts_vint(p + o, avail, 0, &track);
      if (ut && avail >= ut + 3) {
        for (int i = 0; i < count; i++)
          if (numbers[i] == track && kinds[i] == NV_DTS_KIND_UNKNOWN) {
            kinds[i] = nv_mkv_dts_frame(p + o + ut + 3, avail - ut - 3, size >= 0 && size <= n - o);
            if (kinds[i] != NV_DTS_KIND_UNKNOWN) left--;
          }
      }
    }
    if (size < 0 || size > n - o) break;
    o += (long)size;
  }
  return left;
}
/* The kind a track's name states ("DTS-HD MA 5.1", "DTS-HD Master Audio",
 * "DTS:X", "DTS-HD HRA"), for a track whose frames were not in the probe.
 * Separators and case are ignored; a name that only says "DTS" states
 * nothing (it is how plain and HD tracks alike are often named). */
static inline int nv_mkv_dts_nome(const char *nome) {
  char c[64];
  size_t n = 0;
  if (!nome) return NV_DTS_KIND_UNKNOWN;
  for (; *nome && n + 1 < sizeof c; nome++) {
    char ch = *nome;
    if (ch == ' ' || ch == '-' || ch == '_' || ch == '.' || ch == ':' || ch == '/') continue;
    c[n++] = (char)(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
  }
  c[n] = 0;
  if (strstr(c, "hdma") || strstr(c, "masteraudio") || strstr(c, "dtsx")) return NV_DTS_KIND_MA;
  if (strstr(c, "dtshd") || strstr(c, "highresolution")) return NV_DTS_KIND_HD;
  return NV_DTS_KIND_UNKNOWN;
}
static inline void nv_mkv_dts_scan(const unsigned char *p, long n, const int *numbers,
                                   int *kinds, int count) {
  if (!p || n < 4 || count < 1) return;
  nv_mkv_dts_scan_level(p, n, numbers, kinds, count, 0);
}
#endif
