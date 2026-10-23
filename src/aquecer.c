#include "aquecer.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef AQUECER_TESTE
#include "rede.h"
#endif

static AquecerMotor motor;
void aquecer_definir(AquecerMotor m) { motor = m; }

int aquecer_origem(const char *url, char *dst, unsigned n) {
  const char *p;
  size_t k;
  if (dst && n) dst[0] = 0;
  if (!url || !dst || n < 12) return 0;
  if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7)) return 0;
  p = strstr(url, "://") + 3;
  k = (size_t)(p - url) + strcspn(p, "/?#");
  if (k <= (size_t)(p - url) || k >= n) return 0;
  memcpy(dst, url, k);
  dst[k] = 0;
  // "user:senha@host": a origem nunca leva credencial embutida.
  if (strchr(dst + (p - url), '@')) { dst[0] = 0; return 0; }
  return 1;
}

#define RECENTES 16
static struct { char o[96]; unsigned long ms; } recentes[RECENTES];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

int aquecer_planejar(const char *const *urls, int n, unsigned long agora,
                     char origens[][96]) {
  int i, k, q = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n && q < AQUECER_LOTE_MAX; i++) {
    char o[96];
    int achou = -1, velho = 0, dup = 0;
    if (!aquecer_origem(urls[i], o, sizeof o)) continue;
    for (k = 0; k < q; k++) if (!strcmp(origens[k], o)) dup = 1;
    if (dup) continue;
    for (k = 0; k < RECENTES; k++) {
      if (!strcmp(recentes[k].o, o)) achou = k;
      if (recentes[k].ms < recentes[velho].ms) velho = k;
    }
    if (achou >= 0 && agora - recentes[achou].ms < AQUECER_REPETIR_MS) continue;
    if (achou < 0) achou = velho;
    snprintf(recentes[achou].o, sizeof recentes[achou].o, "%s", o);
    recentes[achou].ms = agora ? agora : 1;
    snprintf(origens[q++], 96, "%s", o);
  }
  pthread_mutex_unlock(&trava);
  return q;
}

static unsigned long relogio(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long)t.tv_sec * 1000UL + (unsigned long)(t.tv_nsec / 1000000L) + 1UL;
}

static pthread_mutex_t emCurso = PTHREAD_MUTEX_INITIALIZER;
static int rodando;
typedef struct { int n; char o[AQUECER_LOTE_MAX][96]; } Lote;

static void *fio(void *u) {
  Lote *l = u;
  const char *p[AQUECER_LOTE_MAX];
  unsigned ms[AQUECER_LOTE_MAX];
  int i;
  AquecerMotor m = motor;
#ifndef AQUECER_TESTE
  if (!m) m = rede_aquecer_lote;
#endif
  for (i = 0; i < l->n; i++) { p[i] = l->o[i]; ms[i] = 0; }
  if (m) {
    m(p, l->n, ms);
    for (i = 0; i < l->n; i++) {
      // So o host aparece no log (sem caminho, sem credencial).
      const char *h = strstr(l->o[i], "://");
      if (ms[i]) printf("[rede] aquecida: %s (%u ms)\n", h ? h + 3 : l->o[i], ms[i]);
    }
    fflush(stdout);
  }
  free(l);
  pthread_mutex_lock(&emCurso); rodando = 0; pthread_mutex_unlock(&emCurso);
  return NULL;
}

int aquecer_pedir(const char *const *urls, int n) {
  Lote *l;
  pthread_t t;
  int q, livre;
  pthread_mutex_lock(&emCurso);
  livre = !rodando;
  if (livre) rodando = 1;
  pthread_mutex_unlock(&emCurso);
  if (!livre) return 0;
  l = calloc(1, sizeof *l);
  if (!l) { pthread_mutex_lock(&emCurso); rodando = 0; pthread_mutex_unlock(&emCurso); return 0; }
  q = aquecer_planejar(urls, n, relogio(), l->o);
  l->n = q;
  if (q < 1 || pthread_create(&t, NULL, fio, l) != 0) {
    free(l);
    pthread_mutex_lock(&emCurso); rodando = 0; pthread_mutex_unlock(&emCurso);
    return 0;
  }
  pthread_detach(t);
  return q;
}
