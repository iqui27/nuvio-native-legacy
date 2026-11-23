#include "pgs_mkv_restore.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

unsigned char *pgs_mkv_restaurar(int codif, const unsigned char *tira, size_t tira_n,
                                 const unsigned char *bloco, size_t bloco_n,
                                 size_t *saida) {
  unsigned char *o;
  if (!saida) return NULL;
  if (codif == PGS_MKV_CODIF_TIRA) {
    if (bloco_n > PGS_MKV_BLOCO_MAX - tira_n) return NULL;
    *saida = tira_n + bloco_n;
    o = malloc(*saida ? *saida : 1);
    if (!o) return NULL;
    if (tira_n) memcpy(o, tira, tira_n);
    memcpy(o + tira_n, bloco, bloco_n);
    return o;
  }
  if (codif != PGS_MKV_CODIF_ZLIB) return NULL;
  z_stream z;
  size_t cap = 256u << 10;
  int r;
  memset(&z, 0, sizeof z);
  o = malloc(cap);
  if (!o) return NULL;
  if (inflateInit(&z) != Z_OK) { free(o); return NULL; }
  z.next_in = (Bytef *)bloco;
  z.avail_in = (uInt)bloco_n;
  z.next_out = o;
  z.avail_out = (uInt)cap;
  while ((r = inflate(&z, Z_NO_FLUSH)) == Z_OK && z.avail_out == 0 && cap < PGS_MKV_BLOCO_MAX) {
    unsigned char *g = realloc(o, cap * 2);
    if (!g) { r = Z_MEM_ERROR; break; }
    o = g;
    z.next_out = o + cap;
    z.avail_out = (uInt)cap;
    cap *= 2;
  }
  *saida = z.total_out;
  inflateEnd(&z);
  if (r != Z_STREAM_END) { free(o); return NULL; }
  return o;
}
