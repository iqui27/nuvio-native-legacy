#ifndef NV_CURADORIA_H
#define NV_CURADORIA_H

// A CURADORIA DA FILEIRA HEROI (#205). A fileira "Herói" da Home é um cartão
// único do tamanho da fileira; este módulo é a terceira fonte dele: uma
// recomendação de filme que o Nuvio Legacy manda, lida de um JSON estático.
//
// POR QUE ARQUIVO E NÃO O SERVIDOR DE RECOMENDAÇÕES: /v1/rec do
// servidor/recomendacoes é a recomendação ENTRE AMIGOS — quem manda é uma
// pessoa da roda de contatos, com sessão. A curadoria é o DONO mandando um
// filme para todo mundo, e isso ainda não tem rota. Enquanto não existe, a
// TV lê um JSON de uma URL escolhida em Ajustes ("Endereço da curadoria",
// curadoria.txt): a TV nunca empurra nada para lá, só lê. Sem URL ou sem
// resposta, a fileira cai no primeiro título dela e, vazia, no sorteio do
// catálogo (a regra inteira está em curadoria_escolher).
//
// O FORMATO é o do item de /v1/rec — as chaves que o servidor já manda:
//   {"imdb":"tt1375663","tipo":"movie","titulo":"Interstellar",
//    "poster":"…","ano":"2014","texto":"Por que assistir…"}
// Um array com várias entradas também serve: vale a primeira. O campo
// "texto" é o que o cartão desenha nas duas linhas.
//
// DISCIPLINA igual à do canal do dono (avisos.c): rede em fio próprio,
// estado atrás de mutex, o laço de desenho só lê.

// O índice no catálogo do título do cartão, na ordem que a fileira Herói
// usa: `curadoria` (>= 0) quando a curadoria baixada está no catálogo;
// senão o PRIMEIRO item da fileira (`itens`/`n`); senão um sorteio estável
// do catálogo inteiro; -1 quando não há nada. `semente` guarda o sorteio da
// sessão — 0 sorteia e guarda (índice + 1), o valor guardado é devolvido nas
// chamadas seguintes: a remontagem da home não pode trocar o cartão sozinha.
int curadoria_escolher(const int *itens, int n, int catN, int curadoria,
                       unsigned *semente);

// Uma resposta lida (a do fio de rede e a do teste). 1 se valeu.
// Objeto único ou array; sem imdb limpa o estado (a fileira cai na fonte
// própria e o selo não mente "Curadoria" para um título que não veio daí).
int  curadoria_receber(const char *corpo);
int  curadoria_ativa(void);          // 1 com imdb válido na mão
const char *curadoria_imdb(void);   // "" sem curadoria
const char *curadoria_texto(void);  // as duas linhas do cartão ("" sem)

// Contador que sobe a cada mudança de estado: a home o compara para
// remontar a fileira quando a recomendação chega depois do catálogo.
unsigned curadoria_revisao(void);

// O endereço escolhido em Ajustes (curadoria.txt na pasta de dados, por
// aparelho). "" = desligado.
const char *curadoria_url(void);
int  curadoria_definir_url(const char *url);

// Um passo por quadro da home: inicia a leitura se há endereço, refaz a
// cada NV_CURADORIA_MIN minutos. Sem endereço, nada acontece.
void curadoria_passo(void);
#endif
