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
enum { XT_OK, XT_SEM_RESPOSTA, XT_RECUSOU };
int xtream_ultima_falha(void);

// ------------------------------------------------------------ reproducao
int xtream_e_id(const char *id);
// Monta a URL do canal em `url`. Nao vai a rede: e so o cadastro + o id. 0
// quando o id nao e deste modulo ou nao ha cadastro.
int xtream_url(const char *id, char *url, unsigned n);

#endif
