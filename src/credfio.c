#include "credfio.h"
#include "sync.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CF_SLOTS 4
typedef struct { char prov[16]; char *json; int estado; int res; } Slot; // 0 livre, 1 no ar, 2 pronto
static Slot slots[CF_SLOTS];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Slot *achar(const char *prov, int criar) { // com a trava
  int i;
  for (i = 0; i < CF_SLOTS; i++) if (slots[i].prov[0] && !strcmp(slots[i].prov, prov)) return &slots[i];
  if (!criar) return NULL;
  for (i = 0; i < CF_SLOTS; i++)
    if (!slots[i].prov[0]) { snprintf(slots[i].prov, sizeof slots[i].prov, "%s", prov); return &slots[i]; }
  return NULL;
}

static void *fio(void *u) {
  Slot *s = (Slot *)u;
  char prov[16]; char *json;
  int r;
  pthread_mutex_lock(&trava);
  snprintf(prov, sizeof prov, "%s", s->prov);
  json = s->json; s->json = NULL;
  pthread_mutex_unlock(&trava);
  r = sync_empurrar_credencial(prov, json);   // rede: sem trava
  free(json);
  pthread_mutex_lock(&trava);
  s->res = r; s->estado = 2;
  pthread_mutex_unlock(&trava);
  return NULL;
}

int credfio_iniciar(const char *provider, const char *credJson) {
  Slot *s; pthread_t t; int ok = 0;
  if (!provider || !*provider || strlen(provider) >= sizeof slots[0].prov || !credJson) return 0;
  pthread_mutex_lock(&trava);
  s = achar(provider, 1);
  if (s && s->estado == 0 && (s->json = strdup(credJson))) {
    s->estado = 1;
    if (pthread_create(&t, NULL, fio, s) == 0) { pthread_detach(t); ok = 1; }
    else { free(s->json); s->json = NULL; s->estado = 0; }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int credfio_resultado(const char *provider, int *res) {
  Slot *s; int pronto = 0;
  pthread_mutex_lock(&trava);
  s = achar(provider, 0);
  if (s && s->estado == 2) { if (res) *res = s->res; s->estado = 0; pronto = 1; }
  pthread_mutex_unlock(&trava);
  return pronto;
}
