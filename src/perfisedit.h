// GERENCIAR PERFIS — o modal "Adicionar perfil" e "Gerenciar perfis" da tela
// de escolha de perfil (2.1).
//
// POR QUE UM MODAL E NAO UMA TELA: ele abre de dois lugares da MESMA tela
// (o cartao "+" no fim da fileira e os botoes de baixo) e Voltar devolve o
// foco exatamente a quem abriu — o perfil focado continua focado, o PIN nao
// perde o lugar. E o mesmo molde de pessoas.c e recenviar.c.
//
// O QUE ELE FAZ, e so isso (o dono: "o botao de gerenciar e adicionar"):
//   Adicionar: nome (teclado) + cor -> sync_push_profiles com a lista inteira.
//   Renomear:  teclado com o nome atual -> push.
//   Cor:       a paleta de 16 do app oficial -> push.
//   Remover:   confirmacao -> sync_delete_profile_data.
// O PIN continua sendo criado no app Nuvio (web/celular), que e onde a conta
// mora; aqui a TV so verifica (perfilsel.c).
//
// A REDE NUNCA NO FIO DO QUADRO: push e delete BLOQUEIAM (perfis.c), entao
// rodam num fio proprio — o mesmo molde do fio do PIN em perfilsel.c. Enquanto
// o fio vive, o modal diz "Salvando..." e engole o teclado; sem rede, o aviso
// e o mesmo da tela de PIN, e nada e gravado.
#ifndef NV_PERFISEDIT_H
#define NV_PERFISEDIT_H
#include <SDL2/SDL.h>

// `adicionar` 1 = abre ja no fluxo de adicionar (o cartao "+"); 0 = na lista.
void perfisedit_abrir(int adicionar);
int  perfisedit_aberto(void);
void perfisedit_fechar(void);
void perfisedit_evento(const SDL_Event *e);
void perfisedit_atualizar(float dt, Uint32 agora);
void perfisedit_desenhar(Uint32 agora);
// Foco da lista, para os testes.
int  perfisedit_foco(void);

#endif
