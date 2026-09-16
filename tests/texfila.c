// A fila do cache de textura: o URGENTE fura, o resto mantem a ordem.
//
// POR QUE ESTE TESTE EXISTE. O #55 do @rawldon mediu `[hero] arte atrasada
// chegou em 12249 ms (prazo e 400)` com `pend=17` no painel de log: a arte que
// ocupa a tela inteira esperava atras de dezessete miniaturas porque a fila era
// FIFO pura. A ordem de uma fila e o tipo de coisa que quebra em silencio — nao
// ha tela que mostre "o hero furou" —, entao ela e exercitada aqui.
#include "../src/tex_cache.c"
#include <assert.h>
#include <stdio.h>

static void encher(int *f, int *ini, int *fim, const int *idx, int n) {
  int k;
  *ini = *fim = 0;
  for (k = 0; k < MAX_ITENS_ABS; k++) itens[k].urgente = 0;
  for (k = 0; k < n; k++) { f[*fim] = idx[k]; *fim = (*fim + 1) % MAX_FILA; }
}

int main(void) {
  int f[MAX_FILA], ini, fim, k;
  int ordem[6] = { 10, 11, 12, 13, 14, 15 };

  // 1. SEM URGENTE E FIFO, que e o comportamento de sempre.
  encher(f, &ini, &fim, ordem, 6);
  for (k = 0; k < 6; k++) assert(tirarFila(f, &ini, fim) == ordem[k]);
  assert(ini == fim);
  puts("ok  sem urgente a fila continua FIFO");

  // 2. O URGENTE DO MEIO SAI PRIMEIRO, e os outros mantem a ordem de chegada.
  //    E a ordem que faz a fileira aparecer da esquerda para a direita.
  encher(f, &ini, &fim, ordem, 6);
  itens[13].urgente = 1;
  assert(tirarFila(f, &ini, fim) == 13);
  { int esperado[5] = { 10, 11, 12, 14, 15 };
    for (k = 0; k < 5; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
  puts("ok  urgente do meio fura e o resto mantem a ordem");

  // 3. URGENTE NA CABECA nao embaralha nada.
  encher(f, &ini, &fim, ordem, 6);
  itens[10].urgente = 1;
  for (k = 0; k < 6; k++) assert(tirarFila(f, &ini, fim) == ordem[k]);
  puts("ok  urgente na cabeca sai como sairia");

  // 4. O ULTIMO da fila tambem fura — e o caso do #55: o hero e pedido DEPOIS
  //    dos posteres da fileira que ja estavam na fila.
  encher(f, &ini, &fim, ordem, 6);
  itens[15].urgente = 1;
  assert(tirarFila(f, &ini, fim) == 15);
  { int esperado[5] = { 10, 11, 12, 13, 14 };
    for (k = 0; k < 5; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
  puts("ok  urgente no fim fura a fila inteira");

  // 5. DOIS URGENTES saem antes dos comuns e entre si respeitam a chegada.
  encher(f, &ini, &fim, ordem, 6);
  itens[12].urgente = 1; itens[14].urgente = 1;
  assert(tirarFila(f, &ini, fim) == 12);
  assert(tirarFila(f, &ini, fim) == 14);
  { int esperado[4] = { 10, 11, 13, 15 };
    for (k = 0; k < 4; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
  puts("ok  dois urgentes saem na ordem de chegada entre si");

  // 6. A FILA QUE DEU A VOLTA no vetor circular. O bug classico deste tipo de
  //    codigo mora aqui: com ini > fim o "puxar para a frente" atravessa o zero.
  { int idx[4] = { 20, 21, 22, 23 };
    ini = fim = MAX_FILA - 2;
    for (k = 0; k < MAX_ITENS_ABS; k++) itens[k].urgente = 0;
    for (k = 0; k < 4; k++) { f[fim] = idx[k]; fim = (fim + 1) % MAX_FILA; }
    assert(ini > fim);                 // deu a volta mesmo
    itens[23].urgente = 1;             // o ultimo, ja depois do zero
    assert(tirarFila(f, &ini, fim) == 23);
    { int esperado[3] = { 20, 21, 22 };
      for (k = 0; k < 3; k++) assert(tirarFila(f, &ini, fim) == esperado[k]); }
    assert(ini == fim); }
  puts("ok  fila circular que deu a volta tambem reordena certo");

  puts("texfila: tudo ok");
  return 0;
}
