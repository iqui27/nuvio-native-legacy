// XTREAM CODES, como mais uma fonte de canais do guia.
//
// O QUE E: o formato de IPTV mais comum do mercado — um servidor, um usuario e
// uma senha. A API e HTTP puro em JSON:
//   <servidor>/player_api.php?username=U&password=P&action=get_live_categories
//   <servidor>/player_api.php?username=U&password=P&action=get_live_streams
// e o canal toca por uma URL previsivel, sem "criar link":
//   <servidor>/live/U/P/<stream_id>.m3u8
// (a mesma coisa que um M3U de Xtream traz linha a linha).
//
// POR QUE EXISTE: gente chegou com "I've tried putting my xtream address but
// nothing happens" — colava o servidor no campo "Portal IPTV" dos Ajustes, que
// e Stalker (portal + MAC), e nada acontecia. Este modulo e o irmao de
// stalker.c: mesma forma de cadastro por perfil, mesma entrega de canais ao
// guia (guia.c), mesma resolucao de URL na hora de tocar (app.c). O que muda
// e que aqui NAO HA token, NAO HA link que expira e NAO HA handshake — a URL
// e estavel e leva a credencial dentro.
//
// A CREDENCIAL E A URL. Usuario e senha viajam no caminho de toda URL de
// canal. Por isso: (1) mora em xtream-p<N>.txt, por perfil, nunca no pacote
// nem no git — ver a lista de exclusao em tools/arm.sh e tizen-art.sh; (2)
// nunca sai em printf — registro.c mostra o stdout NA TELA; (3) a tela de
// Ajustes so mostra o servidor e o usuario, a senha nunca volta em claro; (4)
// a URL do canal so existe na lista de fontes do player, que morre com ele —
// nao entra em favoritos, cache de catalogo nem progresso (o id "xtream:<n>"
// e o que se guarda).
#ifndef NV_XTREAM_H
#define NV_XTREAM_H

#include <time.h>

// ---------------------------------------------------------------- cadastro
void xtream_carregar(void);
int  xtream_configurado(void);          // servidor, usuario e senha presentes
void xtream_definir_servidor(const char *servidor);
void xtream_definir_usuario(const char *usuario);
void xtream_definir_senha(const char *senha);
void xtream_esquecer(void);
// Para a tela de Ajustes. O servidor sem esquema; o usuario inteiro (nao e
// segredo sozinho); a senha como "••••" com o tamanho real. Memoria do modulo,
// valida ate a proxima chamada.
const char *xtream_servidor_curto(void);
const char *xtream_usuario(void);
const char *xtream_senha_mascarada(void);

// ----------------------------------------------------------------- canais
typedef struct {
  char id[80];        // "xtream:<stream_id>"
  char nome[140];
  char logo[480];
  char categoria[64];
  char epgId[64];     // epg_channel_id do servidor, quando ha
} XtreamCanal;

// Baixa categorias + canais ao vivo e preenche `saida`. Devolve quantos; 0 sem
// cadastro ou sem resposta. BLOQUEIA: e do fio do guia.
int xtream_canais(XtreamCanal *saida, int max);
// Por que a ULTIMA xtream_canais devolveu 0 (issue #112): XT_OK (respondeu,
// mesmo vazia, ou sem cadastro), XT_SEM_RESPOSTA, XT_RECUSOU (auth 0). O guia
// diz isso na tela; antes o "0" do Xtream sumia no meio dos canais dos addons
// e so o log sabia.
// XT_PAGINA (#158): respondeu, mas com uma pagina HTML (Cloudflare, WAF,
// portal cativo) em vez da lista — nao e senha errada, e a primeira versao
// dizia que era. XT_HTTP: respondeu 4xx/5xx que nao e de credencial; o codigo
// esta em xtream_ultimo_http (403 bloqueio, 429 rajada, 458 telas, 5xx painel).
enum { XT_OK, XT_SEM_RESPOSTA, XT_RECUSOU, XT_PAGINA, XT_HTTP };
int xtream_ultima_falha(void);
int xtream_ultimo_http(void);
// Quantos canais o servidor mandou na ULTIMA xtream_canais, mesmo os que nao
// couberam em `max` (o guia usa para dizer "Mostrando N de M").
int xtream_ultimo_total(void);

// ----------------------------------------------------------------- conta
// user_info do player_api.php (#158). Tudo opcional: -1/0/"" = nao veio.
typedef struct {
  int  valido;             // houve user_info na resposta
  int  http;               // status do pedido (0 = sem resposta)
  int  auth;
  char status[24];         // "Active", "Expired", "Banned", "Disabled"
  long long expira;        // epoch; 0 = sem vencimento
  int  conexoes, maxConexoes;
  int  teste;              // is_trial
  int  formatosDeclarados; // allowed_output_formats veio
  int  temM3u8, temTs;
} XtreamConta;
// Vai a rede (BLOQUEIA: fio do guia) e guarda a copia. 1 se veio user_info.
int xtream_conta_ler(XtreamConta *c);
// A ultima copia, sem rede. 1 se valida.
int xtream_conta(XtreamConta *c);
// So o parse, para os testes.
int xtream_conta_parse(const char *json, XtreamConta *c);
// O que a tela deve avisar sobre a conta em `agora` (epoch).
enum { XA_NADA, XA_RECUSOU, XA_EXPIRADA, XA_DESATIVADA, XA_TELAS_CHEIAS, XA_VENCE_LOGO };
int xtream_conta_aviso(const XtreamConta *c, long long agora);

// ------------------------------------------------------------ reproducao
int xtream_e_id(const char *id);
// Monta a URL do canal em `url`. Nao vai a rede: e so o cadastro + o id. 0
// quando o id nao e deste modulo ou nao ha cadastro.
int xtream_url(const char *id, char *url, unsigned n);
// Os formatos a tentar, em ordem ("m3u8"/"ts"), pelo que a conta declara e
// pelo que ja tocou nesta sessao. Devolve quantos (1 ou 2).
int xtream_formatos(const char *ext[2]);
// A URL com a extensao pedida.
int xtream_url_formato(const char *id, const char *ext, char *url, unsigned n);
// Avisa que esta URL tocou (loadCompleted): o formato dela vai primeiro nos
// proximos canais da sessao.
void xtream_formato_funcionou(const char *url);

// --------------------------------------------------------- grade curta
// get_short_epg de UM canal (#158): sem o XMLTV inteiro. `titulo` ja
// decodificado do base64. xtream_epg_curto BLOQUEIA (rede); devolve quantos,
// ou -1 em falha (status HTTP em *status, 0 = sem resposta).
typedef struct { time_t ini, fim; char titulo[112]; } XtreamProg;
int xtream_epg_curto(const char *id, XtreamProg *out, int cap, int *status);
// Retry-After (s) da ultima resposta do painel; 0 = nao veio.
int xtream_ultimo_retry_after(void);
int xtream_epg_parse(const char *json, XtreamProg *out, int cap);
// A grade XMLTV do proprio provedor (<servidor>/xmltv.php), que e onde o
// epg_channel_id de cada canal existe (#158). Leva a credencial: nunca em log.
// 0 sem cadastro, ou na Samsung com painel http (bloqueado, ver xtream.c).
int xtream_url_xmltv(char *url, unsigned n);

#endif
