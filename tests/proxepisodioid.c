// ISSUE #697: Next Episode opened a different show after a catalog publication
// landed while the player was closing.
//
// abrirProximoEpisodio resolves the title, closes the player (fecharSessao
// itself starts desc_refazer_continuar) and only then reopens by id. If the
// title's only copy left in that window, cat_indice_vivo returned -1 and
// player_abrir wrapped it to the LAST catalog item. If indexes merely slid,
// the stale index pointed the episode at the neighbouring show.
//
//   bash tests/proxepisodioid.sh
#define main catalog_identity_fixture_main
#include "catalog_episode_identity512.c"
#undef main
#include <stdlib.h>

static CatItem show(const char *id) {
  CatItem c = title(0);
  snprintf(c.imdb, sizeof c.imdb, "%s", id);
  snprintf(c.titulo, sizeof c.titulo, "%s", id);
  return c;
}

static void publicar(CatItem *v, int qtd, int cw) {
  CatFileira f[2];
  memset(f, 0, sizeof f);
  snprintf(f[0].chave, sizeof f[0].chave, "continue_watching");
  f[0].ini = 0; f[0].n = cw;
  snprintf(f[1].chave, sizeof f[1].chave, "outra");
  f[1].ini = cw; f[1].n = qtd - cw;
  cat_definir_tudo(v, qtd, f, 2);
}

int main(void) {
  // The anime lives only in Continue Watching; an unrelated show is last.
  CatItem v[3] = {show("kitsu:7442"), show("tt0000001"), show("tt0903747")};
  CatItem aberto = v[0];
  char alvo[64];
  int i;

  publicar(v, 3, 1);
  assert(cat_indice_vivo(0, aberto.imdb) == 0);
  cat_trocar_continuar(NULL, 0); // the rebuild drops the title mid-close
  assert(cat_indice_vivo(0, aberto.imdb) == -1); // the path that used to wrap

  i = cat_indice_reabrir(0, &aberto);
  assert(i >= 0);
  assert(!strcmp(cat_item(i)->imdb, "kitsu:7442"));
  assert(cat_indice_reabrir(i, &aberto) == i); // no second copy
  assert(cat_id_stream(i, 1, 18, alvo, sizeof alvo) && !strcmp(alvo, "kitsu:7442:18"));

  // Indexes slide but the title stays: the result is its current slot, so the
  // episode pointer written next lands on it and not on the neighbour.
  publicar(v, 3, 1);
  { CatItem cw[2] = {show("tt7777777"), show("tt8888888")};
    cat_trocar_continuar(cw, 2); }
  i = cat_indice_reabrir(1, &v[1]);
  assert(i >= 0 && !strcmp(cat_item(i)->imdb, "tt0000001"));
  cat_apontar_episodio(i, 2, 3);
  assert(cat_item(i)->temporada == 2 && cat_item(i)->episodio == 3);
  assert(cat_item(1)->episodio != 3);

  // No id keeps the old contract; a full catalog reports failure, never a wrap.
  { CatItem semId = show("");
    assert(cat_indice_reabrir(1, &semId) == 1);
    CatItem *cheio = calloc(CAT_MAX, sizeof *cheio);
    assert(cheio);
    for (i = 0; i < CAT_MAX; i++) cheio[i] = show("tt0903747");
    cat_definir_tudo(cheio, CAT_MAX, NULL, 0);
    free(cheio);
    assert(cat_indice_reabrir(0, &aberto) == -1); }

  puts("PASS #697: reopening keeps the title through a catalog publication");
  return 0;
}
