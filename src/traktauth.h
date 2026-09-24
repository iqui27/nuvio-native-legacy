// Vincular o Trakt na propria TV, pelo fluxo de DEVICE CODE do Trakt.
//
// POR QUE ISTO EXISTE. MEDIDO na conta do dono: a RPC de credenciais do Nuvio
// (`sync_pull_provider_credentials`) devolve tmdb, mdblist, animeskip, introdb
// e os debrid — e NENHUM `trakt`. Como `art/trakt.txt` deixou de ir no pacote
// (era credencial de quem montou), quem instalasse ficaria sem Trakt, e e dele
// que sai o "Continuar assistindo". Esperar o app web escrever a linha na conta
// resolveria so para quem ja usa o app web.
//
// O fluxo e o proprio do Trakt, o mesmo que o app web usa:
//   POST https://api.trakt.tv/oauth/device/code  {client_id}
//     -> {device_code, user_code, verification_url, expires_in, interval}
//   POST /oauth/device/token  {code, client_id, client_secret}
//     -> 200 com o token; 400 ainda nao autorizado; 409 ja usado;
//        410 expirou; 418 negado; 429 devagar.
//
// DIFERENCA IMPORTANTE PARA A TELA DE LOGIN DA CONTA: o `user_code` do Trakt e
// CURTO (8 caracteres) e o endereco e fixo. Isso da para ler da TV e digitar no
// celular — nao precisa de QR, ao contrario dos 32 digitos hexadecimais do
// login da conta.
#ifndef NV_TRAKTAUTH_H
#define NV_TRAKTAUTH_H

typedef enum {
  TRA_PARADO = 0,
  TRA_PEDINDO,     // buscando o codigo
  TRA_AGUARDANDO,  // codigo na tela, esperando a pessoa autorizar
  TRA_LIGADO,      // token obtido
  TRA_ERRO,
  TRA_INVALIDO     // token recusado (401) e refresh impossivel ou negado
} TraEstado;

// Comeca o fluxo num fio proprio. Idempotente enquanto um estiver em andamento.
void traktauth_comecar(void);

// Um passo. Chamar uma vez por quadro; nao bloqueia. E aqui que o poll e
// reagendado — o intervalo vem do proprio Trakt, e sobe quando ele manda 429.
void traktauth_passo(unsigned agoraMs);

TraEstado   traktauth_estado(void);
const char *traktauth_codigo(void);    // user_code, para exibir
const char *traktauth_url(void);       // verification_url
const char *traktauth_erro(void);

void traktauth_cancelar(void);

// O VINCULO E POR PERFIL (trakt-p<N>.txt). O trakt.txt antigo, de antes dos
// perfis, e do perfil 1 e so dele: vira trakt-p1.txt na primeira leitura e
// nunca e copiado para outro perfil. Ver a nota no topo de traktauth.c.
//
// Carrega o token guardado para o perfil em vigor (1 ate alguem dizer outro) e
// o aplica em trakt.c. 1 quando havia token.
int  traktauth_carregar(void);
// O mesmo, escolhendo antes o perfil. Chamar no arranque, depois de
// dados_iniciar e de perfis_carregar_ativo, com perfis_ativo().
int  traktauth_carregar_perfil(int perfil);
// TROCA DE PERFIL: esquece a credencial em uso (trakt_esquecer — inclusive a
// que veio da conta pelo sync) e carrega a do perfil novo, ou nenhuma. 0 quando
// o perfil e o mesmo. Devolve 1 quando o Trakt estava ou ficou ligado, ou seja,
// quando as fileiras do Trakt na tela sao do perfil errado e a home tem de ser
// remontada. Um fio de poll ou de renovacao que ainda estiver no ar termina
// gravando no arquivo do perfil que o pediu, nunca no novo.
int  traktauth_trocar_perfil(int perfil);
int  traktauth_perfil(void);

// Esquece o vinculo de TODOS os perfis (chamado ao sair da conta).
void traktauth_esquecer(void);

#endif
