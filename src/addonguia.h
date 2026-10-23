#ifndef NV_ADDONGUIA_H
#define NV_ADDONGUIA_H
#include <SDL2/SDL.h>
#include "addonmodo.h"
enum { AG_NOVO_TORRENT=10, AG_NOVO_CATALOGO, AG_NOVO_DIRETO, AG_MODO, AG_CONECTAR, AG_INICIANDO, AG_LISTA };
enum { AGP_NADA, AGP_FONTES, AGP_REPETIR, AGP_PROXIMA, AGP_FILEIRAS };
void addonguia_abrir(int tela, int addon, const char *titulo);
void addonguia_fechar(void);
int addonguia_aberto(void);
void addonguia_evento(const SDL_Event *e);
void addonguia_atualizar(int permitirBoasVindas);
void addonguia_desenhar(void);
int addonguia_pedido(void);
uint64_t addonguia_identidade(void);
void addonguia_progresso(const char *servico, int pct);
void addonguia_lista(int foco);
void addonguia_falha(void);
#ifdef NV_SHOT_HOOKS
void addonguia_shot(int tela);
#endif
#endif
