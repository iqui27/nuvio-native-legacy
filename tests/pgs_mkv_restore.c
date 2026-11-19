#include "pgs_mkv_restore.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void test_zlib(void) {
  static const unsigned char raw[] = {
      0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00
  };
  unsigned char comp[64];
  uLong compN = compressBound(sizeof raw);
  assert(compress2(comp, &compN, raw, sizeof raw, Z_DEFAULT_COMPRESSION) == Z_OK);
  size_t outN = 0;
  unsigned char *out = pgs_mkv_restaurar(PGS_MKV_CODIF_ZLIB, NULL, 0, comp, compN, &outN);
  assert(out);
  assert(outN == sizeof raw);
  assert(!memcmp(out, raw, sizeof raw));
  free(out);
}

static void test_strip(void) {
  static const unsigned char pre[] = {0x16};
  static const unsigned char body[] = {0x50, 0x01};
  size_t outN = 0;
  unsigned char *out =
      pgs_mkv_restaurar(PGS_MKV_CODIF_TIRA, pre, sizeof pre, body, sizeof body, &outN);
  assert(out && outN == sizeof pre + sizeof body);
  assert(out[0] == 0x16 && out[1] == 0x50);
  free(out);
}

static void test_fail(void) {
  static const unsigned char bad[] = {0x00, 0x01};
  size_t outN = 99;
  assert(!pgs_mkv_restaurar(PGS_MKV_CODIF_ZLIB, NULL, 0, bad, sizeof bad, &outN));
}

int main(void) {
  test_zlib();
  test_strip();
  test_fail();
  puts("pgs_mkv_restore: ok");
  return 0;
}
