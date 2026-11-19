#include "../src/intro.c"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *dados_caminho(char *d, unsigned n, const char *name) {
  snprintf(d, n, "%s/%s", getenv("NUVIO_DADOS"), name);
  return d;
}

int main(void) {
  char dir[128];
  snprintf(dir, sizeof dir, "/tmp/nuvio-contrib-XXXXXX");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  intro_contrib_registrar("tt123", INTRO_ABERTURA, 0, 90000);
  char path[256];
  snprintf(path, sizeof path, "%s/skip-contrib-local.jsonl", dir);
  FILE *f = fopen(path, "r");
  assert(f);
  char line[256];
  assert(fgets(line, sizeof line, f));
  assert(strstr(line, "tt123"));
  fclose(f);
  puts("ok intro_contrib");
  return 0;
}
