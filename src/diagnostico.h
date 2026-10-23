// Diagnostico e otimizacao sob demanda, executado na TV.
#ifndef NV_DIAGNOSTICO_H
#define NV_DIAGNOSTICO_H
#include <SDL2/SDL.h>

typedef enum {
  DIAG_QUALIDADE = 0,
  DIAG_DESEMPENHO = 1
} DiagnosticoModo;

void diagnostico_iniciar(void);
// Pede que o PROXIMO diagnostico_iniciar abra direto no teste de velocidade,
// sem apresentacao nem objetivo; o Voltar do resultado sai da tela. Chamar
// antes de trocar para TELA_DIAGNOSTICO (o atalho de Ajustes, em app.c).
void diagnostico_abrir_velocidade(void);
// UI thread: stop an active speed test before opening playback/mini player.
void diagnostico_cancelar_vazao(void);
void diagnostico_recuperar_checkpoint(void);
void diagnostico_evento(const SDL_Event *e);
void diagnostico_atualizar(float dt, Uint32 agora);
void diagnostico_desenhar(Uint32 agora);
void diagnostico_intro_primeira_vez(void);
int  diagnostico_intro_aberto(void);
#ifdef NV_TOUCH_UI
int  diagnostico_apresentacao_aberta(void);
#endif
// Tira a apresentacao global desta sessao (quem ja falou do diagnostico, o
// cartao da 1.4.2). marcarVista=1 tambem a grava como vista: a tela abre
// direto na escolha do objetivo, sem o "Antes de comecar".
void diagnostico_intro_dispensar(int marcarVista);
void diagnostico_intro_evento(const SDL_Event *e);
void diagnostico_intro_atualizar(float dt, Uint32 agora);
void diagnostico_intro_desenhar(Uint32 agora);
int  diagnostico_quer_sair(void);
void diagnostico_encerrar(void);
// Testes do ponteiro (#99): o foco das setas (modo/linha do objetivo, botao
// do resultado, botao do teste de velocidade); devolve o estado (0/1/2).
int  diagnostico_teste_foco(int *modo, int *linha, int *botao, int *vzBotao);

#endif
