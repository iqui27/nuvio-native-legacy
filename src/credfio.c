#define NV_DUBLE_SESSAO
#include "credfio.h"
#include "sync.h"
#include "perfisessao.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Isolated fixtures omit sync.c: weak legacy-equivalent defaults; sync.c's strong scoped APIs override them.
__attribute__((weak)) int sync_credencial_pedido(SyncCredencialPedido *p) {
  memset(p, 0, sizeof *p); p->sessao = nv_perfil_sessao_ler(); return 1;
}
__attribute__((weak)) int sync_empurrar_credencial_pedido(const SyncCredencialPedido *p, const char *prov, const char *j) {
  (void)p; return sync_empurrar_credencial(prov, j);
}
#define CF_SLOTS 4
typedef struct { char prov[16]; int estado, res; uint64_t sessao, pedido; } Slot;
typedef struct { char prov[16]; char *json; SyncCredencialPedido dono; uint64_t pedido; } Pedido;
static Slot slots[CF_SLOTS];
static uint64_t proximoPedido;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Slot *achar(const char *prov, int criar) {
  for (int i = 0; i < CF_SLOTS; i++)
    if (slots[i].prov[0] && !strcmp(slots[i].prov, prov)) return &slots[i];
  if (criar) for (int i = 0; i < CF_SLOTS; i++)
    if (!slots[i].prov[0]) { snprintf(slots[i].prov, sizeof slots[i].prov, "%s", prov); return &slots[i]; }
  return NULL;
}
static void *fio(void *u) {
  Pedido *p = u;
  int r = 0;
  if (nv_perfil_sessao_atual(p->dono.sessao))
    r = sync_empurrar_credencial_pedido(&p->dono, p->prov, p->json);
  if (nv_perfil_sessao_travar(p->dono.sessao)) {
    pthread_mutex_lock(&trava);
    Slot *s = achar(p->prov, 0);
    if (s && s->pedido == p->pedido && s->sessao == p->dono.sessao) {
      s->res = r; s->estado = 2;
    }
    pthread_mutex_unlock(&trava);
    nv_perfil_sessao_destravar();
  }
  memset(p->json, 0, strlen(p->json)); free(p->json);
  memset(p, 0, sizeof *p); free(p);
  return NULL;
}
int credfio_iniciar(const char *provider, const char *credJson) {
  pthread_t t; int ok = 0;
  if (!provider || !*provider || strlen(provider) >= sizeof slots[0].prov || !credJson) return 0;
  Pedido *p = calloc(1, sizeof *p);
  if (!p) return 0;
  if (!sync_credencial_pedido(&p->dono)) { free(p); return 0; }
  snprintf(p->prov, sizeof p->prov, "%s", provider);
  p->json = strdup(credJson);
  if (!p->json || !nv_perfil_sessao_travar(p->dono.sessao)) { free(p->json); free(p); return 0; }
  pthread_mutex_lock(&trava);
  Slot *s = achar(provider, 1);
  // A late worker from another opening owns a private job. It neither blocks
  // this profile's push nor clears its slot when the old response arrives.
  if (s && (s->estado == 0 || s->sessao != p->dono.sessao)) {
    if (!++proximoPedido) ++proximoPedido;
    p->pedido = proximoPedido;
    s->sessao = p->dono.sessao; s->pedido = p->pedido; s->estado = 1;
    if (pthread_create(&t, NULL, fio, p) == 0) { pthread_detach(t); ok = 1; }
    else s->estado = 0;
  }
  pthread_mutex_unlock(&trava);
  nv_perfil_sessao_destravar();
  if (!ok) { memset(p->json, 0, strlen(p->json)); free(p->json); free(p); }
  return ok;
}
int credfio_resultado(const char *provider, int *res) {
  int pronto = 0;
  uint64_t sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(sessao)) return 0;
  pthread_mutex_lock(&trava);
  Slot *s = provider ? achar(provider, 0) : NULL;
  if (s && s->sessao == sessao && s->estado == 2) {
    if (res) *res = s->res;
    s->estado = 0; pronto = 1;
  }
  pthread_mutex_unlock(&trava);
  nv_perfil_sessao_destravar();
  return pronto;
}
