#ifndef NV_ADDONMODO_H
#define NV_ADDONMODO_H
#include <stdint.h>
typedef enum { AM_AUTO, AM_P2P, AM_DEBRID, AM_DIRETO } AddonModo;
typedef enum { AG_ERRO, AG_P2P, AG_DEBRID, AG_OFFLINE, AG_401, AG_403, AG_HDR, AG_VAZIO } AddonGuiaRazao;
typedef enum { AGA_BUSCAR, AGA_P2P, AGA_CONECTAR, AGA_PROXIMA, AGA_CONFIG } AddonGuiaAcao;
uint64_t addonmodo_id(const char *base);
void addonmodo_carregar(const char *conta, int perfil);
void addonmodo_preparar(const char *conta, int perfil);
AddonModo addonmodo_get(uint64_t id);
int addonmodo_set(uint64_t id, int modo);
int addonmodo_visto(uint64_t id);
int addonmodo_marcar(uint64_t id);
/* Last real request only. Unknown/failed responses never become catalog-only. */
void addonmodo_resposta(uint64_t id, int http, int valida, int diretos, int torrents);
void addonmodo_nova_busca(void);
void addonmodo_fatos(uint64_t id, int *http, int *valida, int *diretos, int *torrents);
int addonmodo_permite(uint64_t id, int direto, int debrid, int p2p);
AddonGuiaRazao addonguia_razao(int http, int offline, int p2p, int debrid, int hdr, int vazio);
AddonGuiaAcao addonguia_acao(AddonGuiaRazao r, int motor, int proxima);
#endif
