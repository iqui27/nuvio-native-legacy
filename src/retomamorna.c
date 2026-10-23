#include "retomamorna.h"
#include "aquecer.h"
#include "rede.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static RedeJob *ativo;
static char origem[96];
typedef struct { RedeJob *job; char url[96]; } PedidoMorno;

void retomamorna_cancelar(void) {
  if (ativo) { rede_job_cancelar(ativo); rede_job_soltar(ativo); ativo = NULL; }
  origem[0] = 0;
}
static void *preparar(void *u) {
  PedidoMorno *p = u;
  const char *url = p->url; unsigned ms;
  // Reuse the existing parked connection pool; cancellation interrupts idle I/O.
  rede_aquecer_job(&url, 1, &ms, p->job);
  rede_job_soltar(p->job); free(p);
  return NULL;
}
void retomamorna_foco(const char *url) {
  char o[96]; pthread_t t;
  if (!aquecer_origem(url, o, sizeof o)) { retomamorna_cancelar(); return; }
  if (!strcmp(o, origem)) return;
  retomamorna_cancelar();
  RedeGrupo *g = rede_grupo_criar();
  if (!g) return;
  PedidoMorno *p = calloc(1, sizeof *p);
  ativo = rede_job_criar(g); rede_grupo_soltar(g);
  if (!p || !ativo) { free(p); retomamorna_cancelar(); return; }
  p->job = ativo; rede_job_reter(ativo);
  snprintf(p->url, sizeof p->url, "%s", o);
  snprintf(origem, sizeof origem, "%s", o);
  if (pthread_create(&t, NULL, preparar, p) == 0) pthread_detach(t);
  else { rede_job_soltar(p->job); free(p); retomamorna_cancelar(); }
}
#ifndef NV_ANDROID
void cacheboost_backend_morno(int ligado) { (void)ligado; }
int cacheboost_backend_quadro(int guardar) { (void)guardar; return 0; }
#endif
