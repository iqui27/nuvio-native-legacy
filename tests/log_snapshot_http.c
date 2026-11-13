// Same runtime HTTP assertion on the pre-port server and current server.
// The baseline mode runs the pre-port server's real LAN page, so a missing
// snapshot fails on HTTP content, rather than at link time.
#define _POSIX_C_SOURCE 200809L
#include "celular.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
const char *i18n(const char *s) { return s; }
int ajustes_idioma(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1; }
int ajustes_tinta_foco(void) { return 18; }
int main(void) {
  char path[1024], cmd[2048], body[8192];
  snprintf(path, sizeof path, "%s/snapshot-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  int fd = mkstemp(path); assert(fd >= 0);
  const char *snapshot = "[app] FROZEN_HTTP_SNAPSHOT\n";
  assert(write(fd, snapshot, strlen(snapshot)) == (ssize_t)strlen(snapshot)); close(fd);
#ifdef PRE_PORT_SERVER
  assert(celular_abrir("Log"));
#else
  assert(celular_abrir_relatorio(path));
#endif
  snprintf(cmd, sizeof cmd, "curl --silent --max-time 5 'http://127.0.0.1:%d/%s?text=1'", celular_porta(), strrchr(celular_url(), '/') + 1);
  FILE *r = popen(cmd, "r"); assert(r);
  size_t n = fread(body, 1, sizeof body - 1, r); body[n] = 0;
  int rc = pclose(r); celular_fechar(); unlink(path);
  if (rc || !strstr(body, "FROZEN_HTTP_SNAPSHOT")) {
    fputs("FAIL: HTTP endpoint did not serve the frozen log snapshot\n", stderr); return 1;
  }
  puts("PASS: real HTTP serves the frozen log snapshot");
  return 0;
}
