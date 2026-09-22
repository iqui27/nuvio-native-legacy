// TECLADO DE TELA, a superficie de digitacao compartilhada.
//
// POR QUE EXISTE: a tela de busca ja tinha uma grade 6x6 de `a-z0-9`
// (busca.c:106), feita exatamente para este alfabeto e para o D-pad. Quando o
// codigo de pareamento precisou de digitacao, a saida obvia era copiar aquela
// grade para dentro da tela de amigos — e uma segunda grade e o comeco de duas
// que divergem: uma ganha a tecla de apagar maior, a outra nao; uma troca de
// alfabeto, a outra fica para tras.
//
// O QUE E COMPARTILHADO DE VERDADE: o ALFABETO (teclado_alfabeto, que busca.c
// tambem usa) e esta MODAL. A grade embutida na tela de busca continua la
// porque ela nao e modal — ela divide o foco com as fileiras de resultado a
// direita, e transformar aquilo numa camada por cima mudaria a tela de busca
// inteira, que nao e o assunto de quem so quer digitar seis caracteres.
//
// SEM ESPACO, de proposito. Esta modal nasceu para codigo de pareamento: seis
// caracteres de `a-z0-9`, sem espaco possivel. A ultima fileira e
// apagar/limpar/pronto. Quando o texto livre da recomendacao entrar, o espaco
// entra como quarta tecla dessa fileira — nao como uma segunda modal.
#ifndef NV_TECLADO_H
#define NV_TECLADO_H
#include <SDL2/SDL.h>

// Os 36 caracteres da grade, em ordem de leitura (6 fileiras de 6).
const char *teclado_alfabeto(void);

// 64 e nao 24: o endereco de um portal IPTV ("meu-portal.exemplo.tv:8080") nao
// cabe em 24, e o campo de texto que a modal ja usa quando as caixas ficam
// estreitas rola pelo fim — texto longo sempre foi desenhavel aqui.
#define TECLADO_MAX 64

// Abre a modal. `titulo` e a linha de cima ("Código do amigo"), `dica` a linha
// de apoio logo abaixo, e `max` o teto de caracteres (limitado a TECLADO_MAX).
// As duas frases passam por i18n no desenho, como todo texto do app.
void teclado_abrir(const char *titulo, const char *dica, int max);

// Mesma modal com ALFABETO proprio e valor inicial. `alfabeto` NULL cai no
// padrao a-z0-9; um alfabeto maior que 36 ganha fileiras (ate 7 de caractere)
// e, passando de 42, colunas (13, depois ate 20 — ver TE_COLS_LONGO em
// teclado.c e a #88); um menor encolhe a grade. `inicial` NULL comeca vazio.
//
// Existe porque endereco de portal precisa de ponto, dois pontos e hifen, e
// MAC precisa so de 0-9a-f e dois pontos — grades diferentes, uma modal so. Ver
// a nota no topo: a saida errada seria uma segunda modal.
void teclado_abrir_com(const char *titulo, const char *dica, int max,
                       const char *alfabeto, const char *inicial);
int  teclado_aberto(void);
void teclado_evento(const SDL_Event *e);
void teclado_atualizar(float dt, Uint32 agora);
void teclado_desenhar(Uint32 agora);

// O que aconteceu, CONSUMIDO NA LEITURA (mesmo contrato de
// recomenda_pediu_abrir): quem pergunta duas vezes recebe TECLADO_NADA na
// segunda, e nao age duas vezes sobre o mesmo OK.
enum { TECLADO_NADA = 0, TECLADO_PRONTO, TECLADO_CANCELOU };
int  teclado_resultado(void);
// O que foi digitado. Continua valido depois de teclado_resultado(); so a
// proxima abertura o zera.
const char *teclado_texto(void);

#endif
