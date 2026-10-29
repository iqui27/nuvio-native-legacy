// #595: a watched action belongs to the title where it was requested.
// The router can open the next title before consuming queued detail actions.
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <string.h>

// Keep metadata/trailer workers offline while exercising real detail events.
int rede_pedir(const RedePedido *pedido, RedeResposta *resposta) {
  (void)pedido;
  memset(resposta, 0, sizeof *resposta);
  resposta->erro = REDE_INDISPONIVEL;
  return 0;
}
#include "../src/detail.c"

static void abrir(int indice) {
  HomeItem hi = {0};
  hi.indice = indice;
  detail_abrir(&hi);
}

static void marcar(void) {
  SDL_Event e = {0};
  maisAcoes = 1;
  for (botao = 0; botao < nBotoes(); botao++)
    if (acaoEm(botao) == ACAO_ASSISTIDO) break;
  assert(botao < nBotoes());
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  detail_evento(&e);
  e.type = SDL_KEYUP;
  detail_evento(&e);
  assert(pedAssistido);
}

int main(void) {
  CatItem filmes[2] = {0};
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  SDL_Delay(1); // OK timestamps use zero as "not pressed".
  dados_iniciar("");
  strcpy(filmes[0].imdb, "tt0000595");
  strcpy(filmes[1].imdb, "tt0000596");
  strcpy(filmes[0].tipo, "movie");
  strcpy(filmes[1].tipo, "movie");
  cat_definir_tudo(filmes, 2, NULL, 0);

  abrir(0);
  assert(!detail_pediu_assistido());
  marcar();
  assert(detail_pediu_assistido()); // An explicit action still works once.
  assert(!detail_pediu_assistido());

  marcar();
  abrir(1); // Same router order: title switch, then watched request consumption.
  assert(detail_indice() == 1);
  assert(!detail_pediu_assistido());
  assert(!cat_visto(cat_item(1)));

  // The same stale request must not survive reopening or opening a series.
  marcar();
  detail_fechar_seco();
  strcpy(filmes[0].tipo, "series");
  cat_definir_tudo(filmes, 2, NULL, 0);
  abrir(0);
  assert(!detail_pediu_assistido());
  assert(!cat_visto(cat_item(0)));
  puts("detail_assistido: PASS");
  return 0;
}
