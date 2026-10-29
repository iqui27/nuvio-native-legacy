// Exercise the Library with the provider matching the Settings write target.
// All is now an explicit union; this suite checks each individual provider.
void bibfonte_library_shot(void);
#define txt_tracking bibfonte_tracking
#include "../src/biblioteca.c"
#undef txt_tracking
#include <assert.h>
static char badge[64];
static void selectWriteSource(void) {
  int source = ajustes_fonte_biblioteca();
  origem = source == AJ_SALVOS_TRAKT ? ORIG_TRAKT : source == AJ_SALVOS_SIMKL ? ORIG_SIMKL : ORIG_NUVIO;
  reconstruir();
}
float txt_tracking(TxtEstilo, const char *, int, int, int, float, float, float, float);
float bibfonte_tracking(TxtEstilo style, const char *s, int r, int g, int b,
                       float x, float y, float alpha, float tracking) {
  snprintf(badge, sizeof badge, "%s", s);
  return txt_tracking(style, s, r, g, b, x, y, alpha, tracking);
}
int bibfonte_library_frame(int count, const char *id, const char *expectedBadge) {
  biblioteca_atualizar(1.0f / 60, SDL_GetTicks());
  selectWriteSource();
  CatItem tmp;
  const CatItem *first = nFiltro ? itemFiltro(filtro[0], &tmp) : NULL;
  badge[0] = 0;
  desenhaCabecalho();
  const char *expected = !strcmp(expectedBadge, "ACCOUNT") ? "Conta Nuvio" : !strcmp(expectedBadge, "TRAKT") ? "Trakt" : "Simkl";
  return !strcmp(fonteCab, expected) && nFiltro == count && nCelulas == count && contaOrigem[origem] == count &&
         ((!id && !first) || (id && first && !strcmp(first->imdb, id)));
}
int bibfonte_library_count(void) {
  modo = MODO_PARAVER; tipo = TIPO_TODOS; ordem = ORD_ADICIONADOS;
  selectWriteSource();
  assert(contaOrigem[origem] == nFiltro);
  return nFiltro;
}
void bibfonte_library_shot(void) {
  biblioteca_atualizar(1.0f / 60, SDL_GetTicks());
  biblioteca_desenhar(SDL_GetTicks());
}
