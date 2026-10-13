// GUIA DAS NOVIDADES DA 2.0 (mockup aprovado 05-whatsnew-20.html).
//
// Tela inteira, no lugar do cartao da 1.8.0 como o UNICO "primeira vez" daqui
// em diante: hero (Começar o guia / Só o que muda pra mim / Ver depois), dez
// capitulos com uma cena viva a esquerda (OK troca o estado da cena), a faixa
// de capitulos por tema, o resumo "O que muda pra você" em tres colunas, a
// tela final apontando para Ajustes › Sobre e ajuda › Guia de uso e o dialogo
// "Sair do guia?".
//
// QUANDO ABRE (decisao do dono, 05/10): UMA vez, na primeira abertura da 2.0,
// para TODO MUNDO — quem atualiza e quem instala do zero (este ganha tambem o
// botao "Abrir o Guia de uso" na tela final). A marca N20_ARQ e gravada quando
// o guia abre: "Ver depois" e "Sair" nao o trazem de volta sozinho. Depois, so
// por Ajustes › Sobre e ajuda › Novidades 2.0 (AJ_NOVIDADES20).
//
// O APARELHO NAO E ESCOLHIDO: vem da build (ptv_plataforma, NV_TPK40). O que
// nao existe aqui aparece apagado com "Não neste aparelho".
#ifndef NV_NOVIDADES20_H
#define NV_NOVIDADES20_H
#include <SDL2/SDL.h>

#define N20_VERSAO "2.0"
#define N20_ARQ "novidades-20-guia.txt"

void novidades20_dir(const char *dirArte);
// Uma vez, com a home de pe: prepara a fila antiga (novidadesfila_preparar),
// grava a marca da 1.8.0 (o cartao dela nao aparece mais) e abre o guia se a
// 2.0 ainda nao foi vista.
void novidades20_primeira_vez(void);
// Mesma gravacao da primeira_vez (fila antiga vista, marca da 2.0), SEM abrir
// o guia: quem conduz o primeiro uso e o Comecar da 2.1 (comecar.h).
void novidades20_suprimir(void);
int  novidades20_aberto(void);
// `novo` = 1: instalacao nova (a tela final oferece o Guia de uso primeiro).
void novidades20_abrir(int novo);
void novidades20_evento(const SDL_Event *e);
void novidades20_atualizar(float dt, Uint32 agora);
// Desenha o guia (tela inteira) ou o fim do esmaecer de saida por cima do app.
void novidades20_desenhar(Uint32 agora);
int  novidades20_visivel(void);

#define N20_PEDIU_NADA 0
#define N20_PEDIU_GUIA 1   // Ajustes › Sobre e ajuda › Guia de uso
int novidades20_pedido(void);

// ---- Para a captura e os testes.
enum { N20_DEV_ANDROID = 0, N20_DEV_LG, N20_DEV_TPK, N20_DEV_WGT, N20_NDEV };
enum { N20_HERO = -1, N20_RESUMO = -2, N20_FIM = -3 };
#define N20_NCAP 11
void novidades20_aparelho(int dev);   // forca o aparelho (-1 = o da build)
int  novidades20_dev(void);
int  novidades20_telas(void);         // telas na lista atual (hero..fim)
int  novidades20_tela(void);          // indice da tela atual
int  novidades20_tela_tipo(int i);    // capitulo 0..9 ou N20_HERO/RESUMO/FIM
int  novidades20_estado(int cap);     // estado da cena do capitulo
int  novidades20_estados(int cap);
int  novidades20_essencial(void);
int  novidades20_saindo(void);        // "Sair do guia?" aberto
int  novidades20_foco(void);          // botao em foco (hero, fim ou dialogo)
// 1 = a linha `l` (0..2) ou o "Também" `e` (3..) do capitulo existe aqui.
int  novidades20_disponivel(int cap, int item);
const char *novidades20_cap_nome(int cap);
void novidades20_ir(int tela);        // pula sem transicao (capturas)
#endif
