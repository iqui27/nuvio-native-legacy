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

// #763: a entrada comprimida pode passar de 8 MiB se a SAIDA restaurada couber
// no teto. A base aceitava; a branch rejeitava a entrada. Saida = 8 MiB exatos,
// entrada zlib ~8.39 MiB (dados incompressiveis + overhead).
static void test_entrada_grande_saida_no_teto(void) {
  size_t outAlvo = PGS_MKV_BLOCO_MAX; // 8 MiB
  unsigned char *raw = malloc(outAlvo);
  assert(raw);
  // Padrao pseudo-aleatorio pouco compressivel: a entrada fica > 8 MiB.
  unsigned s = 12345;
  for (size_t i = 0; i < outAlvo; i++) { s = s * 1103515245u + 12345u; raw[i] = (unsigned char)(s >> 16); }
  uLong compN = compressBound((uLong)outAlvo);
  unsigned char *comp = malloc(compN);
  assert(comp);
  assert(compress2(comp, &compN, raw, (uLong)outAlvo, Z_DEFAULT_COMPRESSION) == Z_OK);
  assert(compN > PGS_MKV_BLOCO_MAX); // entrada maior que o teto de saida
  size_t gotN = 0;
  unsigned char *got = pgs_mkv_restaurar(PGS_MKV_CODIF_ZLIB, NULL, 0, comp, compN, &gotN);
  assert(got && gotN == outAlvo);
  assert(!memcmp(got, raw, outAlvo));
  free(got); free(comp); free(raw);
}

int main(void) {
  test_zlib();
  test_strip();
  test_fail();
  test_entrada_grande_saida_no_teto();
  puts("pgs_mkv_restore: ok");
  return 0;
}
