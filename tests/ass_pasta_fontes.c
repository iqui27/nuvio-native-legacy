// A PASTA DE FONTES DO SISTEMA NAO E SO DE FONTES.
//
// Log da C9 (1.4.6-dev): "[libass] Error opening memory font 'arib_mrg_v5-10.bin'"
// e parecidas, uma por arquivo de firmware em /usr/share/fonts. O app passava a
// pasta inteira ao libass (ass_set_fonts_dir), que le TODO arquivo como fonte.
// Aqui: uma pasta com uma .ttf de verdade, uma .bin de firmware, um .txt, um
// arquivo oculto e uma subpasta; so a .ttf pode chegar ao libass.
//
//   bash tests/ass_pasta_fontes.sh
#include "../src/assrender.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int vistas;
static char ultimo[128];
static void cb(const char *nome, const void *d, size_t n, void *u) {
  (void)u;
  assert(assrender_bytes_sao_fonte(d, n));
  snprintf(ultimo, sizeof ultimo, "%s", nome);
  vistas++;
}

static void copiar(const char *de, const char *para) {
  FILE *a = fopen(de, "rb"), *b = fopen(para, "wb");
  char buf[65536]; size_t n;
  assert(a && b);
  while ((n = fread(buf, 1, sizeof buf, a)) > 0) assert(fwrite(buf, 1, n, b) == n);
  fclose(a); fclose(b);
}
static void escrever(const char *c, const void *d, size_t n) {
  FILE *f = fopen(c, "wb"); assert(f); assert(fwrite(d, 1, n, f) == n); fclose(f);
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  char dir[512], c[640];
  int ign = -1, n;
  static const unsigned char arib[] = { 0x41, 0x52, 0x49, 0x42, 0x00, 0x05, 0x10, 0xff };
  snprintf(dir, sizeof dir, "%snuvio-ass-pasta-%d", tmp && *tmp ? tmp : "/tmp/", (int)getpid());
  assert(mkdir(dir, 0700) == 0);
  snprintf(c, sizeof c, "%s/InterDisplay-Regular.ttf", dir);
  copiar("deploy/app/fonts/InterDisplay-Regular.ttf", c);
  snprintf(c, sizeof c, "%s/arib_mrg_v5-10.bin", dir);  escrever(c, arib, sizeof arib);
  snprintf(c, sizeof c, "%s/fonts.conf.txt", dir);      escrever(c, "<fontconfig/>", 13);
  snprintf(c, sizeof c, "%s/.oculta.ttf", dir);          escrever(c, "\0\1\0\0xxxx", 8);
  snprintf(c, sizeof c, "%s/sub", dir);                  assert(mkdir(c, 0700) == 0);

  assert(assrender_bytes_sao_fonte("OTTO", 4) && assrender_bytes_sao_fonte("ttcf", 4));
  assert(!assrender_bytes_sao_fonte(arib, sizeof arib) && !assrender_bytes_sao_fonte("\0\1", 2));

  n = assrender_ler_pasta_fontes(dir, cb, NULL, &ign);
  printf("  %d fonte(s), %d ignorado(s), ultima=%s\n", n, ign, ultimo);
  assert(n == 1 && vistas == 1 && !strcmp(ultimo, "InterDisplay-Regular.ttf"));
  assert(ign == 2);   // a .bin e o .txt; a oculta e a subpasta nem sao olhadas
  assert(assrender_ler_pasta_fontes("/nao/existe", cb, NULL, &ign) == 0 && ign == 0);

  snprintf(c, sizeof c, "rm -rf '%s'", dir);
  if (system(c) != 0) return 1;
  puts("ass_pasta_fontes: tudo ok");
  return 0;
}
