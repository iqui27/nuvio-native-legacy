// Cartao de NOVIDADES DA 2.0.2 — no padrao aprovado da 1.4.8 (novidades148.c):
// a esquerda uma PREVIA VIVA do que mais mudou (menu de contexto, destaque da
// Home, ilha do relogio), trocando a cada ~4 s; a direita as mudancas em grupos, uma
// linha apagada por item. A ultima pagina e "Apoie o projeto", com os QRs do
// Patreon e do Ko-fi (apoio.h): opcional, nunca no caminho de quem fecha.
//
// QUANDO ABRE: uma vez, na home, para quem JA viu o guia da 2.0. Quem ainda
// vai ver o guia (instalacao nova ou vindo da 1.x) recebe o guia e esta marca
// gravada: dois cartoes seguidos na primeira abertura e demais. Quem ainda nao
// viu o da 2.0.1 NAO o ve: este o substitui (novidades201_primeira_vez).
#ifndef NV_NOVIDADES202_H
#define NV_NOVIDADES202_H
#include <SDL2/SDL.h>

#define N202_VERSAO "2.0.2"
#define N202_ARQ    "novidades-202.txt"

// Pasta da arte do pacote (a mesma de app_iniciar): fundo da previa, sem rede.
void novidades202_dir(const char *dirArte);
// Chamar ANTES de novidades20_primeira_vez no mesmo quadro: a decisao olha a
// marca do guia da 2.0 antes de ele gravar a dele.
void novidades202_primeira_vez(void);
int  novidades202_aberto(void);
void novidades202_abrir(void);
void novidades202_evento(const SDL_Event *e);
void novidades202_atualizar(float dt, Uint32 agora);
void novidades202_desenhar(Uint32 agora);

#define N202_PEDIU_NADA    0
int  novidades202_pedido(void);

// ---- Para a captura e os testes.
int  novidades202_pagina(void);        // 0 = novidades, 1 = apoie o projeto
int  novidades202_foco(void);
int  novidades202_previa_pronta(void); // a arte do fundo da previa carregou
// Pula o relogio da previa para `seg` segundos (cena e momento dela).
void novidades202_teste_relogio(float seg);
void novidades202_teste_esquecer(void);
float novidades202_teste_folga(void);   // rodape - fim da lista, ultimo quadro
int  novidades202_teste_cortadas(void);  // frases com reticencias, ultimo quadro   // testes: a decisao volta a valer
#endif
