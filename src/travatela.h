// TRAVA DE SERIE: a modal para criar, ajustar e desfazer a trava de uma serie
// (trava.c tem a regra e o arquivo; esta tela so escolhe).
//
// Uma ilha so, uma coluna, no molde de recenviar.c: os perfis da conta (liga e
// desliga quem entra), o modo (Juntos / Separados), o PIN, a pausa e a acao.
// Sem trava na serie, a acao CRIA e a ilha do relogio confirma com Desfazer.
// Com trava, as linhas mudam a trava na hora e a acao a desfaz.
#ifndef NV_TRAVATELA_H
#define NV_TRAVATELA_H

#include <SDL2/SDL.h>

// Abre para a serie `serie` (id do titulo). `titulo` so enfeita a tela.
// 0 quando a conta tem menos de 2 perfis ou nao ha serie. 1 quando abriu.
int  travatela_abrir(const char *serie, const char *titulo);

int  travatela_aberto(void);
void travatela_evento(const SDL_Event *e);
void travatela_atualizar(float dt, Uint32 agora);
void travatela_desenhar(Uint32 agora);

#endif
