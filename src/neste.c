#include "neste.h"
#include "rede.h"
#include "js.h"
#include "descoberta.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAB 6

typedef struct { char chave[32]; int estado; long quando; NesteElenco el; } Entrada;
static Entrada tab[TAB];
static int nTab, prox;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static Entrada *achar(const char *chave) {
  for (int i = 0; i < nTab; i++) if (!strcmp(tab[i].chave, chave)) return &tab[i];
  return NULL;
}
static Entrada *reservar(const char *chave) {
  Entrada *e = achar(chave);
  if (e) return e;
  if (nTab < TAB) e = &tab[nTab++];
  else { e = &tab[prox]; prox = (prox + 1) % TAB; }
  memset(e, 0, sizeof *e);
  snprintf(e->chave, sizeof e->chave, "%s", chave);
  return e;
}

int neste_interpretar(const char *json, NesteElenco *out) {
  memset(out, 0, sizeof *out);
  if (!json) return 0;
  for (const char *p = js_array(json, NULL, "guest_stars"); p && out->n < NESTE_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    int k = out->n;
    if (!f) break;
    js_texto(p, f, "name", out->p[k].nome, sizeof out->p[k].nome);
    js_texto(p, f, "character", out->p[k].personagem, sizeof out->p[k].personagem);
    if (out->p[k].nome[0]) out->n++;
  }
  return out->n;
}

static void chaveDe(char *dst, unsigned tam, const char *imdb, int t, int e) {
  snprintf(dst, tam, "%s:%d:%d", imdb, t, e);
}

typedef struct { char imdb[16]; int t, e; } Pedido;

static void *buscar(void *arg) {
  Pedido pd = *(Pedido *)arg;
  char url[300], chave[32];
  const char *key = desc_chave_tmdb();
  char *corpo;
  NesteElenco el;
  int achou = 0, ok = 0;
  Entrada *e;
  free(arg);
  el.n = 0;
  if (key && key[0]) {
    long id = 0;
    snprintf(url, sizeof url,
             "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", pd.imdb, key);
    corpo = rede_baixar(url, 10);
    if (corpo) {
      const char *v = js_array(corpo, NULL, "tv_results");
      if (v) id = (long)js_num(v, js_fim(v), "id", 0.0);
      free(corpo);
    }
    if (id > 0) {
      snprintf(url, sizeof url,
               "https://api.themoviedb.org/3/tv/%ld/season/%d/episode/%d/credits?api_key=%s&language=%s",
               id, pd.t, pd.e, key, desc_tmdb_idioma());
      corpo = rede_baixar(url, 10);
      if (corpo) { ok = 1; achou = neste_interpretar(corpo, &el); free(corpo); }
    }
  }
  chaveDe(chave, sizeof chave, pd.imdb, pd.t, pd.e);
  pthread_mutex_lock(&mtx);
  e = reservar(chave);
  e->quando = (long)time(NULL);
  if (achou) { e->el = el; e->estado = NESTE_PRONTO; }
  else e->estado = ok ? NESTE_VAZIO : NESTE_ERRO;
  pthread_mutex_unlock(&mtx);
  return NULL;
}

void neste_pedir(const char *imdb, int temp, int ep) {
  char chave[32];
  Entrada *e;
  Pedido *pd;
  pthread_t f;
  if (!imdb || strncmp(imdb, "tt", 2) || temp < 0 || ep <= 0) return;
  chaveDe(chave, sizeof chave, imdb, temp, ep);
  pthread_mutex_lock(&mtx);
  e = reservar(chave);
  if (e->estado == NESTE_CARREGANDO || e->estado == NESTE_PRONTO || e->estado == NESTE_VAZIO ||
      (e->estado == NESTE_ERRO && (long)time(NULL) - e->quando < 60)) {
    pthread_mutex_unlock(&mtx);
    return;
  }
  e->estado = NESTE_CARREGANDO;
  pthread_mutex_unlock(&mtx);
  pd = calloc(1, sizeof *pd);
  if (!pd) return;
  snprintf(pd->imdb, sizeof pd->imdb, "%s", imdb);
  pd->t = temp; pd->e = ep;
  if (pthread_create(&f, NULL, buscar, pd) == 0) pthread_detach(f);
  else { free(pd); pthread_mutex_lock(&mtx); e->estado = NESTE_ERRO; pthread_mutex_unlock(&mtx); }
}

NesteEstado neste_estado(const char *imdb, int temp, int ep, NesteElenco *out) {
  char chave[32];
  Entrada *e;
  NesteEstado r = NESTE_NADA;
  if (!imdb) return r;
  chaveDe(chave, sizeof chave, imdb, temp, ep);
  pthread_mutex_lock(&mtx);
  e = achar(chave);
  if (e) { r = (NesteEstado)e->estado; if (out && r == NESTE_PRONTO) *out = e->el; }
  pthread_mutex_unlock(&mtx);
  return r;
}
