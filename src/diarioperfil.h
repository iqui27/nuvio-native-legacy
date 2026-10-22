// PERFIL E STATS SEM TRAKT: o snapshot da tela (perfil.h) montado do Diario.
// Mesmo recorte do trakt_perfil (o mes corrente), sem rede, sem inventar:
// minutos so do que se sabe (seg tocado ou duracao do player); entrada da conta
// sem duracao conta como reproducao, nunca como minutos chutados.
#ifndef NV_DIARIOPERFIL_H
#define NV_DIARIOPERFIL_H
#include "perfil.h"

// 1 se o Diario do perfil tem pelo menos uma vez vista (o snapshot vale);
// 0 = vazio (a tela fica com o estado de antes). `agora` em epoch.
int diario_perfil_dados(int perfil, PerfilDados *d, long long agora);
#endif
