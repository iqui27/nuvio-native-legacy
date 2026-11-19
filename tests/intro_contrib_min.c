// #494: exercita intro_contrib_registrar sem linkar intro.c inteiro.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

char *dados_caminho(char *d, unsigned n, const char *name) {
  snprintf(d, n, "%s/%s", getenv("NUVIO_DADOS"), name);
  return d;
}

void intro_contrib_registrar(const char *imdb, int tipo, int iniMs, int fimMs) {
  FILE *f;
  char path[600];
  if (!imdb || !imdb[0] || iniMs < 0 || fimMs <= iniMs) return;
  if (!dados_caminho(path, sizeof path, "skip-contrib-local.jsonl")) return;
  f = fopen(path, "a");
  if (!f) return;
  fprintf(f, "{\"imdb\":\"%s\",\"tipo\":%d,\"ini\":%d,\"fim\":%d}\n",
          imdb, tipo, iniMs, fimMs);
  fclose(f);
  chmod(path, 0600);
}

int main(void) {
  char dir[128];
  snprintf(dir, sizeof dir, "/tmp/nuvio-contrib-XXXXXX");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  intro_contrib_registrar("tt123", 1, 0, 90000);
  char path[256];
  snprintf(path, sizeof path, "%s/skip-contrib-local.jsonl", dir);
  FILE *f = fopen(path, "r");
  assert(f);
  char line[256];
  assert(fgets(line, sizeof line, f));
  assert(strstr(line, "tt123") && strstr(line, "90000"));
  fclose(f);
  puts("ok intro_contrib");
  return 0;
}
