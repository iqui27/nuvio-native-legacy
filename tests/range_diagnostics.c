/* Range diagnostics (Matis94 C3): bounded, redacted [range-diag] records for
 * chunk and stream range requests — redirects, HTTP status, Retry-After and
 * server classification — with URLs/secrets never reaching the log.
 * Adapted to this tree's rede API (vigiado_retry callback, fluxo64 out-param). */
#define _POSIX_C_SOURCE 200809L
#include "rede.h"
#include "logexport.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int port;
static void chunk(const char *path, int expected) {
  char url[512]; long size = -1; int64_t total = -1; int status = -1;
  snprintf(url, sizeof url, "http://127.0.0.1:%d%s?hidden-query-secret", port, path);
  char *body = rede_baixar_trecho64_cab(url, "Authorization: Bearer hidden-request-secret", 0, 31,
                                     &size, &total, &status, NULL);
  assert(status == expected);
  if (expected == 206) {
    assert(body && size == 32 && total == 32);
    for (int i = 0; i < 32; i++) assert((unsigned char)body[i] == i);
  } else assert(!body && size == 0 && total == -1);
  free(body);
}
static int sink(void *opaque, const char *body, size_t n) {
  size_t *bytes = opaque;
  for (size_t i = 0; i < n; i++) assert((unsigned char)body[i] == *bytes + i);
  *bytes += n; return 1;
}
static void *parallel(void *unused) {
  (void)unused; chunk("/diag-parallel", 429); return NULL;
}
static int last_retry_seconds;
/* The callback fires on every 429 hop: seconds >= 0 when Retry-After was
 * present (0 when malformed), -1 when the final response carried none —
 * matching the old int* out-param semantics. */
static void retry_cb(void *opaque, int seconds) {
  int *out = opaque; *out = seconds;
}
int main(int argc, char **argv) {
  assert(argc == 2 || argc == 3);
  if (argc == 3) {
    char path[256], line[4096];
    assert(!strcmp(argv[1], "--export") && logexport_create(argv[2], path, sizeof path));
    FILE *f = fopen(path, "r"); assert(f);
    while (fgets(line, sizeof line, f)) fputs(line, stdout);
    fclose(f); unlink(path); return 0;
  }
  port = atoi(argv[1]);
  chunk("/diag-limit", 429);
  chunk("/diag-date", 429);
  chunk("/diag-invalid", 429);
  chunk("/diag-redirect", 429);
  char url[256]; int64_t total = -1, received = -1; int status = -1; size_t bytes = 0;
  snprintf(url, sizeof url, "http://127.0.0.1:%d/diag-limit?hidden-query-secret", port);
  assert(rede_fluxo64(url, "", 0, sink, &bytes, &total, &status, NULL, NULL, NULL, 1000, &received) == -1);
  assert(status == 429 && bytes == 0 && received == 0);
  snprintf(url, sizeof url, "http://127.0.0.1:%d/diag-stream?hidden-query-secret", port);
  assert(rede_fluxo64(url, "", 0, sink, &bytes, &total, &status, NULL, NULL, NULL, 1000, &received) == 0);
  assert(status == 200 && bytes == 32 && received == 32);
  for (int i = 0; i < 32; i++) chunk("/diag-ok", 206);
  pthread_t threads[4];
  for (int i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, parallel, NULL));
  for (int i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
  // The scheduling metadata is request-owned and resets after redirects.
  const char *paths[] = {"/diag-limit", "/diag-redirect", "/diag-invalid"};
  const int delays[] = {7, -1, 0};
  for (int i = 0; i < 3; i++) {
    long size = -1; int stalled = 0; last_retry_seconds = -1;
    snprintf(url, sizeof url, "http://127.0.0.1:%d%s", port, paths[i]);
    char *body = rede_baixar_trecho64_vigiado_retry(url, "", 0, 31, &size, &total, &status,
      NULL, NULL, NULL, NULL, 0, &stalled, retry_cb, &last_retry_seconds);
    assert(!body && status == 429 && size == 0 && last_retry_seconds == delays[i]);
    free(body);
  }
  puts("PASS: range diagnostics preserve HTTP results, bytes and parallel requests");
  return 0;
}
