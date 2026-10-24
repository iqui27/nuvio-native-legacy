// Tela Biblioteca: abas no topo e uma grade de posteres embaixo.
//
// Ela e a unica tela do app onde o conteudo e um CONJUNTO do usuario, nao uma
// vitrine editorial. Por isso as abas nao sao decoracao: "Minha Lista" e
// "Comprados" existem no aparelho como estado de conta e mudam durante o uso,
// e a tela precisa continuar legivel quando esse conjunto esta vazio.
#ifndef NV_BIBLIOTECA_H
#define NV_BIBLIOTECA_H
#include <SDL2/SDL.h>

int  biblioteca_iniciar(void);
void biblioteca_abrir_sessao(void); // foco/fonte transitorios; conserva preferencias
void biblioteca_evento(const SDL_Event *e);
void biblioteca_atualizar(float dt, Uint32 agora);
void biblioteca_desenhar(Uint32 agora);
int  biblioteca_quer_sair(void);   // 1 quando o Back deve fechar a tela
int  biblioteca_pediu_relogio(void);
void biblioteca_encerrar(void);
// Headless navigation checks: actual row/column and selected action.
void biblioteca_teste_foco(int *fileira, int *coluna, int *acao);

// OK pressionado sobre um poster: consome o pedido e devolve 1, escrevendo em
// *indiceCatalogo o indice do item NO CATALOGO (nao a posicao na grade — a
// grade e filtrada, e quem abre o detalhe precisa do item real).
int  biblioteca_pediu_abrir(int *indiceCatalogo);

// Estado de conta, em memoria. Exposto porque quem marca um titulo e a tela de
// detalhe (o botao "+"), nao a biblioteca: sem isto a aba "Minha Lista" so
// poderia ser alimentada por dentro deste modulo, e o "+" do detalhe nao teria
// onde escrever.
int  biblioteca_na_lista(int indiceCatalogo);
void biblioteca_alternar_lista(int indiceCatalogo);
int  biblioteca_comprado(int indiceCatalogo);

#endif
