#include "perdifio.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Ate PERDIFIO_FIOS pedidos vivos (o corrente e os cancelados que ainda estao
// saindo). Cada fio e JUNTAVEL: perdifio_pedir recolhe os que ja sairam e
// perdifio_encerrar cancela e junta todos. O cancelamento e cooperativo: o fio
// instala rede_parar_quando, e a libcurl aborta a transferencia (inclusive a
// ociosa) assim que o pedido deixa de ser o corrente.
#define PERDIFIO_FIOS 4

typedef struct {
  unsigned id;
  char chave[96];
  char *corpo;
} Pedido;
typedef struct {
  pthread_t fio;
  int vivo;      // pthread_create deu certo e ainda nao houve join
  int saiu;      // o fio terminou (join nao bloqueia)
  Pedido *pd;
} Vaga;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static unsigned corrente, serie;
static int estado = PERDIFIO_NADA, encerrando;
static PerdiResultado resultado;
static char resumo[PERDI_RESUMO_MAX];
static Vaga vagas[PERDIFIO_FIOS];

// Zera sem o compilador poder apagar a escrita (buffer que vai ser liberado).
static void zerar(void *p, size_t n) {
  volatile unsigned char *v = p;
  while (n--) *v++ = 0;
}

static int parar(void *u) {
  const Pedido *pd = u;
  int p;
  pthread_mutex_lock(&mtx);
  p = encerrando || pd->id != corrente;
  pthread_mutex_unlock(&mtx);
  return p;
}

static void *buscar(void *arg) {
  Vaga *vg = arg;
  Pedido *pd = vg->pd;
  char cab[128], texto[PERDI_RESUMO_MAX];
  const char *cabs[2] = { cab, NULL };
  int http = 0;
  char *r = NULL;
  PerdiResultado res;
  rede_parar_quando(parar, pd);
  if (!parar(pd)) {
    snprintf(cab, sizeof cab, "x-goog-api-key: %s", pd->chave);
    r = rede_postar_seguro_st(PERDI_URL, 45, cabs, pd->corpo, &http);
    zerar(cab, sizeof cab);
  }
  zerar(pd->chave, sizeof pd->chave);
  rede_parar_quando(NULL, NULL);
  res = perdi_resposta(http, r, texto, sizeof texto);
  free(r);
  pthread_mutex_lock(&mtx);
  if (!encerrando && pd->id == corrente) {
    resultado = res;
    snprintf(resumo, sizeof resumo, "%s", texto);
    estado = PERDIFIO_PRONTO;
  }
  vg->saiu = 1;
  pthread_mutex_unlock(&mtx);
  return NULL;
}

static void liberarPedido(Pedido *pd) {
  if (!pd) return;
  zerar(pd->chave, sizeof pd->chave);
  free(pd->corpo);
  free(pd);
}

// Junta os fios que ja sairam. Chamar SEM a trava.
static void recolher(void) {
  for (int i = 0; i < PERDIFIO_FIOS; i++) {
    int pronto;
    pthread_mutex_lock(&mtx);
    pronto = vagas[i].vivo && vagas[i].saiu;
    pthread_mutex_unlock(&mtx);
    if (!pronto) continue;
    pthread_join(vagas[i].fio, NULL);
    liberarPedido(vagas[i].pd);
    pthread_mutex_lock(&mtx);
    memset(&vagas[i], 0, sizeof vagas[i]);
    pthread_mutex_unlock(&mtx);
  }
}

unsigned perdifio_pedir(const char *chave, char *corpo) {
  Pedido *pd;
  Vaga *vg = NULL;
  unsigned id;
  recolher();
  if (!chave || !*chave || !corpo || !(pd = calloc(1, sizeof *pd))) { free(corpo); return 0; }
  snprintf(pd->chave, sizeof pd->chave, "%s", chave);
  pd->corpo = corpo;
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < PERDIFIO_FIOS && !vg; i++) if (!vagas[i].vivo) vg = &vagas[i];
  if (encerrando || !vg) {
    // Sem vaga: os anteriores ainda estao saindo. Nao dispara (o cartao diz
    // "Nao foi possivel resumir agora") em vez de acumular fios.
    corrente = 0; estado = PERDIFIO_NADA;
    pthread_mutex_unlock(&mtx);
    liberarPedido(pd);
    return 0;
  }
  if (!++serie) serie = 1;
  id = pd->id = corrente = serie;
  estado = PERDIFIO_CARREGANDO;
  vg->pd = pd; vg->saiu = 0; vg->vivo = 1;
  if (pthread_create(&vg->fio, NULL, buscar, vg) != 0) {
    memset(vg, 0, sizeof *vg);
    corrente = 0; estado = PERDIFIO_NADA;
    pthread_mutex_unlock(&mtx);
    liberarPedido(pd);
    return 0;
  }
  pthread_mutex_unlock(&mtx);
  return id;
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
  zerar(resumo, sizeof resumo);
  pthread_mutex_unlock(&mtx);
  recolher();
}

void perdifio_encerrar(void) {
  pthread_mutex_lock(&mtx);
  encerrando = 1;
  corrente = 0;
  estado = PERDIFIO_NADA;
  zerar(resumo, sizeof resumo);
  pthread_mutex_unlock(&mtx);
  for (int i = 0; i < PERDIFIO_FIOS; i++) {
    if (!vagas[i].vivo) continue;
    pthread_join(vagas[i].fio, NULL);   // o parar() ja devolve 1: a libcurl aborta
    liberarPedido(vagas[i].pd);
    memset(&vagas[i], 0, sizeof vagas[i]);
  }
}
