// TIRAR UM ITEM DE UMA FILEIRA SEM QUEBRAR AS OUTRAS. Issue #22.
//
// As fileiras nao tem vetor proprio: cada uma e uma janela (ini, n) sobre o
// vetor unico de itens (ver CatFileira em catalogo.h). Por isso a operacao
// obvia — compactar o vetor — seria a errada: ela mudaria o `ini` de todas as
// fileiras seguintes de uma vez. O que este teste guarda e exatamente isso.
//
// O QUE ELE PROVA:
//   1. o item sai da fileira certa e a ORDEM dos que ficam nao muda;
//   2. as outras fileiras continuam apontando para os MESMOS titulos — e a
//      assercao e sobre o titulo, nao sobre o indice, porque e o titulo que a
//      pessoa ve na tela;
//   3. a fileira que esvazia SAI da lista, senao a home desenha um cabecalho
//      com nada embaixo;
//   4. um indice que nao esta em fileira nenhuma nao mexe em nada.
//
//   bash tests/catfileira.sh
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// --- DUBLES ------------------------------------------------------------------
// Nenhum deles participa da regra: estao aqui so para catalogo.c linkar sem
// arrastar SDL, rede e descoberta atras.
int         ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

// --- APOIO -------------------------------------------------------------------
#define N_ITENS 9

static CatItem itens[N_ITENS];
static CatFileira fils[3];

// Tres fileiras encostadas, como a montagem as produz:
//   [0..2] retomada   [3..5] populares   [6..8] series
static void montar(void) {
  int i;
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < N_ITENS; i++)
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "T%d", i);
  snprintf(fils[0].chave, sizeof fils[0].chave, "retomada");
  fils[0].ini = 0; fils[0].n = 3;
  snprintf(fils[1].chave, sizeof fils[1].chave, "populares");
  fils[1].ini = 3; fils[1].n = 3;
  snprintf(fils[2].chave, sizeof fils[2].chave, "series");
  fils[2].ini = 6; fils[2].n = 3;
  cat_definir_tudo(itens, N_ITENS, fils, 3);
}

// O titulo que a fileira `r` mostra na posicao `k` — que e o que a pessoa ve.
static const char *na_tela(int r, int k) {
  const CatFileira *f = cat_fileira(r);
  assert(f && k < f->n);
  return cat_item(f->ini + k)->titulo;
}

static const CatFileira *porChave(const char *chave) {
  int r;
  for (r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->chave, chave)) return cat_fileira(r);
  return NULL;
}

int main(void) {
  // A) DO MEIO DA PRIMEIRA FILEIRA. E o caso do #22: a pessoa tira o segundo
  //    card de "Continuar assistindo".
  montar();
  assert(cat_tirar_item_da_fileira(1) == 1);
  assert(porChave("retomada")->n == 2);
  assert(!strcmp(na_tela(0, 0), "T0"));
  assert(!strcmp(na_tela(0, 1), "T2"));   // ordem preservada, T1 sumiu
  // As outras duas continuam mostrando os MESMOS titulos. Se a remocao tivesse
  // compactado o vetor de itens, aqui apareceriam T4 e T7.
  assert(porChave("populares")->n == 3 && !strcmp(na_tela(1, 0), "T3"));
  assert(porChave("series")->n == 3    && !strcmp(na_tela(2, 0), "T6"));
  puts("ok  tira do meio, mantem a ordem e nao desloca as outras fileiras");

  // B) O ULTIMO DA FILEIRA. Nada a deslocar; so a janela encolhe.
  montar();
  assert(cat_tirar_item_da_fileira(2) == 1);
  assert(porChave("retomada")->n == 2);
  assert(!strcmp(na_tela(0, 0), "T0") && !strcmp(na_tela(0, 1), "T1"));
  assert(porChave("populares")->n == 3 && !strcmp(na_tela(1, 0), "T3"));
  puts("ok  tira o ultimo da fileira");

  // C) DE UMA FILEIRA DO MEIO, que e onde um erro de indice apareceria: se o
  //    deslocamento passasse do fim da janela, ele comeria o primeiro item da
  //    fileira seguinte.
  montar();
  assert(cat_tirar_item_da_fileira(3) == 1);
  assert(porChave("populares")->n == 2);
  assert(!strcmp(na_tela(1, 0), "T4") && !strcmp(na_tela(1, 1), "T5"));
  assert(porChave("series")->n == 3 && !strcmp(na_tela(2, 0), "T6"));
  puts("ok  tira da fileira do meio sem comer a seguinte");

  // D) ESVAZIAR A FILEIRA a tira da lista. Uma fileira com zero itens desenha
  //    um cabecalho com nada embaixo.
  montar();
  assert(cat_tirar_item_da_fileira(0) == 1);
  assert(cat_tirar_item_da_fileira(0) == 1);
  assert(cat_tirar_item_da_fileira(0) == 1);
  assert(cat_n_fileiras() == 2);
  assert(porChave("retomada") == NULL);
  assert(porChave("populares") && !strcmp(porChave("populares")->chave, "populares"));
  puts("ok  fileira que esvazia sai da lista");

  // E) INDICE FORA DE QUALQUER JANELA nao mexe em nada. Depois de D) o slot 0
  //    ficou orfao — nenhuma fileira o referencia — e pedir para tira-lo de
  //    novo tem de ser um nao-evento, nao um estrago.
  { int antes = cat_n_fileiras();
    assert(cat_tirar_item_da_fileira(0) == 0);
    assert(cat_n_fileiras() == antes);
    assert(cat_tirar_item_da_fileira(-1) == 0);
    assert(cat_tirar_item_da_fileira(9999) == 0); }
  puts("ok  indice fora de fileira nao mexe em nada");

  puts("catfileira: tudo ok");
  return 0;
}
