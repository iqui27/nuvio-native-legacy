// Another episode's source list for "From another source" (audioextep.h).
#include "audioextep.h"
#include "addons.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  AddonsConsulta *consulta;
  unsigned geracao;
  char alvo[64];
} EpJob;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static _Atomic unsigned pedido = 1;   // the request alive; older workers drop what they get
// Handed back by a worker (under the lock), taken in on the UI thread.
static Stream *chegou;
static int chegouN = -2;
static unsigned chegouGer;
// The UI's own.
static char alvoAtual[64];
static Stream *lista;
static int listaN, estado;
static unsigned geracaoLista;

static int cancelado(void *u) {
  const EpJob *j = u;
  return j->geracao != atomic_load_explicit(&pedido, memory_order_acquire);
}
static void *trabalhar(void *u) {
  EpJob *j = u;
  Stream *fontes = NULL;
  int n = addons_consulta_consultar(j->consulta, j->alvo, "series", 4, cancelado, j, &fontes);
  pthread_mutex_lock(&trava);
  if (!cancelado(j)) {
    free(chegou);
    chegou = fontes; fontes = NULL;
    chegouN = n; chegouGer = j->geracao;
  }
  pthread_mutex_unlock(&trava);
  free(fontes);
  addons_consulta_liberar(j->consulta);
  free(j);
  return NULL;
}
static void largar(void) {
  free(lista); lista = NULL; listaN = 0;
  pthread_mutex_lock(&trava);
  free(chegou); chegou = NULL; chegouN = -2;
  pthread_mutex_unlock(&trava);
}
void audioextep_pedir(const char *alvo) {
  if (!alvo) alvo = "";
  if (!strcmp(alvo, alvoAtual) && (estado == AEXTEP_BUSCANDO || estado == AEXTEP_PRONTA)) return;
  atomic_fetch_add_explicit(&pedido, 1, memory_order_acq_rel);
  largar();
  geracaoLista++;
  snprintf(alvoAtual, sizeof alvoAtual, "%s", alvo);
  estado = AEXTEP_NADA;
  if (!alvo[0] || strlen(alvo) >= sizeof alvoAtual) { alvoAtual[0] = 0; return; }
  EpJob *j = calloc(1, sizeof *j);
  if (!j) { estado = AEXTEP_FALHOU; return; }
  j->geracao = atomic_load_explicit(&pedido, memory_order_acquire);
  snprintf(j->alvo, sizeof j->alvo, "%s", alvo);
  if (!(j->consulta = addons_consulta_capturar())) { free(j); estado = AEXTEP_FALHOU; return; }
  pthread_t fio;
  pthread_attr_t a;
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&a, 512u * 1024u);
  int ok = pthread_create(&fio, &a, trabalhar, j) == 0;
  pthread_attr_destroy(&a);
  if (!ok) { addons_consulta_liberar(j->consulta); free(j); estado = AEXTEP_FALHOU; return; }
  estado = AEXTEP_BUSCANDO;
  printf("[ext-audio] another episode: asking the add-ons for %s\n", alvo); fflush(stdout);
}
int audioextep_estado(void) {
  if (estado != AEXTEP_BUSCANDO) return estado;
  pthread_mutex_lock(&trava);
  if (chegouN != -2 && chegouGer == atomic_load_explicit(&pedido, memory_order_acquire)) {
    free(lista);
    lista = chegou; listaN = chegouN > 0 && chegou ? chegouN : 0;
    if (!lista) listaN = 0;
    estado = chegouN < 0 ? AEXTEP_FALHOU : AEXTEP_PRONTA;
    chegou = NULL; chegouN = -2;
    geracaoLista++;
    printf("[ext-audio] another episode: %s has %d source(s)%s\n", alvoAtual, listaN,
           estado == AEXTEP_FALHOU ? " (the add-ons did not answer)" : "");
    fflush(stdout);
  }
  pthread_mutex_unlock(&trava);
  return estado;
}
const char *audioextep_alvo(void) { return alvoAtual; }
int audioextep_n(void) { return estado == AEXTEP_PRONTA ? listaN : 0; }
const Stream *audioextep_item(int i) { return estado == AEXTEP_PRONTA && i >= 0 && i < listaN ? &lista[i] : NULL; }
unsigned audioextep_geracao(void) { return geracaoLista; }
