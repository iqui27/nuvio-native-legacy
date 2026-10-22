/* "O que o Nuvio faz?": tabela de cartoes do tour. Sem UI, rede ou sync. */
#include "../src/ajustes.c"
#include <assert.h>

static int naTela(int op) {
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return 1;
  return 0;
}

int main(void) {
  int n = tourN();
  assert(n >= 10);
  for (int i = 0; i < n; i++) {
    int op = tourOp(i);
    assert(op >= 0 && op < AJ_N);          /* cada cartao tem op valido */
    assert(naTela(op));                    /* e a opcao existe na lista de Ajustes */
    assert(tourTitulo(i)[0] && tourFrase(i)[0]);
    for (int j = 0; j < i; j++) assert(tourOp(j) != op);
  }
  /* "Ligar agora": foco na opcao + edicao aberta quando a opcao existe aqui. */
  for (int i = 0; i < n; i++) {
    if (!tourDisponivel(i)) continue;
    uxAbrirOp = -1; uxAbrirEdicao = 0;
    tourLigar(i);
    assert(uxAbrirOp == tourOp(i) && uxAbrirEdicao == 1);
    uxAbrirOp = -1; uxAbrirEdicao = 0;
    tourOnde(i);
    assert(uxAbrirOp == tourOp(i) && uxAbrirEdicao == 0);
  }
  /* "Novo" por 30 dias a contar da primeira vez que o tour foi visto. */
  assert(tourNovoJanela(1000, 1000 + 29 * 86400));
  assert(!tourNovoJanela(1000, 1000 + 31 * 86400));
  puts("ok");
  return 0;
}
