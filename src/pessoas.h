// ENCONTRAR PESSOAS — a modal de descoberta do "Entre amigos" (busca, sugestoes
// por gosto, cartao de perfil, pedidos de amizade, bloqueio e "Meu perfil").
//
// POR QUE E UMA MODAL E NAO UMA TELA: ela abre de dois lugares que nao sao a
// mesma tela — a aba Social do painel da tecla azul e a linha "Perfil
// pesquisavel" de Ajustes — e precisa devolver o foco a quem abriu. E a mesma
// razao que fez recenviar.c virar modulo (ver recenviar.h): uma camada acima,
// Voltar fecha, nada por baixo perde o lugar.
//
// TUDO O QUE ELA MOSTRA VEM DE recomenda.h; ela nao fala com a rede. As regras
// de privacidade (o que sai, para quem, o que fica desligado) estao la e em
// docs/SOCIAL-PRIVACIDADE.md — aqui so ha a tela.
//
// SEM NUVIO_REC_URL ELA NAO ABRE (pessoas_abrir devolve 0), como recenviar.
#ifndef NV_PESSOAS_H
#define NV_PESSOAS_H

#include <SDL2/SDL.h>

// Abre no menu "Encontrar pessoas". 1 quando abriu.
int  pessoas_abrir(void);
// Abre direto em "Meu perfil".
int  pessoas_abrir_perfil(void);

// O interruptor "Perfil pesquisavel" de Ajustes. Ligar SEM apelido nao liga:
// abre "Meu perfil" pedindo o apelido primeiro (devolve 0). Desligar apaga o
// perfil publico do servidor na hora. Devolve 1 quando o estado pedido vale.
int  pessoas_definir_pesquisavel(int ligado);

int  pessoas_aberto(void);
void pessoas_evento(const SDL_Event *e);
void pessoas_atualizar(float dt, Uint32 agora);
void pessoas_desenhar(Uint32 agora);
// Teste do ponteiro (#99): a linha em foco; em `col` a coluna (1 = pilula).
int  pessoas_teste_foco(int *col);

#endif
