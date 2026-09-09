// Escolha de perfil — a primeira tela de quem tem conta.
//
// QUANDO ELA APARECE esta escrito em perfis_precisa_escolher() (perfis.h), que
// e onde a regra pode ser testada. Em uma linha: uma vez por sessao, so quando
// ha mais de um perfil (ou um so, travado por PIN), e nunca para quem nao tem
// conta. Ela nao aparecia a cada arranque; aparecia uma vez por INSTALACAO, e
// numa TV de sala isso faz o app herdar em silencio o perfil de quem desligou o
// aparelho ontem.
//
// ELA NAO ATRASA O ARRANQUE. A home ja foi montada do cache dentro de
// app_iniciar (o marco "catalogo do cache na tela", 844 ms medidos na TV) antes
// de esta tela existir, e app.c continua chamando home_atualizar por tras dela
// — quando a pessoa escolhe, a home ja esta pronta. A lista de perfis tambem
// vem de um cache em disco, entao a tela desenha no primeiro quadro em vez de
// esperar a rede.
//
// O PIN e verificado NO SERVIDOR (verify_profile_pin). Guardar o PIN aqui para
// comparar localmente seria guardar o segredo no aparelho — e um perfil
// travado existe justamente para o aparelho nao poder abri-lo sozinho. O PIN
// digitado nao vai para log nenhum, e nao pode passar a ir.
#ifndef NV_PERFILSEL_H
#define NV_PERFILSEL_H
#include <SDL2/SDL.h>

void perfilsel_iniciar(void);
void perfilsel_evento(const SDL_Event *e);
void perfilsel_atualizar(float dt, Uint32 agora);
void perfilsel_desenhar(Uint32 agora);
int  perfilsel_concluido(void);
int  perfilsel_quer_sair(void);
int  perfilsel_pediu_repetir(void);

#endif
