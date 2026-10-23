// Exercise the actual Saved-tab filtering and counters.
void bibfonte_library_shot(void);
#define txt_tracking bibfonte_tracking
#include "../src/biblioteca.c"
#undef txt_tracking
#include <assert.h>
static char badge[64];
float txt_tracking(TxtEstilo, const char *, int, int, int, float, float, float, float);
float bibfonte_tracking(TxtEstilo style, const char *s, int r, int g, int b,
                       float x, float y, float alpha, float tracking) {
  snprintf(badge, sizeof badge, "%s", s);
  return txt_tracking(style, s, r, g, b, x, y, alpha, tracking);
}
int bibfonte_library_frame(int count, const char *id, const char *expectedBadge) {
  biblioteca_atualizar(1.0f / 60, SDL_GetTicks());
  CatItem tmp;
  const CatItem *first = nFiltro ? itemFiltro(filtro[0], &tmp) : NULL;
  badge[0] = 0;
  desenhaCabecalho();
  return !strcmp(badge, expectedBadge) && nFiltro == count && nCelulas == count && contaModo[MODO_SALVOS] == count &&
         ((!id && !first) || (id && first && !strcmp(first->imdb, id)));
}
int bibfonte_library_count(void) {
  modo = MODO_SALVOS; tipo = TIPO_TODOS; ordem = ORD_ADICIONADOS;
  reconstruir();
  assert(contaModo[MODO_SALVOS] == nFiltro);
  return nFiltro;
}
void bibfonte_library_shot(void) {
  biblioteca_atualizar(1.0f / 60, SDL_GetTicks());
  biblioteca_desenhar(SDL_GetTicks());
}
