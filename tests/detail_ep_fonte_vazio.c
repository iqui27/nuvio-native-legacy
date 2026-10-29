// ISSUE #513 (P2): short OK on an episode row with no CatEp (focus past the
// list, empty slot) must not dereference ep. The hold path already guards it;
// the choose-on-play path used ep->temporada with ep possibly NULL.
//
// Zero-init the catalog. One episode stored. After the frame loop the focus
// is forced past the list, where cat_episodio returns NULL. Short OK with
// choose-on-play on must ask to play, not crash, and must not latch a season.
#include "nvtmp.h"
#include "dados.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/detail.c"

static char dir[NVT_MAX], caminho[700];
static void gravar(const char *txt) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(txt, f);
  fclose(f);
  ajustes_dir(dir);
}

int main(void) {
  CatItem c;
  CatEp ep;
  int i, t, e;

  nvt_set(dir, sizeof dir, "/nuvio-detail-epvazio-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Série de Ensaio");
  snprintf(c.imdb, sizeof c.imdb, "tt0000513");
  c.temporadas[0] = 1; c.nTemporadas = 1;
  cat_definir_tudo(&c, 1, NULL, 0); idx = 0;
  memset(&ep, 0, sizeof ep);
  ep.temporada = 1; ep.episodio = 1;
  snprintf(ep.nome, sizeof ep.nome, "Episódio 1");
  cat_definir_episodios(0, &ep, 1);
  assert(cat_episodio(0, 5) == NULL);

  gravar("escolherFonteManual 0\n");
  assert(ajustes_fonte_manual());

  aberto = 1; saindo = 0; nivel = 1;
  foco.fileira = SEC_EPISODIOS; foco.coluna = 0;
  for (i = 0; i < 30; i++) detail_atualizar(1.0f / 60.0f, 1000 + i * 16);
  // The frame loop clamps the column to the list. Past it, cat_episodio
  // returns NULL (epAbsoluto past the last visible episode).
  foco.fileira = SEC_EPISODIOS; foco.coluna = 5;
  assert(cat_episodio(idx, epAbsoluto(foco.coluna)) == NULL);

  { SDL_Event ev = {0};
    ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN;
    detail_evento(&ev);
    ev.type = SDL_KEYUP;
    detail_evento(&ev);
  }
  assert(!pedFontes && pedReproduzir);
  t = e = -1;
  assert(!detail_pediu_fontes_ep(&t, &e));
  assert(t == 0 && e == 0);
  puts("ok: empty episode slot does not latch and does not crash");
  puts("detail_ep_fonte_vazio: short OK past the list is safe");
  return 0;
}
