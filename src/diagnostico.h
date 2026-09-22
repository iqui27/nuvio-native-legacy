// Diagnostico e otimizacao sob demanda, executado na TV.
#ifndef NV_DIAGNOSTICO_H
#define NV_DIAGNOSTICO_H
#include <SDL2/SDL.h>

typedef enum {
  DIAG_QUALIDADE = 0,
  DIAG_DESEMPENHO = 1
} DiagnosticoModo;

void diagnostico_iniciar(void);
void diagnostico_recuperar_checkpoint(void);
void diagnostico_evento(const SDL_Event *e);
void diagnostico_atualizar(float dt, Uint32 agora);
void diagnostico_desenhar(Uint32 agora);
void diagnostico_intro_primeira_vez(void);
int  diagnostico_intro_aberto(void);
void diagnostico_intro_evento(const SDL_Event *e);
void diagnostico_intro_atualizar(float dt, Uint32 agora);
void diagnostico_intro_desenhar(Uint32 agora);
int  diagnostico_quer_sair(void);
void diagnostico_encerrar(void);

#endif
