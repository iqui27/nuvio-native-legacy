#define main biblioteca_existing_main
#include "biblioteca_filtro.c"
#undef main

static int membership(const CatItem *it, int provider) {
  int bits = !strcmp(it->imdb, "tt0000000") ? 3 : !strcmp(it->imdb, "tt0000001") ? 4 : 0;
  return provider < 0 ? bits != 0 : (bits & (1 << provider)) != 0;
}
int main(void) {
  for (int i = 0; i < 127; i++) {
    snprintf(catalogoTeste[i].imdb, sizeof catalogoTeste[i].imdb, "tt%07d", i);
    strcpy(catalogoTeste[i].tipo, "movie");
  }
  // Server ownership is sufficient for Library membership; no fake watchlist.
  biblioteca_servidores_contem(membership);
  modo = MODO_PARAVER; tipo = TIPO_TODOS; origem = ORIG_TODAS;
  reconstruir();
  assert(nFiltro == 2 && contaModo[MODO_PARAVER] == 2);
  origem=ORIG_NUVIO; reconstruir(); assert(nFiltro==0 && contaOrigem[ORIG_NUVIO]==0);
  origem=ORIG_TODAS; reconstruir();
  biblioteca_servidor_filtrar(0); assert(nFiltro == 1 && filtro[0] == 0);
  biblioteca_servidor_filtrar(1); assert(nFiltro == 1 && filtro[0] == 0);
  biblioteca_servidor_filtrar(2); assert(nFiltro == 1 && filtro[0] == 1);
  // Canonical title shared by servers still appears once.
  biblioteca_servidor_filtrar(-1); assert(nFiltro == 2);
  catalogoTeste[0].progresso = 50;
  modo = MODO_ANDAMENTO; reconstruir(); assert(nFiltro == 1);
  biblioteca_servidor_filtrar(2); assert(nFiltro == 0);
  biblioteca_servidores_contem(NULL);
  biblioteca_servidor_filtrar(-1);
  modo = MODO_PARAVER; reconstruir(); assert(nFiltro == 0);
  puts("Library servers: ownership, provider filtering, canonical dedup, Continue: PASS");
  return 0;
}
