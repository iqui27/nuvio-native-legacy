// CARTAO DE CONSENTIMENTO DO ENVIO AUTOMATICO DE REGISTRO (Tizen).
//
// O dono (20/09/2026): "temos que pegar todos os logs para resolver a Samsung".
// O app nunca manda registro sem a pessoa saber (avisos.h): aqui ela e
// perguntada UMA vez, na primeira abertura desta versao, e a resposta vira o
// ajuste "Enviar registros sozinho" (Sobre), que ela pode mudar depois. OK =
// sim, Voltar = agora nao. So existe no alvo Tizen: e la que falta dado.
#ifndef NV_TELEMETRIA_H
#define NV_TELEMETRIA_H
#include <SDL2/SDL.h>

void telemetria_primeira_vez(void);
int  telemetria_aberto(void);
void telemetria_evento(const SDL_Event *e);
void telemetria_atualizar(float dt, Uint32 agora);
void telemetria_desenhar(Uint32 agora);

#endif
