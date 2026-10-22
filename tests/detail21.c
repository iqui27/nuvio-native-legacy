#define extras_relacionado_imdb relFixture
#include "../src/detail.c"
#undef extras_relacionado_imdb
const char *relFixture(int i) { (void)i; return "tt9000011"; }
#include <assert.h>
#include "dados.h"

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  assert(ajustes_detalhe_topo() == 0);
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", dados_dir());
  for (int mode = 0; mode < 3; mode++) {
    FILE *f = fopen(path, "w"); assert(f);
    fprintf(f, "detalheTopoLocal %d\n", mode); fclose(f);
    ajustes_dir(dados_dir());
    assert(ajustes_detalhe_topo() == mode);
    assert(topoVersao() == (mode != 1));
    assert(topoPessoal() == (mode != 0));
  }
  CatItem itemTeste = {0};
  snprintf(itemTeste.tipo, sizeof itemTeste.tipo, "series");
  snprintf(itemTeste.imdb, sizeof itemTeste.imdb, "tt9000011");
  itemTeste.temporadas[0] = 1; itemTeste.nTemporadas = 1;
  cat_definir_tudo(&itemTeste, 1, NULL, 0); idx = 0;
  CatEp ep = {0}; ep.temporada = 1; ep.episodio = 1;
  cat_definir_episodios(0, &ep, 1);
  FILE *f = fopen(path, "w"); assert(f);
  fputs("blurUnwatchedEpisodes 0\n", f); fclose(f); ajustes_dir(dados_dir());
  assert(epOculto(&ep)); // unknown history must not leak a spoiler
  vistoep_definir(itemTeste.imdb, 1, 1, 1);
  assert(!epOculto(&ep));
  vistoep_definir(itemTeste.imdb, 1, 1, 0);
  assert(epOculto(&ep));
  nivel = 1; foco.fileira = SEC_EPISODIOS; foco.coluna = 0;
  saindo = 0; pedReproduzir = 0;
  SDL_Event e = {0}; e.type = SDL_KEYUP; e.key.keysym.sym = SDLK_RETURN;
  okDesceEm = SDL_GetTicks() - NV_HOLD_MS - 2;
  detail_evento(&e);
  assert(!epOculto(&ep) && !pedReproduzir);
  ep.episodio = 2; assert(epOculto(&ep)); // reveal belongs to one episode
  CatEp grid[4] = {{0}};
  grid[0].temporada=1; grid[0].episodio=1; grid[0].nota=72;
  grid[1].temporada=1; grid[1].episodio=2; grid[1].nota=81;
  grid[2].temporada=2; grid[2].episodio=1; grid[2].nota=90;
  grid[3].temporada=2; grid[3].episodio=2; grid[3].nota=79;
  cat_definir_episodios(0, grid, 4);
  assert(progVizinho(1, 1) == 3);
  assert(progVizinho(2, -1) == 0);
  assert(progVizinho(0, -1) == -1);
  assert(secaoN(SEC_PROGTEMP) == 4);
  char mono[12];
  elencoMonograma("Ryan Gosling", mono, sizeof mono); assert(!strcmp(mono,"RG"));
  elencoMonograma("Óscar", mono, sizeof mono); assert(!strcmp(mono,"Ó"));
  elencoMonograma("", mono, sizeof mono); assert(!strcmp(mono,""));
  assert(!relacionadoVisto(0));
  cat_historico_definir_id(itemTeste.imdb,itemTeste.tipo,1);
  assert(relacionadoVisto(0));
  assert(!strcmp(cabecalhoDe(SEC_RELACIONADOS),"Mais como este"));
  puts("detail21: hero, episodes, progress, cast and recommendations OK");
  SDL_Quit();
  return 0;
}
