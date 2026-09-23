#include "visto.h"
#include "trakt.h"
#include "simkl.h"
#include "syncprog.h"
#include "sessao.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Teto do lote: o mesmo do Simkl, que e o menor dos tres (Trakt aceita 256
// desde este mesmo conserto; a conta nao tem teto no cliente).
#define VT_LOTE SMK_LOTE_MAX
#define VT_TEMPS 64

int visto_destinos(void) {
  return (trakt_ativo() ? VISTO_TRAKT : 0) |
         (simkl_ativo() ? VISTO_SIMKL : 0) |
         (sessao_logada() ? VISTO_CONTA : 0);
}

int visto_episodios_ja(const char *imdb, const char *tipo, const VistoPar *pares,
                       int n, int visto, int destinos) {
  int ok = 1;
  if (!imdb || !imdb[0] || !pares || n < 1) return 0;
  if (n > VT_LOTE) n = VT_LOTE;
  // Ordem: Trakt, Simkl, conta. Nenhum depende do outro; a falha de um nao
  // impede os seguintes (cada um diz no log o que aconteceu).
  if (destinos & VISTO_TRAKT) ok &= trakt_episodios_marcar(imdb, pares, n, visto) ? 1 : 0;
  if (destinos & VISTO_SIMKL) ok &= simkl_episodios_marcar(imdb, pares, n, visto) ? 1 : 0;
  if (destinos & VISTO_CONTA) ok &= syncep_empurrar(imdb, tipo, pares, n, visto) ? 1 : 0;
  return ok;
}

int visto_titulo_ja(const char *imdb, const char *tipo, const int *temporadas,
                    int nt, int visto, int destinos) {
  int ok = 1;
  if (!imdb || !imdb[0]) return 0;
  if (destinos & VISTO_SIMKL) ok &= simkl_titulo_marcar(imdb, tipo, temporadas, nt, visto) ? 1 : 0;
  if (destinos & VISTO_CONTA) ok &= syncvisto_titulo(imdb, tipo, visto) ? 1 : 0;
  return ok;
}

// A COPIA E DO FIO: quem chamou pode ter o CatItem trocado de bloco pela
// descoberta antes de o fio ler (mesma razao do Envio de episodios.c).
typedef struct {
  char imdb[24], tipo[12];
  VistoPar pares[VT_LOTE];
  int temps[VT_TEMPS];
  int n, nt, visto, destinos, titulo;
} Envio;

static void *fio(void *u) {
  Envio *e = (Envio *)u;
  if (e->titulo) visto_titulo_ja(e->imdb, e->tipo, e->temps, e->nt, e->visto, e->destinos);
  else visto_episodios_ja(e->imdb, e->tipo, e->pares, e->n, e->visto, e->destinos);
  free(e);
  return NULL;
}

static int lancar(Envio *e) {
  pthread_t t;
  if (pthread_create(&t, NULL, fio, e) != 0) { free(e); return 0; }
  pthread_detach(t);
  return 1;
}

int visto_episodios(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto, int destinos) {
  Envio *e;
  if (!imdb || !imdb[0] || !pares || n < 1) return 0;
  if (!destinos) return 1;   // so local: nao ha o que mandar
  e = (Envio *)calloc(1, sizeof *e);
  if (!e) return 0;
  if (n > VT_LOTE) n = VT_LOTE;
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  snprintf(e->tipo, sizeof e->tipo, "%s", tipo && tipo[0] ? tipo : "series");
  memcpy(e->pares, pares, sizeof(VistoPar) * (size_t)n);
  e->n = n; e->visto = visto; e->destinos = destinos;
  return lancar(e);
}

int visto_titulo(const char *imdb, const char *tipo, const int *temporadas,
                 int nt, int visto, int destinos) {
  Envio *e;
  destinos &= ~VISTO_TRAKT;
  if (!imdb || !imdb[0]) return 0;
  if (!destinos) return 1;
  e = (Envio *)calloc(1, sizeof *e);
  if (!e) return 0;
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  snprintf(e->tipo, sizeof e->tipo, "%s", tipo && tipo[0] ? tipo : "movie");
  if (temporadas && nt > 0) {
    if (nt > VT_TEMPS) nt = VT_TEMPS;
    memcpy(e->temps, temporadas, sizeof(int) * (size_t)nt);
    e->nt = nt;
  }
  e->visto = visto; e->destinos = destinos; e->titulo = 1;
  return lancar(e);
}
