/* Decompresses zlib-compressed Matroska PGS blocks before Nuvio's PGS decoder.
 * Installed into the v2.0.8 ARM binary by patch.py; see README.md there. */
#include <stddef.h>
#include <zlib.h>

int pgs_orig(void *d, const unsigned char *buf, size_t n, void *q);
void *malloc(size_t);
void free(void *);
int printf(const char *, ...);

/* pgs.c caps one object's RLE at 4 MiB; a display set never needs more than this. */
#define PGS_ZLIB_MAX (8u << 20)

int pgs_zlib_hook(void *d, const unsigned char *buf, size_t n, void *q) {
  z_stream z;
  unsigned char *out;
  unsigned i;
  int r, ret;
  /* 0x78 is not a PGS segment type, so this cannot match an uncompressed block. */
  if (n < 2 || buf[0] != 0x78 ||
      (buf[1] != 0x01 && buf[1] != 0x5e && buf[1] != 0x9c && buf[1] != 0xda))
    return pgs_orig(d, buf, n, q);
  out = malloc(PGS_ZLIB_MAX);
  if (!out) { printf("[pgs-zlib] sem memoria\n"); return pgs_orig(d, buf, n, q); }
  for (i = 0; i < sizeof z; i++) ((volatile unsigned char *)&z)[i] = 0;
  r = inflateInit2_(&z, 15, ZLIB_VERSION, (int)sizeof z);
  if (r != Z_OK) { printf("[pgs-zlib] init falhou %d\n", r); free(out); return pgs_orig(d, buf, n, q); }
  z.next_in = (unsigned char *)buf; z.avail_in = (unsigned)n;
  z.next_out = out; z.avail_out = PGS_ZLIB_MAX;
  r = inflate(&z, Z_FINISH);
  inflateEnd(&z);
  if (r != Z_STREAM_END) {
    printf("[pgs-zlib] inflate falhou %d (%u bytes)\n", r, (unsigned)n);
    free(out);
    return pgs_orig(d, buf, n, q);
  }
  ret = pgs_orig(d, out, z.total_out, q);
  printf("[pgs-zlib] %u -> %u bytes, decode=%d\n", (unsigned)n, (unsigned)z.total_out, ret);
  free(out);
  return ret;
}
