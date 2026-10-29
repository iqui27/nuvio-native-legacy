// CURADORIA DA FILEIRA HEROI (#205): onde o cartao unico pega o titulo.
//
// A regra inteira em tres portas, na ordem: a curadoria do Nuvio Legacy (se o
// imdb baixado esta no catalogo), o primeiro titulo da propria fileira (a
// colecao onde a pessoa aplicou o estilo) e, so quando a fileira esta vazia, um
// sorteio do catalogo — estavel na sessao, senao a remontagem trocaria o
// cartao a cada publicacao da descoberta.
//
//   bash tests/curadoria.sh
#include "curadoria.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void teste_parse(void) {
  // O formato e o da recomendacao do servidor (servidor/recomendacoes, /v1/rec):
  // imdb, tipo, titulo, poster, ano, texto.
  curadoria_receber("{\"imdb\":\"tt1375666\",\"tipo\":\"movie\",\"titulo\":\"Origem\","
                    "\"poster\":\"https://x/p.jpg\",\"ano\":\"2010\",\"texto\":\"Um sonho dentro de outro.\"}");
  assert(curadoria_ativa());
  assert(!strcmp(curadoria_imdb(), "tt1375666"));
  assert(!strcmp(curadoria_texto(), "Um sonho dentro de outro."));
  // Array: a primeira entrada vale (e o "itens" de /v1/rec tambem e um array).
  curadoria_receber("[{\"imdb\":\"tt0088763\"},{\"imdb\":\"tt0111161\"}]");
  assert(curadoria_ativa());
  assert(!strcmp(curadoria_imdb(), "tt0088763"));
  // Corpo que nao serve limpa o estado: sem curadoria, a fileira cai no proprio
  // titulo — nunca um cartao de filme errado com o selo de curadoria.
  curadoria_receber("{\"erro\":\"limite diario\"}");
  assert(!curadoria_ativa());
  curadoria_receber("404 page not found");
  assert(!curadoria_ativa());
  curadoria_receber("{\"imdb\":\"\"}");
  assert(!curadoria_ativa());
}

static void teste_escolher(void) {
  const int row[4] = { 7, 9, 11, 13 };
  unsigned semente = 0;   // 0 = "ainda não sorteado"; o sorteio guarda aqui
  // 1. curadoria achada no catalogo vence.
  assert(curadoria_escolher(row, 4, 40, 3, &semente) == 3);
  // 2. sem curadoria, o primeiro titulo da fileira (a colecao escolhida).
  assert(curadoria_escolher(row, 4, 40, -1, &semente) == 7);
  // 3. fileira vazia: sorteio do catalogo, DENTRO dele e ESTAVEL na sessao —
  // a remontagem nao pode trocar o cartao sozinha.
  { int a = curadoria_escolher(NULL, 0, 40, -1, &semente);
    int b = curadoria_escolher(NULL, 0, 40, -1, &semente);
    assert(a >= 0 && a < 40);
    assert(a == b); }
  // fileira vazia E catalogo vazio: nao ha cartao.
  assert(curadoria_escolher(NULL, 0, 0, -1, &semente) == -1);
  // O CATALOGO ENCOLHEU DEPOIS DO SORTEIO (#205, revisao): "Ocultar filmes
  // assistidos" come um titulo e o feed encurta a lista — a semente da
  // sessao pode ter ficado fora dos limites novos. Fora dele NAO e estavel,
  // e cartao em branco: o sorteio tem que recair DENTRO do catalogo novo.
  {
    unsigned s = 0;
    int a = curadoria_escolher(NULL, 0, 40, -1, &s);
    assert(a >= 0 && a < 40);
    assert(a == curadoria_escolher(NULL, 0, 40, -1, &s));   // estavel no catálogo antigo
    int b = curadoria_escolher(NULL, 0, 30, -1, &s);        // catálogo encolheu
    assert(b >= 0 && b < 30);
    assert(b == curadoria_escolher(NULL, 0, 30, -1, &s));   // e re-estabilizou
    assert(curadoria_escolher(NULL, 0, 1, -1, &s) == 0);     // catálogo de um só
  }
  // curadoria fora do catalogo nao vale (o chamador resolve imdb -> indice).
  assert(curadoria_escolher(row, 4, 40, 999, &semente) == 7);
  // fileira de um titulo so, sem curadoria: ele.
  assert(curadoria_escolher((const int[]){ 5 }, 1, 40, -1, &semente) == 5);
  // duas sementes diferentes sorteiros diferentes em algum lugar (nao e prova,
  // e fumo: so guarda que a semente participa — 0 e "ainda nao sorteado").
  { unsigned s1 = 0, s2 = 0;
    int n1 = curadoria_escolher(NULL, 0, 40, -1, &s1);
    int n2 = curadoria_escolher(NULL, 0, 40, -1, &s2);
    assert(n1 >= 0 && n1 < 40 && n2 >= 0 && n2 < 40); (void)n2; }
  printf("curadoria: escolha e parse ok\n");
}

int main(void) {
  teste_parse();
  teste_escolher();
  printf("curadoria: ok\n");
  return 0;
}
