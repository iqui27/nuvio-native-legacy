#ifndef NV_CATSNAPDISK_H
#define NV_CATSNAPDISK_H
#include "dados.h"
#include "catsnap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

static inline long long catsnap_refresh_stamp(int profile) {
  char file[64];
  snprintf(file, sizeof file, "catalog-refresh-p%d.txt", profile);
  char *body = dados_ler(file);
  long long stamp = body ? strtoll(body, NULL, 10) : 0;
  free(body);
  return stamp;
}

static inline void catsnap_request_refresh(int profile) {
  char file[64], body[64];
  long long stamp = catsnap_refresh_stamp(profile);
  snprintf(file, sizeof file, "catalog-refresh-p%d.txt", profile);
  snprintf(body, sizeof body, "%lld", stamp < 0 || stamp == LLONG_MAX ? 1 : stamp + 1);
  dados_gravar(file, body);
}

static inline char *catsnap_read(const char *file, long long refresh, int *fresh) {
  char *body = dados_ler(file);
  unsigned version = 0;
  long long fetched = 0, stamp = -1;
  int offset = 0;
  *fresh = 0;
  if (!body) return NULL;
  if (sscanf(body, "NVCS %u %lld %lld\n%n", &version, &fetched, &stamp, &offset) != 3 || !offset) {
    free(body); return NULL;
  }
  if (version != CATSNAP_VERSION) { free(body); return NULL; }
  *fresh = !catsnap_needs_fetch(1, version, 1, stamp != refresh, fetched, time(NULL));
  memmove(body, body + offset, strlen(body + offset) + 1);
  return body;
}

static inline int catsnap_write(const char *file, const char *body, long long refresh) {
  size_t size = strlen(body) + 128;
  char *saved = malloc(size);
  if (!saved) return 0;
  snprintf(saved, size, "NVCS %u %lld %lld\n%s", CATSNAP_VERSION, (long long)time(NULL), refresh, body);
  int ok = dados_gravar_leve(file, saved);
  free(saved);
  return ok;
}
#endif
