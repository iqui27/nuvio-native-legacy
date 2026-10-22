#include "credfonte.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

double cred_escolher(const CredEntrada *e, int *fonte) {
  double s;
  int f = CRED_FONTE_NENHUMA;
  if (fonte) *fonte = f;
  if (!e || e->dur <= 1.0) return 0.0;
  if (e->capitulo > 1.0) { s = e->capitulo; f = CRED_FONTE_CAPITULO; goto fim; }
  if (e->introdb > 1.0)  { s = e->introdb;  f = CRED_FONTE_INTRODB;  goto fim; }
  if (e->aniskip > 1.0)  { s = e->aniskip;  f = CRED_FONTE_ANISKIP;  goto fim; }
  if (e->vizInicio > 1.0) {
    // Com a duracao do vizinho a conta e por "quanto falta"; sem ela, assume a
    // mesma duracao e o inicio vale como veio.
    s = e->vizDur > e->vizInicio ? e->dur - (e->vizDur - e->vizInicio) : e->vizInicio;
    if (s >= e->dur * 0.5 && s <= e->dur - 15.0) { f = CRED_FONTE_VIZINHO; goto fim; }
  }
  if (e->aprendidoResto >= 15.0) {
    s = e->dur - e->aprendidoResto;
    if (s >= e->dur * 0.5) { f = CRED_FONTE_APRENDIDO; goto fim; }
  }
  return 0.0;
fim:
  if (fonte) *fonte = f;
  return s;
}

const char *cred_fonte_nome(int f) {
  switch (f) {
    case CRED_FONTE_CAPITULO: return "mkv-chapter";
    case CRED_FONTE_INTRODB:  return "introdb";
    case CRED_FONTE_ANISKIP:  return "aniskip";
    case CRED_FONTE_VIZINHO:  return "neighbour-episode";
    case CRED_FONTE_APRENDIDO: return "learned-offset";
    default: return "estimate";
  }
}

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// O id do catalogo pode vir "tt123:1:2": vale so a parte antes do ':'.
static int mesmo(const char *guardado, const char *imdb) {
  size_t n = strcspn(imdb, ":");
  return strlen(guardado) == n && !strncmp(guardado, imdb, n);
}
static void copiar(char *dst, size_t tam, const char *imdb) {
  size_t n = strcspn(imdb, ":");
  if (n > tam - 1) n = tam - 1;
  memcpy(dst, imdb, n); dst[n] = 0;
}

typedef struct { char imdb[24]; int t; double inicio, dur; } Viz;
static Viz viz[16];
static int vizProx;

void cred_viz_guardar(const char *imdb, int t, double inicio, double durViz) {
  int i;
  if (!imdb || !*imdb || inicio <= 1.0) return;
  pthread_mutex_lock(&trava);
  for (i = 0; i < 16; i++) if (viz[i].t == t && mesmo(viz[i].imdb, imdb)) break;
  if (i == 16) { i = vizProx; vizProx = (vizProx + 1) % 16; }
  copiar(viz[i].imdb, sizeof viz[i].imdb, imdb);
  viz[i].t = t; viz[i].inicio = inicio; viz[i].dur = durViz;
  pthread_mutex_unlock(&trava);
}

int cred_viz_ler(const char *imdb, int t, double *inicio, double *durViz) {
  int i, ok = 0;
  if (!imdb || !*imdb) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < 16; i++)
    if (viz[i].inicio > 1.0 && viz[i].t == t && mesmo(viz[i].imdb, imdb)) {
      if (inicio) *inicio = viz[i].inicio;
      if (durViz) *durViz = viz[i].dur;
      ok = 1; break;
    }
  pthread_mutex_unlock(&trava);
  return ok;
}

#define N404 64
#define VALE404_S (30L * 60L)
typedef struct { char imdb[24]; int t, e; long quando; } Perdido;
static Perdido perdido[N404];
static int perdidoProx;

int cred_404_visto(const char *imdb, int t, int e, long agora) {
  int i, r = 0;
  if (!imdb || !*imdb) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < N404; i++)
    if (perdido[i].quando && perdido[i].t == t && perdido[i].e == e &&
        mesmo(perdido[i].imdb, imdb) &&
        agora - perdido[i].quando < VALE404_S) { r = 1; break; }
  pthread_mutex_unlock(&trava);
  return r;
}

void cred_404_marcar(const char *imdb, int t, int e, long agora) {
  int i;
  if (!imdb || !*imdb) return;
  pthread_mutex_lock(&trava);
  for (i = 0; i < N404; i++)
    if (perdido[i].t == t && perdido[i].e == e && mesmo(perdido[i].imdb, imdb)) break;
  if (i == N404) { i = perdidoProx; perdidoProx = (perdidoProx + 1) % N404; }
  copiar(perdido[i].imdb, sizeof perdido[i].imdb, imdb);
  perdido[i].t = t; perdido[i].e = e; perdido[i].quando = agora ? agora : 1;
  pthread_mutex_unlock(&trava);
}
