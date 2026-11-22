// CARTAO DE CONSENTIMENTO DO REGISTRO DE COMPATIBILIDADE (Vai tocar nesta TV,
// 4.2). O app nunca manda nada sem a pessoa saber: aqui ela e perguntada UMA
// vez por instalacao, e a resposta vira o ajuste "Compartilhar
// compatibilidade" (Sobre), que comeca desligado e que ela muda quando quiser.
// O foco inicial e "Agora não"; Voltar tambem recusa. Todas as plataformas.
#ifndef NV_VTREGCARTAO_H
#define NV_VTREGCARTAO_H
#include <SDL2/SDL.h>

void vtregcartao_primeira_vez(void);
int  vtregcartao_aberto(void);
void vtregcartao_evento(const SDL_Event *e);
void vtregcartao_atualizar(float dt, Uint32 agora);
void vtregcartao_desenhar(Uint32 agora);
int  vtregcartao_teste_foco(void);   // 0 = "Sim, pode mandar", 1 = "Agora não"
#ifdef VTREGCARTAO_TESTE
void vtregcartao_teste_abrir(void);
void vtregcartao_teste_zerar(void);
#endif

#endif
