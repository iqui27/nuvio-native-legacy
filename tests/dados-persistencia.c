#include <stdlib.h>
#include <string.h>
static char *fixture_getenv(const char *name) {
  return !strcmp(name, "HOME") || !strcmp(name, "NUVIO_DADOS") ? "/dev/null" : NULL;
}
#define getenv fixture_getenv
#include "../src/dados.c"
#undef getenv
#include <stdio.h>

int main(void) {
  dados_iniciar("/dev/null");
  if (dados_persistente()) {
    fprintf(stderr, "dados_persistente reported true without a writable directory: %s\n", dados_dir());
    return 1;
  }
  if (dados_dir()[0] != '\0') {
    fprintf(stderr, "unexpected writable directory selected: %s\n", dados_dir());
    return 1;
  }
  if (dados_gravar("fixture.txt", "must not be written")) {
    fprintf(stderr, "write unexpectedly succeeded without a configured directory\n");
    return 1;
  }
  puts("PASS native persistence remains false without a writable directory");
  return 0;
}
