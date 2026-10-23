// Os vizinhos do carrossel ja tem meta/lista de episodios ANTES de o foco
// chegar, e voltar a um titulo ja visto na sessao nao pede nada de novo.
//
// C9 do dono, 2.0.3 (build e5b54bc4): "[carrossel] titulo N/12" vinha seguido
// de "[meta] series/tt...: catalogo do Nuvio" e "[desc] ... em N episodios" —
// tudo pedido na CHEGADA, sem pre-busca dos vizinhos. (O 2.0.2 fazia o mesmo:
// app.c pedia na chegada e nada olhava adiante; nao e regressao.)
// Antes do conserto as verificacoes 1 e 3 falham.
//
// Inclui detail.c para armar o carrossel direto (sem a home em Dinamica).
#include "catalogo.h"
#include "descoberta.h"
#include "home.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/detail.c"

#define NT 6

static CatItem fazItem(const char *imdb, const char *titulo, const char *tipo) {
  CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  snprintf(c.titulo, sizeof c.titulo, "%s", titulo);
  snprintf(c.tipo, sizeof c.tipo, "%s", tipo);
  return c;
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) detail_atualizar(0.016f, SDL_GetTicks());
}

// Espera o fio de episodios soltar (rede sem resposta para estes ids).
static void espera(void) {
  int k, i, vivo;
  for (k = 0; k < 800; k++) {
    vivo = 0;
    for (i = 0; i < NT; i++) if (desc_episodios_carregando(i)) vivo = 1;
    if (!vivo) return;
    quadros(1);
    SDL_Delay(25);
  }
}

static void publica(int i) {
  CatEp e[3];
  int k;
  memset(e, 0, sizeof e);
  for (k = 0; k < 3; k++) { e[k].temporada = 1; e[k].episodio = k + 1; }
  cat_definir_episodios(i, e, 3);
}

int main(void) {
  CatItem v[NT];
  HomeItem hi;
  int i, falhas = 0;
  for (i = 0; i < NT; i++) {
    char id[24], nome[24];
    snprintf(id, sizeof id, "tt99999%02d", i);
    snprintf(nome, sizeof nome, "Serie %d", i);
    v[i] = fazItem(id, nome, "series");
  }
  cat_definir_tudo(v, NT, NULL, 0);

  // Abre a serie 0 e arma o carrossel a mao: fileira 0..5, posicao 0.
  memset(&hi, 0, sizeof hi);
  hi.indice = 0;
  detail_abrir(&hi);
  carro = 1; carN = NT; carPos = carAplicado = 0; cartao = 1.0f; carOff = 0.0f;
  for (i = 0; i < NT; i++) carIdx[i] = i;
  quadros(1);
  espera();
  publica(0);
  quadros(5);

  // 1) PARADO NO TITULO 0, O VIZINHO DA DIREITA JA FOI PEDIDO.
  if (!desc_episodios_carregando(1)) {
    printf("FALHA 1: vizinho +1 do carrossel nao foi pre-buscado\n");
    falhas++;
  } else printf("ok 1: vizinho +1 pre-buscado\n");
  espera();
  publica(1);

  // 2) CHEGA AO TITULO 1: nada a pedir para ele (ja esta pronto).
  carPasso(1);
  carOff = 1.0f;
  quadros(3);
  if (desc_episodios_carregando(1)) {
    printf("FALHA 2: chegou a um vizinho ja pronto e pediu de novo\n");
    falhas++;
  } else printf("ok 2: chegada ao vizinho pronto nao pede nada\n");

  // 3) DO TITULO 1 O PROXIMO (2) JA FOI PEDIDO.
  if (!desc_episodios_carregando(2)) {
    printf("FALHA 3: vizinho +1 do titulo 1 nao foi pre-buscado\n");
    falhas++;
  } else printf("ok 3: vizinho +1 do titulo 1 pre-buscado\n");
  espera();
  publica(2);

  // 4) VOLTAR AO 0 (JA VISTO): nenhum pedido novo, de ninguem.
  carPasso(-1);
  carOff = 0.0f;
  quadros(10);
  for (i = 0; i < NT; i++)
    if (desc_episodios_carregando(i) && i != 3) {
      printf("FALHA 4: voltar a titulo visto pediu a lista de %d\n", i);
      falhas++;
    }
  if (!falhas) printf("ok 4: voltar a titulo visto nao pede nada\n");
  printf(falhas ? "FALHOU\n" : "FIM\n");
  fflush(stdout);
  _exit(falhas ? 1 : 0);
}
