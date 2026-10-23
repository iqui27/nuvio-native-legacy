// ITEM 4 DA PESQUISA PLX: recusa no ponto do pedido.
//
// POR QUE ESTE TESTE EXISTE. A pesquisa (docs/pesquisa/plx-native.md, secao 4)
// mediu no aparelho que recusar NO PEDIDO derruba drops de 90/97 para 21/23 e
// sobe a mediana de 55 para 60 fps; "rate-limit depois do decode" e "um em voo"
// ficaram PIORES que nada, porque o pedido e o ultimo ponto onde ainda da para
// recusar de graca. A recusa so pode valer para pedido NOVO: arte conhecida
// passa (a recusa fixa deixava tiles escuros, medido por eles). Dois niveis:
//   2 = arte TOTALMENTE fora da tela  -> recusa sempre (nao ha tile a escurecer)
//   1 = cartao na BORDA (fade)        -> recusa so em MOVIMENTO
//
// Aqui o cache e exercitado sem rede e sem GPU: so o caminho do pedido.
#include "../src/sdlcompat.h"
#ifdef NV_PLX_BEFORE
#include "/Volumes/ExternalSSD/tmp/jobtmp/plx-recusa-before.c"
#else
#include "../src/tex_cache.c"
#endif
#include <assert.h>
#ifdef NV_PLX_BEFORE
// Sem o item 4 na arvore: nada recusa, e o teste tem de FALHAR.
int tex_n_recusa = 0;
void tex_grade_movendo(int m) { (void)m; }
void tex_pedido_fora(int n) { (void)n; }
#endif

static void prepararApi(void) {
  memset(itens, 0, sizeof itens);
  nMax = 8;
  filaIni = filaFim = 0;
  decIni = decFim = 0;
  mtx = SDL_CreateMutex();
  cond = SDL_CreateCond();
  condDec = SDL_CreateCond();
  condLivre = SDL_CreateCond();
  rodando = 1;
  orcamento = 512L * 1024 * 1024;
  assert(mtx && cond && condDec && condLivre);
}

static int ocupados(void) {
  int n = 0;
  for (int i = 0; i < nMax; i++) if (itens[i].estado != VAZIO) n++;
  return n;
}

static int naFila(void) { return filaIni != filaFim; }

// Um pedido NOVO de arte `p`, com o estado atual da grade; devolve a recusa.
static int pedir(const char *p) {
  tex_n_recusa = 0;
  int r = tex_obter(p);
  assert(r == 0);
  return tex_n_recusa;
}

int main(void) {
  prepararApi();
  assert(ocupados() == 0 && !naFila());

  // 1) TOTALMENTE FORA DA TELA: recusado ANTES do fetch, com a grade PARADA.
  //    Nada entra no cache nem na fila de rede.
  tex_grade_movendo(0);
  tex_pedido_fora(2);
  assert(pedir("https://x/fora-parado.jpg") == 1);
  assert(ocupados() == 0 && !naFila());
  tex_pedido_fora(0);

  // 2) TOTALMENTE FORA DA TELA em movimento: idem.
  tex_grade_movendo(1);
  tex_pedido_fora(2);
  assert(pedir("https://x/fora-movendo.jpg") == 1);
  assert(ocupados() == 0 && !naFila());
  tex_pedido_fora(0);

  // 3) NA BORDA (fade) + grade PARADA: NAO recusa — recusar aqui deixaria o
  //    vizinho escuro para sempre, porque parado nao volta a rolar.
  tex_grade_movendo(0);
  tex_pedido_fora(1);
  assert(pedir("https://x/borda-parado.jpg") == 0);
  assert(ocupados() == 1 && naFila());
  tex_pedido_fora(0);

  // 4) NA BORDA + MOVIMENTO: recusa.
  tex_grade_movendo(1);
  tex_pedido_fora(1);
  assert(pedir("https://x/borda-movendo.jpg") == 1);
  assert(ocupados() == 1);
  tex_pedido_fora(0);

  // 5) CARTAO VISIVEL em movimento: admitido.
  tex_grade_movendo(1);
  tex_pedido_fora(0);
  assert(pedir("https://x/visivel.jpg") == 0);
  assert(ocupados() == 2);

  // 6) ARTE JA CONHECIDA nunca e recusada: e ela que serve a janela. Mesmo
  //    totalmente fora e em movimento, o pedido de um item existente passa.
  tex_grade_movendo(1);
  tex_pedido_fora(2);
  assert(pedir("https://x/visivel.jpg") == 0);
  assert(ocupados() == 2);
  tex_pedido_fora(0);

  puts("recusa: fora-da-tela sempre; borda so em movimento; arte conhecida passa");
  puts("plx_recusa: PASS");
  return 0;
}
