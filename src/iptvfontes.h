#ifndef NV_IPTVFONTES_H
#define NV_IPTVFONTES_H
#include <stdint.h>
#define IPTV_MAX_CONTAS 32
enum { IPTV_XTREAM = 1, IPTV_M3U, IPTV_STALKER };
typedef struct { unsigned id; int tipo, ativa; char nome[96]; } IptvConta;
// Metadata only. Each backend owns its credentials and playback state.
int iptv_contas(IptvConta *out, int max);
int iptv_conta_selecionada(IptvConta *out);
int iptv_conta_indice(void);
int iptv_conta_selecionar(int indice);
int iptv_conta_mover(int indice,int delta);
int iptv_conta_adicionar(int tipo);
int iptv_conta_renomear(const char *nome);
int iptv_conta_ativar(int ativa);
int iptv_conta_remover(IptvConta *removida);
// Remove per-profile account registries and all native credentials/caches.
void iptv_contas_esquecer_todas(void);
#endif
