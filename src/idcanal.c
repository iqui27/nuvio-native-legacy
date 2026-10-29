#include "idcanal.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREFIXO "~c:"
#define TAB_MAX 8192

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static struct { uint64_t h; char *id; } tab[TAB_MAX];
static int nTab;

static uint64_t fnv1a(const char *s) {
  uint64_t h = 1469598103934665603ULL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

int idcanal_curto(const char *id, char *dst, size_t n) {
  size_t len;
  uint64_t h;
  int i, ok = 0;
  if (!id || !dst || !n) return 0;
  len = strlen(id);
  if (!len || len > IDCANAL_LONGO_MAX) return 0;
  if (len <= IDCANAL_CURTO_MAX && strncmp(id, PREFIXO, sizeof PREFIXO - 1)) {
    if (len >= n) return 0;
    memcpy(dst, id, len + 1);
    return 1;
  }
  if (n < sizeof PREFIXO + 16) return 0;
  h = fnv1a(id);
  pthread_mutex_lock(&trava);
  for (i = 0; i < nTab && tab[i].h != h; i++) ;
  if (i < nTab) ok = !strcmp(tab[i].id, id);   // mesmo hash e outro id: recusa
  else if (nTab < TAB_MAX && (tab[nTab].id = strdup(id))) {
    tab[nTab++].h = h;
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  if (ok) snprintf(dst, n, PREFIXO "%016llx", (unsigned long long)h);
  return ok;
}

const char *idcanal_longo(const char *id, char *dst, size_t n) {
  unsigned long long h;
  char fim = 0;
  int i;
  if (!dst || !n) return dst;
  dst[0] = 0;
  if (!id) return dst;
  snprintf(dst, n, "%s", id);
  if (strncmp(id, PREFIXO, sizeof PREFIXO - 1) || strlen(id) != sizeof PREFIXO - 1 + 16 ||
      sscanf(id + sizeof PREFIXO - 1, "%16llx%c", &h, &fim) != 1)
    return dst;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nTab; i++)
    if (tab[i].h == (uint64_t)h) { snprintf(dst, n, "%s", tab[i].id); break; }
  pthread_mutex_unlock(&trava);
  return dst;
}
