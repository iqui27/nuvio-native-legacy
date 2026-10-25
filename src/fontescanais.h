#ifndef NV_FONTESCANAIS_H
#define NV_FONTESCANAIS_H
#include <time.h>
#include <stddef.h>
#include "perfisessao.h"

#define FONTESCANAIS_MAX 64
typedef struct {
  char id[128], nome[200], conta[120], programa[240];
  time_t inicio, fim;
} FonteCanal;
typedef struct {
  FonteCanal canais[FONTESCANAIS_MAX];
  char titulo[256], escolha[128];
  int ativo, n, buscando, atualizar;
  NvPerfilSessao perfil;
} FontesCanais;
/* UI-owned choices, independent of the active playback Stream list. */
const FontesCanais *fontescanais_estado(void);
void fontescanais_iniciar(const char *titulo);
void fontescanais_limpar(void);
void fontescanais_publicar(const FonteCanal *lista, int n, int buscando);
int fontescanais_escolher(int indice, time_t agora);
int fontescanais_consumir(char *id, size_t n);
void fontescanais_atualizar(void);
int fontescanais_pediu_atualizar(void);
#endif
