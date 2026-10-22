#include "fonteparalela.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

enum { P_ESPERA = 0, P_SERVE, P_NAO };

typedef struct Par Par;
typedef struct { Par *p; int q, indice; } Item;
struct Par {
  pthread_mutex_t m;
  pthread_cond_t cv;
  int k, refs;
  int estado[FONTEPARALELA_MAX];
  FonteVerificar ver;
  FonteFalhou fal;
  void *u;
  void (*soltarU)(void *u);
  Item item[FONTEPARALELA_MAX];
};

static void soltar(Par *p) {
  int resto;
  pthread_mutex_lock(&p->m);
  resto = --p->refs;
  pthread_mutex_unlock(&p->m);
  if (!resto) {
    if (p->soltarU) p->soltarU(p->u);
    pthread_mutex_destroy(&p->m);
    pthread_cond_destroy(&p->cv);
    free(p);
  }
}

static void *trabalho(void *a) {
  Item *it = a;
  Par *p = it->p;
  int ok = p->ver(it->indice, p->u);
  if (!ok && p->fal) p->fal(it->indice, p->u);
  pthread_mutex_lock(&p->m);
  p->estado[it->q] = ok ? P_SERVE : P_NAO;
  pthread_cond_broadcast(&p->cv);
  pthread_mutex_unlock(&p->m);
  soltar(p);
  return NULL;
}

int fonteparalela(const int *fila, int n, int k, FonteVerificar verificar,
                  FonteFalhou falhou, void *u, int *tocadas, unsigned prazoMs) {
  return fonteparalela_soltando(fila, n, k, verificar, falhou, u, tocadas, prazoMs, NULL);
}

int fonteparalela_soltando(const int *fila, int n, int k, FonteVerificar verificar,
                           FonteFalhou falhou, void *u, int *tocadas, unsigned prazoMs,
                           void (*soltarU)(void *u)) {
  Par *p;
  int q, venceu = -1;
  struct timespec fim;
  if (tocadas) *tocadas = 0;
  if (!fila || n < 1 || !verificar) { if (soltarU) soltarU(u); return -1; }
  if (k > n) k = n;
  if (k > FONTEPARALELA_MAX) k = FONTEPARALELA_MAX;
  if (k < 1) { if (soltarU) soltarU(u); return -1; }
  p = calloc(1, sizeof *p);
  if (!p) { if (soltarU) soltarU(u); return -1; }
  pthread_mutex_init(&p->m, NULL);
  pthread_cond_init(&p->cv, NULL);
  p->k = k; p->refs = 1 + k;
  p->ver = verificar; p->fal = falhou; p->u = u; p->soltarU = soltarU;
  for (q = 0; q < k; q++) {
    pthread_t t;
    p->item[q].p = p; p->item[q].q = q; p->item[q].indice = fila[q];
    if (tocadas) (*tocadas)++;
    if (pthread_create(&t, NULL, trabalho, &p->item[q]) == 0) pthread_detach(t);
    else trabalho(&p->item[q]);   // sem fio: confere aqui mesmo, em serie
  }
  if (prazoMs) {
    clock_gettime(CLOCK_REALTIME, &fim);
    fim.tv_sec += prazoMs / 1000u;
    fim.tv_nsec += (long)(prazoMs % 1000u) * 1000000L;
    if (fim.tv_nsec >= 1000000000L) { fim.tv_sec++; fim.tv_nsec -= 1000000000L; }
  }
  pthread_mutex_lock(&p->m);
  for (;;) {
    int espera = 0;
    for (q = 0; q < k; q++) {
      if (p->estado[q] == P_SERVE) { venceu = fila[q]; break; }
      if (p->estado[q] == P_ESPERA) { espera = 1; break; }
    }
    if (venceu >= 0 || !espera) break;
    if (prazoMs) {
      if (pthread_cond_timedwait(&p->cv, &p->m, &fim) == ETIMEDOUT) break;
    } else pthread_cond_wait(&p->cv, &p->m);
  }
  pthread_mutex_unlock(&p->m);
  soltar(p);
  return venceu;
}

int fonteparalela_prefixo(const int *fila, int n, int max, int (*pronta)(int i, void *u), void *u) {
  int k = 0;
  if (max > FONTEPARALELA_MAX) max = FONTEPARALELA_MAX;
  while (k < n && k < max && pronta(fila[k], u)) k++;
  return k;
}
