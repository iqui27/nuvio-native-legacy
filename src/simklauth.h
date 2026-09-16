// Vincular o Simkl na propria TV, pelo fluxo de PIN do Simkl.
//
// FLUXO (o mesmo do app web, em js/data/repository/simklAuthService.js), e ele
// NAO e igual ao do Trakt — sao GETs, e nao ha client_secret:
//   GET https://api.simkl.com/oauth/pin?client_id=..&app-name=..&app-version=..
//     -> {"result":"OK","user_code":"ABC123","verification_url":"...",
//         "expires_in":900}
//   GET /oauth/pin/<user_code>?client_id=..   (mesma query)
//     -> {"result":"KO"}                        ainda nao autorizado
//     -> {"result":"OK","access_token":"..."}   pronto
//
// O QUE CONSOME ISTO HOJE: a aba "Listas" da Biblioteca (src/listas.c), que le
// os CINCO ESTADOS de acompanhamento do Simkl (/sync/all-items/<tipo>/<estado>)
// com o token daqui. O Simkl nao tem listas nomeadas na API — nao existe
// equivalente a /users/me/lists do Trakt —, entao "listas do Simkl" quer dizer
// esses cinco estados, e a tela diz isso em vez de fingir outra coisa.
// Vincular aqui continua servindo tambem para a CREDENCIAL CHEGAR NA CONTA, e
// dali para o app web e o celular.
// LIMITE CONHECIDO, e diferente do Trakt: aqui o pedido pendente NAO sobrevive
// a um reinicio do app — o PIN vive so na memoria. No Trakt isso foi corrigido
// porque mordeu de verdade (o dono autorizou e o app tinha reiniciado no meio);
// aqui fica anotado em vez de implementado sem uso, ja que nada neste app
// consome Simkl ainda. Se virar problema, e o mesmo remendo do traktauth.c.
#ifndef NV_SIMKLAUTH_H
#define NV_SIMKLAUTH_H

typedef enum {
  SMK_PARADO = 0,
  SMK_PEDINDO,
  SMK_AGUARDANDO,
  SMK_LIGADO,
  SMK_ERRO
} SmkEstado;

void simklauth_comecar(void);
void simklauth_passo(unsigned agoraMs);

SmkEstado   simklauth_estado(void);
const char *simklauth_codigo(void);
const char *simklauth_url(void);
const char *simklauth_erro(void);

void simklauth_cancelar(void);
int  simklauth_carregar(void);    // le o token guardado; 1 quando havia
// Token de acesso, ou "" quando nao ha vinculo. Existe desde que a Biblioteca
// passou a ler as listas do Simkl (src/listas.c) — ate entao nada neste app
// consumia Simkl e o token so servia para chegar a conta.
const char *simklauth_token(void);
void simklauth_esquecer(void);

#endif
