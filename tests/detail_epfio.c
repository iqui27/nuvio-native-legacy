// #F110: sonda do fio de episodios — pedidos trocados rapido e encerramento
// com a busca em voo.
//
// O fio de episodios (desc_episodios -> buscarEpsPedido) e detachado e segue
// dentro da libcurl/OpenSSL quando o processo decide terminar. Na base, o
// exit() do fio principal corria OPENSSL_cleanup (atexit) e o _dl_fini com
// o fio vivo dentro delas — SIGSEGV/SIGABRT em ~3/4 das corridas (ver
// CORRECAO-F110). Este teste abre/fecha series em rapida sucessao — cada
// abertura sobe a geracao do pedido e larga o fio anterior em voo — e sai
// com uma busca recem-disparada, o pior caso do relato.
#include "dados.h"
#include "rede.h"
#include <string.h>

// Fora de rede os fios de meta/trailer; os eventos de detalhe seguem reais.
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

int main(void) {
  CatItem itens[4] = {0};
  SDL_Init(SDL_INIT_TIMER);
  dados_iniciar("");
  strcpy(itens[0].imdb, "x00001110");
  strcpy(itens[1].imdb, "x00001111");
  strcpy(itens[2].imdb, "tt0001112");
  strcpy(itens[3].imdb, "tt0001113");
  strcpy(itens[0].tipo, "series");
  strcpy(itens[1].tipo, "series");
  strcpy(itens[2].tipo, "series");
  strcpy(itens[3].tipo, "series");
  cat_definir_tudo(itens, 4, NULL, 0);

  // Pedidos trocados rapido: cada abrir() dispara um pedido novo e o
  // anterior e cedido — os fios desses dois morrem no preCede antes do
  // metaprov (o id nem e "tt"), sem gastar a init da libcurl.
  for (int i = 0; i < 10; i++) {
    abrir(i & 1);
    detail_fechar_seco();
  }
  // Encerramento com busca em voo: dois pedidos de serie em escalonado —
  // o segundo cede o primeiro, mas nada cancela quem ja desceu para a
  // rede — e o processo sai com fios dentro do curl_global_init/OpenSSL.
  // Na base era ai que o OPENSSL_cleanup (atexit) do exit() corria o fio.
  abrir(2);
  SDL_Delay(2);
  abrir(3);
  SDL_Delay(1);
  puts("detail_epfio: PASS");
  return 0;
}
