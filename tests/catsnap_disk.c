#include "catsnapdisk.h"
#include <assert.h>

static char *stored;
static int fail_write;
char *dados_ler(const char *file) { return !strcmp(file, "snapshot") && stored ? strdup(stored) : NULL; }
int dados_gravar_leve(const char *file, const char *body) {
  assert(!strcmp(file, "snapshot"));
  if (fail_write) return 0;
  free(stored); stored = strdup(body); return stored != NULL;
}
int main(void) {
  int fresh;
  assert(!catsnap_read("snapshot", 0, &fresh) && !fresh);
  assert(catsnap_write("snapshot", "[]", 3));
  char *body = catsnap_read("snapshot", 3, &fresh);
  assert(body && fresh && !strcmp(body, "[]")); free(body);
  body = catsnap_read("snapshot", 4, &fresh);
  assert(body && !fresh && !strcmp(body, "[]")); free(body);
  fail_write = 1;
  assert(!catsnap_write("snapshot", "[1]", 4));
  body = catsnap_read("snapshot", 3, &fresh);
  assert(body && fresh && !strcmp(body, "[]")); free(body);
  assert(!catsnap_read("other-identity", 3, &fresh) && !fresh);
  free(stored); stored = strdup("NVCS 999 1000000 0\n[]");
  assert(!catsnap_read("snapshot", 0, &fresh) && !fresh);
  free(stored); stored = strdup("truncated");
  assert(!catsnap_read("snapshot", 0, &fresh) && !fresh);
  free(stored);
  puts("catsnap disk: empty, identity, refresh, failed write retention, version, corrupt OK");
  return 0;
}
