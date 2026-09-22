// Tela de descoberta cinematografica: uma fileira principal e uma ficha
// editorial do titulo sob o foco. O modulo nao busca dados; le o catalogo que
// a Home ja montou, para abrir instantaneo e continuar coerente com o cache.
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
