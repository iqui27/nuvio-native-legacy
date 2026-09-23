// Explorar: o ceu das historias da pessoa. O que ela viu vira estrela, o que
// as historias tem em comum vira linha, e entre duas estrelas nasce a proxima.
// Os dados vem de mapa.c (catalogo local na hora, TMDB num fio, com cache);
// este modulo so desenha e navega.
#ifndef NV_EXPLORAR_H
#define NV_EXPLORAR_H

#include <SDL2/SDL.h>

void explorar_iniciar(void);
void explorar_encerrar(void);
void explorar_evento(const SDL_Event *e);
void explorar_atualizar(float dt, Uint32 agora);
void explorar_desenhar(Uint32 agora);

// Flags de navegacao no mesmo contrato das outras telas: a leitura consome o
// pedido, evitando reabrir o detalhe ou a barra lateral em todos os quadros.
int explorar_quer_sair(void);
int explorar_pediu_abrir(int *indice);

#endif
