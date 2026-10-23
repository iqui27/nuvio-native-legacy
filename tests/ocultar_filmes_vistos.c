// #366: "Ocultar filmes assistidos" — desligado de fabrica, por perfil, e so
// FILMES vistos entram no filtro da descoberta (serie e filme nao visto ficam).
//
//   bash tests/ocultar_filmes_vistos.sh
#include "../src/ajustes.c"
#include "descoberta.h"
#include "catalogo.h"
#include <assert.h>

static CatItem item(const char *imdb, const char *tipo) {
  CatItem it; memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "%s", imdb);
  snprintf(it.tipo, sizeof it.tipo, "%s", tipo);
  return it;
}

int main(void) {
  // Fabrica: desligado, local deste aparelho e do PERFIL, chave propria.
  assert(!ajustes_ocultar_filmes_vistos());
  assert(!strcmp(CHAVE[AJ_OCULTAR_FILMES_VISTOS], "hideWatchedMoviesLocal"));
  assert(somenteDesteAparelho(AJ_OCULTAR_FILMES_VISTOS));
  assert(perfilLocal(AJ_OCULTAR_FILMES_VISTOS));
  ajustes_definir_ocultar_filmes_vistos(1); assert(ajustes_ocultar_filmes_vistos());
  ajustes_definir_ocultar_filmes_vistos(0); assert(!ajustes_ocultar_filmes_vistos());

  // So filme visto conta: serie vista e filme nao visto nunca sao ocultados.
  cat_historico_definir_id("tt0000001", "movie", 1);
  cat_historico_definir_id("tt0000002", "movie", 0);
  cat_historico_definir_id("tt0000003", "series", 1);
  CatItem visto = item("tt0000001", "movie"), nao = item("tt0000002", "movie"),
          serie = item("tt0000003", "series");
  assert(desc_filme_visto(&visto));
  assert(!desc_filme_visto(&nao));
  assert(!desc_filme_visto(&serie));
  puts("ok ocultar_filmes_vistos");
  return 0;
}
