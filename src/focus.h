// Gerenciador de foco espacial com MEMORIA DE COLUNA por fileira.
//
// Memoria de coluna e o detalhe que separa uma navegacao boa de uma irritante:
// ao descer da fileira 1 (coluna 5) para a fileira 2 e voltar, o foco tem que
// retornar a coluna 5, nao a coluna 0. O tvOS faz isso; sem isso o usuario
// perde o lugar toda vez que troca de fileira.
#ifndef NV_FOCUS_H
#define NV_FOCUS_H

// ERA 32, e a grade da Biblioteca e uma fileira de foco POR LINHA de cartazes:
// 30 linhas x 6 = 180 titulos navegaveis, e na exibicao de lista (1 coluna) so
// 30. O que passasse disso existia no contador e nunca aparecia na tela — a
// outra metade do issue do Owlphibia29. 700 linhas cobrem a grade inteira da
// Biblioteca (CAT_MAX + SALVOS_MAX = 4000 titulos / 6). A home tem teto proprio
// (MAX_FIL, 32) e a busca tambem (BU_MAX_FILEIRAS), entao isto so custa memoria:
// ~5,6 KB por Foco, e ha cinco no app.
#define FOCUS_MAX_FILEIRAS 700

typedef struct {
  int fileira;
  int coluna;
  int colunaLembrada[FOCUS_MAX_FILEIRAS];
  int nFileiras;
  int nColunas[FOCUS_MAX_FILEIRAS];
} Foco;

void focus_iniciar(Foco *f, int nFileiras, const int *nColunas);
int  focus_mover(Foco *f, int dx, int dy);   // 1 se moveu

// GRADE: sobe e desce MANTENDO a coluna, sem memoria por fileira.
//
// A memoria de coluna acima e certa para FILEIRAS de conteudo, onde cada uma
// tem um comprimento proprio e a pessoa "guarda o lugar" em cada. Numa GRADE
// ela e um defeito visivel: o teclado da busca tem 6 colunas por fileira, e
// descer da letra "f" (coluna 5) caia na coluna 0 da fileira seguinte, porque
// era ali que o cursor tinha estado por ultimo NAQUELA fileira — no "g" em vez
// do "l". Do sofa isso se le exatamente como o relato do issue #4: "se eu movo
// para baixo ou para cima, pula para uma letra aleatoria". A grade de posteres
// da biblioteca tem o mesmo defeito, pelo mesmo motivo.
//
// Aqui a coluna e PRESERVADA e apenas presa ao fim da fileira de destino
// quando ela e mais curta (a ultima fileira do teclado tem 3 teclas, nao 6).
int  focus_mover_grade(Foco *f, int dx, int dy);

int  focus_indice(const Foco *f, int fileira, int coluna);

#endif
