// Normalizacao do texto da BUSCA, sem dependencia de tela.
//
// Vive fora de busca.c para poder ser testada sozinha (tests/buscanorm.sh) e
// porque ela decide o que a busca LOCAL considera igual — nao o que vai aos
// addons (esses recebem o termo como a pessoa digitou; ver refiltrar).
#ifndef NV_BUSCANORM_H
#define NV_BUSCANORM_H
#include <stddef.h>

// Copia `s` em `destino` sem caixa e sem acento, como UTF-8:
//   - ASCII: minusculo;
//   - latino com acento (Latin-1, Latin Extended-A e as virgulas do romeno
//     ș/ț): a letra base, ASCII;
//   - cirilico (russo, ucraniano, bielorrusso, bulgaro, servio): minusculo, com
//     "ё" dobrado em "е";
//   - qualquer outra escrita passa como veio (antes virava espaco, e um termo
//     em cirilico ou grego virava uma sequencia de espacos que casava com
//     qualquer titulo).
// Byte invalido vira espaco. Sempre termina em NUL, sem cortar no meio de uma
// sequencia UTF-8.
void busca_normalizar(const char *s, char *destino, size_t tam);

// Quantos caracteres (pontos de codigo) tem `s`.
int busca_codepoints(const char *s);

// Tira o ultimo CARACTER de `s` (nao o ultimo byte): a tecla "apagar" nao pode
// deixar meia sequencia UTF-8 no campo. Devolve o novo tamanho em bytes.
size_t busca_apagar_ultimo(char *s, size_t n);

// CURSOR do campo (teclado do app no .tpk): posicao em CARACTERES, -1 = no fim
// (o caso comum, e o que todo texto trocado inteiro volta a ser).
// Byte em que o cursor `cur` cai em `s` (`cur` < 0 ou alem do fim: o fim).
size_t busca_byte_cursor(const char *s, int cur);
// Insere `t` no cursor se couber inteiro em `cap` (com o NUL); o cursor anda
// junto. Devolve o novo tamanho em bytes (o mesmo, se nao coube).
size_t busca_inserir(char *s, size_t n, size_t cap, int *cur, const char *t);
// Tira o caractere ANTES do cursor (no fim, o ultimo); no comeco nao faz nada.
// Devolve o novo tamanho em bytes.
size_t busca_apagar_antes(char *s, size_t n, int *cur);
// Esquerda (d < 0) / Direita: anda um caractere. 0 = ja estava na ponta, e quem
// chama passa o foco adiante (Direita no fim vai ao microfone).
int busca_cursor_mover(const char *s, int *cur, int d);

#endif
