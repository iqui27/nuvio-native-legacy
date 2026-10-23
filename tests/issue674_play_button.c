// #674: exercise real detail selection and focus with offline metadata workers.
#include "nvtmp.h"
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0;
}
#include "../src/detail.c"

static void abrir(void) {
  HomeItem hi = {0}; hi.indice = 0; detail_abrir(&hi);
}
static void alvo(int t, int e) {
  int at, ae;
  assert(detail_ep_foco(&at, &ae) && at == t && ae == e);
}
static void ancora(int t, int e) {
  const CatEp *ep = cat_episodio(idx, epAbsoluto(epAncora));
  assert(ep && ep->temporada == t && ep->episodio == e);
  assert(foco.colunaLembrada[SEC_TEMPORADAS] == colTemp(temporada));
  assert(foco.colunaLembrada[SEC_EPISODIOS] == epAncora);
}
int main(void) {
  CatItem c = {0}; CatEp eps[20] = {{0}};
  char dir[NVT_MAX];
  nvt_set(dir, sizeof dir, "/nuvio-issue674-XXXXXX");
  assert(mkdtemp(dir)); assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  strcpy(c.tipo, "series"); strcpy(c.titulo, "Series 674");
  strcpy(c.imdb, "tt0000674");
  c.temporadas[0] = 1; c.temporadas[1] = 2; c.nTemporadas = 2;
  cat_definir_tudo(&c, 1, NULL, 0);
  for (int i = 0; i < 20; i++) {
    eps[i].temporada = i / 10 + 1; eps[i].episodio = i % 10 + 1;
  }
  cat_definir_episodios(0, eps, 20);
  abrir(); alvo(1, 1); ancora(1, 1); // No history.

  // History arrives after opening: S1 watched, S2E1..8 watched, next S2E9.
  for (int i = 0; i < 20; i++)
    vistoep_definir(c.imdb, eps[i].temporada, eps[i].episodio, i < 18);
  extras_teste_progresso(c.imdb, 18, 20, 2, 9);
  sincronizarEpisodioPadrao(); alvo(2, 9); ancora(2, 9);
  abrir(); alvo(2, 9); ancora(2, 9); // Local history also seeds reopening.

  // Browse another card deliberately; late history must preserve that choice.
  nivel = 1; foco.fileira = SEC_EPISODIOS; foco.coluna = 2;
  sincronizarEpisodioPadrao(); alvo(2, 3);
  assert(epEscolhido);
  // Back to the hero must play its advertised default, not the old row focus.
  nivel = 0; alvo(2, 9);

  // Fully watched starts at S1E1 even after browsing season 2.
  for (int i = 0; i < 20; i++)
    vistoep_definir(c.imdb, eps[i].temporada, eps[i].episodio, 1);
  extras_teste_progresso(c.imdb, 20, 20, 0, 0);
  abrir(); alvo(1, 1); ancora(1, 1);

  // Resume still takes priority; use an unwatched, partially played episode.
  vistoep_definir(c.imdb, 2, 10, 0);
  c.progresso = 40; c.temporada = 2; c.episodio = 10;
  cat_definir_tudo(&c, 1, NULL, 0);
  cat_definir_episodios(0, eps, 20);
  abrir(); alvo(2, 10); ancora(2, 10);
  puts("PASS #674: local/late history, hero target, manual focus, all watched, resume");
  return 0;
}
