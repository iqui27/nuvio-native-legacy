// TELA "CACA" (plano social-pessoal, secao 8.3): as cacadas do perfil, o
// progresso de cada uma, os pôsteres (visto em cor, falta apagado), a legenda
// dos degraus e os gestos (Fazer fileira, Fazer colecao, + Documentarios,
// + Curtas, Mostrar aos amigos, Parar), mais "+ Cacar novo" e "+ Nova trilha".
//
// Tela burra: le tudo de caca.h, nao fala com a rede. O app roteia os pedidos
// (abrir titulo, ir para a busca de pessoas) como faz com amigoperfil.
#ifndef NV_CACAUI_H
#define NV_CACAUI_H
#include <SDL2/SDL.h>
#include <stddef.h>

// `foco` = indice da cacada a mostrar (caca.h), -1 = a primeira.
void cacaui_abrir(int foco);
void cacaui_evento(const SDL_Event *e);
void cacaui_atualizar(float dt, Uint32 agora);
void cacaui_desenhar(Uint32 agora);
int  cacaui_quer_sair(void);
// OK num pôster: o imdb do titulo, uma vez.
int  cacaui_pediu_titulo(char *imdb, size_t tam);
// "+ Cacar novo": o app abre a busca (Pessoas). Uma vez.
int  cacaui_pediu_busca(void);

#endif
