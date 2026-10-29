// Explorar 2.0: climas na entrada e, por titulo, a vizinhanca (pessoa, tema,
// recomendados, amigos) com uma trilha de onde a pessoa veio. Os dados vem de
// mapa.c (catalogo local na hora, TMDB num fio, com cache); este modulo so
// desenha e navega.
#ifndef NV_EXPLORAR_H
#define NV_EXPLORAR_H

#include <SDL2/SDL.h>
#include "mapa.h"

void explorar_iniciar(void);
// Comeca a toca no titulo `o` (o circular "Explorar" da pagina do titulo).
// Chamar DEPOIS de explorar_iniciar: Voltar no primeiro degrau pede a pagina
// do titulo de volta (explorar_pediu_abrir).
void explorar_abrir_titulo(const MapaObra *o);
void explorar_encerrar(void);
void explorar_evento(const SDL_Event *e);
void explorar_atualizar(float dt, Uint32 agora);
void explorar_desenhar(Uint32 agora);

// Flags de navegacao no mesmo contrato das outras telas: a leitura consome o
// pedido, evitando reabrir o detalhe ou a barra lateral em todos os quadros.
int explorar_quer_sair(void);
int explorar_pediu_relogio(void);
int explorar_pediu_abrir(int *indice);

#endif
