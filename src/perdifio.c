#include "perdifio.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned id; char chave[96]; char *corpo; } Pedido;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static unsigned corrente, serie;
static int estado = PERDIFIO_NADA;
static PerdiResultado resultado;
static char resumo[PERDI_RESUMO_MAX];

static void *buscar(void *arg) {
  Pedido *pd = arg;
  char cab[128], texto[PERDI_RESUMO_MAX];
  const char *cabs[2] = { cab, NULL };
  int http = 0;
  char *r;
  PerdiResultado res;
  snprintf(cab, sizeof cab, "x-goog-api-key: %s", pd->chave);
  r = rede_postar_seguro_st(PERDI_URL, 45, cabs, pd->corpo, &http);
  res = perdi_resposta(http, r, texto, sizeof texto);
  free(r);
  memset(cab, 0, sizeof cab);
  memset(pd->chave, 0, sizeof pd->chave);
  pthread_mutex_lock(&mtx);
  if (pd->id == corrente) {
    resultado = res;
    snprintf(resumo, sizeof resumo, "%s", texto);
    estado = PERDIFIO_PRONTO;
  }
  pthread_mutex_unlock(&mtx);
  free(pd->corpo);
  free(pd);
  return NULL;
}

unsigned perdifio_pedir(const char *chave, char *corpo) {
  Pedido *pd;
  pthread_t f;
  unsigned id;
  if (!chave || !*chave || !corpo || !(pd = calloc(1, sizeof *pd))) { free(corpo); return 0; }
  snprintf(pd->chave, sizeof pd->chave, "%s", chave);
  pd->corpo = corpo;
  pthread_mutex_lock(&mtx);
  if (!++serie) serie = 1;
  id = pd->id = corrente = serie;
  estado = PERDIFIO_CARREGANDO;
  pthread_mutex_unlock(&mtx);
  if (pthread_create(&f, NULL, buscar, pd) == 0) { pthread_detach(f); return id; }
  free(pd->corpo); free(pd);
  perdifio_cancelar();
  return 0;
}

PerdiFioEstado perdifio_estado(unsigned id, PerdiResultado *res, char *dst, size_t tam) {
  PerdiFioEstado e = PERDIFIO_NADA;
  pthread_mutex_lock(&mtx);
  if (id && id == corrente) {
    e = (PerdiFioEstado)estado;
    if (e == PERDIFIO_PRONTO) {
      if (res) *res = resultado;
      if (dst && tam) snprintf(dst, tam, "%s", resumo);
    }
  }
  pthread_mutex_unlock(&mtx);
  return e;
}

void perdifio_cancelar(void) {
  pthread_mutex_lock(&mtx);
  corrente = 0;
  estado = PERDIFIO_NADA;
  resumo[0] = 0;
  pthread_mutex_unlock(&mtx);
}
