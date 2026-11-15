#include "mkv_dts.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* A tiny Matroska: EBML header, Segment of unknown size, a Tracks stand-in,
 * one Cluster with SimpleBlocks and a BlockGroup. */
typedef struct { unsigned char b[4096]; long n; } Buf;
static void put(Buf *d, const void *p, long n) { memcpy(d->b + d->n, p, (size_t)n); d->n += n; }
static void elem(Buf *d, const unsigned char *id, int idn, const Buf *data) {
  unsigned char sz[2] = {(unsigned char)(0x40 | (data->n >> 8)), (unsigned char)data->n};   /* 2-byte size */
  put(d, id, idn); put(d, sz, 2); put(d, data->b, data->n);
}
static void frame(Buf *f, int track, int ext, int xll) {
  static const unsigned char core[] = {0x7F, 0xFE, 0x80, 0x01}, sub[] = {0x64, 0x58, 0x20, 0x25},
                             x[] = {0x41, 0xA2, 0x95, 0x47};
  unsigned char head[4] = {(unsigned char)(0x80 | track), 0, 0, 0x80}, fill[40];
  for (int i = 0; i < 40; i++) fill[i] = (unsigned char)(i * 7 + 2);
  f->n = 0;
  put(f, head, 4); put(f, core, 4); put(f, fill, 40);
  if (ext) { put(f, sub, 4); put(f, fill, 20); }
  if (xll) put(f, x, 4);
}
int main(void) {
  static const unsigned char ebml[] = {0x1A, 0x45, 0xDF, 0xA3},
      seg[] = {0x18, 0x53, 0x80, 0x67, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
      tracks[] = {0x16, 0x54, 0xAE, 0x6B}, cluster[] = {0x1F, 0x43, 0xB6, 0x75},
      sb[] = {0xA3}, bg[] = {0xA0}, blk[] = {0xA1};
  static Buf file, body, f, group, zero;
  long cluster_at, second_block_at;
  zero.n = 16;
  elem(&file, ebml, 4, &zero);
  put(&file, seg, sizeof seg);
  zero.n = 64;
  elem(&file, tracks, 4, &zero);
  frame(&f, 2, 1, 1); elem(&body, sb, 1, &f);            /* track 2: Master Audio */
  second_block_at = body.n;
  frame(&f, 3, 1, 0); elem(&body, sb, 1, &f);            /* track 3: DTS-HD (HRA) */
  frame(&f, 4, 0, 0); elem(&group, blk, 1, &f);          /* track 4: plain DTS, in a BlockGroup */
  elem(&body, bg, 1, &group);
  cluster_at = file.n;
  elem(&file, cluster, 4, &body);
  { int nums[4] = {2, 3, 4, 5}, kinds[4] = {0};
    nv_mkv_dts_scan(file.b, file.n, nums, kinds, 4);
    assert(kinds[0] == NV_DTS_KIND_MA && kinds[1] == NV_DTS_KIND_HD && kinds[2] == NV_DTS_KIND_CORE &&
           kinds[3] == NV_DTS_KIND_UNKNOWN); }
  /* The probe ends inside track 3's frame, before its substream: not taken
   * for plain DTS. */
  { int nums[2] = {2, 3}, kinds[2] = {0};
    nv_mkv_dts_scan(file.b, cluster_at + 6 + second_block_at + 3 + 30, nums, kinds, 2);
    assert(kinds[0] == NV_DTS_KIND_MA && kinds[1] == NV_DTS_KIND_UNKNOWN); }
  /* No Cluster in the probe: nothing known. */
  { int nums[1] = {2}, kinds[1] = {0};
    nv_mkv_dts_scan(file.b, cluster_at, nums, kinds, 1);
    assert(kinds[0] == NV_DTS_KIND_UNKNOWN); }
  puts("mkv_dts: ok");
  return 0;
}
