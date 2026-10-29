#ifndef NV_DICASCONTROLE_H
#define NV_DICASCONTROLE_H
#include <SDL2/SDL.h>

// Content version: bump the marker only when a new set of tips ships.
#define DICASCONTROLE_ARQUIVO "dicas-controle-205.txt"

// Arte do controle e gestos de cada plataforma: Google TV (Android), Magic
// Remote (webOS), Smart Remote SolarCell (Samsung .tpk/.wgt) e toque.
enum { DICAS_TV_GTV, DICAS_TV_LG, DICAS_TV_SAM, DICAS_TOQUE };

void dicascontrole_depois_novidades(void);
void dicascontrole_home_livre(int livre, float dt);
void dicascontrole_abrir(void);
int dicascontrole_aberto(void);
int dicascontrole_visivel(void);
int dicascontrole_ocupado(void);
int dicascontrole_pagina(void);
int dicascontrole_modo(void);
const char *dicascontrole_tecla(int pagina);
void dicascontrole_evento(const SDL_Event *e);
void dicascontrole_atualizar(float dt, Uint32 agora);
// Desenha por cima do app (app_desenhar chama por ultimo): a dica abre o
// componente DE VERDADE pelo caminho normal e so acrescenta o controle, a
// legenda e os pontinhos.
void dicascontrole_desenhar(Uint32 agora);
// Screenshot harness: -1 restores the actual platform/device mode.
void dicascontrole_teste_modo(int modo);
// 1 = nao abre componente nenhum (testes de logica sem GL).
void dicascontrole_teste_seco(int seco);
void dicascontrole_teste_gravar(int ligar);
int dicascontrole_teste_teclas(const char **rot, int *on, int max);
#endif
