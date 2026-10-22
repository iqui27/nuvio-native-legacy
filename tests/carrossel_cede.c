// A pre-busca de meta/episodios do carrossel CEDE ao pedido do titulo que
// chegou: o fio de episodios e um so, e o pedido de verdade ficava guardado
// (pendItem) ate a pre-busca acabar. Sem rede (ids que nenhum servico conhece):
// o que se confere e o log "pre-busca de <id> cedeu" e que o pedido guardado
// sai depois. Nao existe versao deste teste para o pai: desc_episodios_precarregar
// e do conserto.
#include "catalogo.h"
#include "descoberta.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  CatItem v[3];
  char log[] = "/tmp/nuvio-cede-XXXXXX", linha[512];
  int i, achou = 0;
  FILE *f;
  for (i = 0; i < 3; i++) {
    memset(&v[i], 0, sizeof v[i]);
    snprintf(v[i].imdb, sizeof v[i].imdb, "tt99997%02d", i);
    snprintf(v[i].titulo, sizeof v[i].titulo, "Serie %d", i);
    snprintf(v[i].tipo, sizeof v[i].tipo, "series");
  }
  cat_definir_tudo(v, 3, NULL, 0);
  assert(mkstemp(log) >= 0);
  fflush(stdout);
  assert(freopen(log, "w", stdout));
  assert(desc_episodios_precarregar(1) == 1);
  desc_episodios(2, 0);                     // o titulo que chegou: fica guardado e a pre-busca cede
  for (i = 0; i < 800 && (desc_episodios_carregando(1) || desc_episodios_carregando(2)); i++) {
    desc_episodios_pendente();
    SDL_Delay(25);
  }
  fflush(stdout);
  f = fopen(log, "r");
  while (f && fgets(linha, sizeof linha, f))
    if (strstr(linha, "pre-busca de tt9999701 cedeu")) achou = 1;
  if (f) fclose(f);
  fprintf(stderr, achou ? "ok: a pre-busca cedeu ao pedido do titulo em cena\n"
                        : "FALHA: a pre-busca nao cedeu (pedido de verdade esperou)\n");
  _exit(achou ? 0 : 1);
}
