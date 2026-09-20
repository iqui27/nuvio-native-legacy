// TENDENCIA: quanto um titulo SUBIU ou DESCEU numa fileira desde a ultima
// visita, para o card aberto na home dizer "↑ 3" / "↓ 2" / "Novo".
//
// Pedido do dono (20/09/2026): "quando abrir o card, colocar mais informacoes,
// como nota, se caiu ou desceu no trending — coisas uteis para tomada de
// decisao". Nenhum addon manda a variacao: o Cinemeta e o Trakt devolvem a
// ORDEM de hoje e so. A variacao sai de comparar a ordem de hoje com a de uma
// visita anterior, e e este modulo que guarda a anterior.
//
// DOIS ARQUIVOS por fileira (chave da CatFileira), em dados_dir()/tendencia/:
//   <hash>.hoje      "AAAA-MM-DD\n" + um imdb por linha, a ordem do dia
//   <hash>.anterior  o mesmo, do ultimo dia DIFERENTE
// Ao publicar uma fileira num dia novo, .hoje vira .anterior e a lista nova
// vira .hoje. As variacoes sao sempre contra .anterior — republicar a mesma
// fileira dez vezes no mesmo dia nao zera nada.
//
// A conta e por dia porque e o que da para explicar ("desde ontem" ou "desde
// a ultima vez"); comparar com a publicacao anterior de 30 min atras diria
// "igual" quase sempre.
#ifndef NV_TENDENCIA_H
#define NV_TENDENCIA_H
#include "catalogo.h"

// Registra a ordem de uma fileira publicada. Chamada de catalogo.c a cada
// cat_definir_tudo; le/grava arquivo — nunca do laco de desenho.
void tend_registrar(const CatFileira *f, const CatItem *itens);

// Variacao de `imdb` na fileira `chave`: 1 quando se sabe, com *delta em
// posicoes (positivo = subiu) e *novo = 1 quando o titulo nao estava na lista
// anterior (delta fica 0). 0 = sem historico para esta fileira.
int  tend_delta(const char *chave, const char *imdb, int *delta, int *novo);

// Data da lista anterior ("AAAA-MM-DD") ou "" — para o card dizer "desde 19/09".
const char *tend_desde(const char *chave);

#endif
