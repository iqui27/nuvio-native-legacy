#ifndef NV_AMIGOSPERFIS_H
#define NV_AMIGOSPERFIS_H
#include "perfis.h"
enum { AP_DISPONIVEL, AP_AMIGO, AP_BLOQUEADO };
enum { AP_NADA, AP_CARREGANDO, AP_PRONTO, AP_ERRO, AP_INDISPONIVEL, AP_CONTA_NUVIO };
typedef struct { ContaPerfil perfil; int estado; char id[96]; } AmigoPerfil;
// Authenticated account profiles. Pairing codes stay internal on older servers;
// no active-profile switch or sharing-preference changes.
int amigosperfis_carregar(void);
int amigosperfis_estado(void);
int amigosperfis_listar(AmigoPerfil *saida,int cap);
int amigosperfis_adicionar(int indice);
#endif
