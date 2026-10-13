// watch_state v2 (AIOStreams): o player EMPURRA eventos de reproducao para os
// addons instalados que declaram o resource "watch_state". Este modulo e
// o contrato, sem rede nem SDL: le o manifesto, filtra, monta corpo e id.
// A fila persistente com retry e o corpo do push ficam aqui tambem. Spec local:
// .mercado/aiostreams-watch-state.md, plano em docs/issues/PLANO-MERCADO.md (W1-W3).
#ifndef NV_WATCHSTATE_H
#define NV_WATCHSTATE_H

#include <stddef.h>

enum { WS_START = 1, WS_PAUSE = 2, WS_STOP = 4, WS_PLAYED = 8, WS_UNPLAYED = 16,
       WS_TODOS = 31 };

#define WS_PREF_MAX 12
#define WS_PREF_TAM 24

// O que o manifesto de UM addon declara. Zerado = nao participa.
typedef struct {
  int  push;                 // 1 = resource watch_state + watchState.version 2 + push{}
  int  eventos;              // mascara WS_* que o addon quer (so reproducao)
  int  viewers;              // watchState.viewers (adiado: so 0 e empurrado, ver watchstate_aceita)
  char tipos[64];            // "|movie|series|" minusculo; vazio = qualquer tipo
  char pref[WS_PREF_MAX][WS_PREF_TAM]; int nPref;   // vazio = qualquer id
} WsDecl;

// Le o manifesto `corpo`. Sempre zera `d` antes; sem resource watch_state, sem
// watchState.push ou com version diferente de 2 deixa push = 0.
void ws_decl_ler(const char *corpo, WsDecl *d);

// O addon quer o evento `ev` (WS_*) de um video do tipo `tipo` ("movie" |
// "series"), de meta `metaId` e video `videoId`? Os idPrefixes casam com
// QUALQUER dos dois ids (W9: meta IMDb com videos Kitsu).
int ws_decl_aceita(const WsDecl *d, int ev, const char *tipo,
                   const char *metaId, const char *videoId);

// ---- Evento, corpo, id, URL ------------------------------------------------

#define WS_ID_MAX 96

// Uma transicao de reproducao ja resolvida pelo player. Sem identidade clara
// (tipo, videoId, e season/episode em serie) nao ha evento: ws_corpo devolve 0.
typedef struct {
  int       ev;                       // WS_START | WS_PAUSE | WS_STOP | WS_PLAYED | WS_UNPLAYED
  char      tipo[12];                 // "movie" | "series"
  char      metaId[WS_ID_MAX], videoId[WS_ID_MAX];
  long long atMs;                     // quando aconteceu (ms Unix); "at" sai em segundos
  long long iniMs;                    // quando a reproducao comecou (ms); so start/pause/stop
  long long posMs;                    // posicao REAL; <0 = desconhecida (so played/unplayed)
  long long durMs;                    // <=0 = desconhecida: a chave nao sai
  int       played;
  int       season, episode;          // -1 = nao se aplica (filme)
  char      imdb[16], tmdb[16];       // ids do titulo; vazio = nao sai
} WsEvento;

// Id estavel entre tentativas: start/pause/stop terminam em posicao e inicio da
// reproducao, played/unplayed no instante. 0 se o evento nao e valido.
int ws_id(const WsEvento *e, char *out, size_t n);
// Corpo JSON do push (campos na ordem da spec). 0 se invalido ou nao coube.
int ws_corpo(const WsEvento *e, char *out, size_t n);
// {base}/watch_state/push/{tipo}/{videoId}.json, id em percent-encoding. base
// e o manifesto sem /manifest.json. 0 se base nao e http(s) ou nao coube.
int ws_url(const char *base, const WsEvento *e, char *out, size_t n);

// ---- Fila com retry --------------------------------------------------------

#define WS_FILA_MAX   64
#define WS_VIDA_S     86400      // evento mais velho que isso e descartado
#define WS_SEGURA_S   21600      // 401/403: o backlog do addon espera 6 h
#define WS_AUSENTE    (-1)       // destino fora da lista de agora: segura, nao descarta
#define WS_AUSENTE_S  5          // ...e reconferencia a lista logo (a lista do pacote
                                // no arranque nao e a lista da conta, e some quando o
                                // sync da conta aplica a de verdade)

typedef struct {
  char *addon, *caminho, *id, *corpo;
  long long criadoS, proxS;      // criacao e proxima tentativa (s Unix)
  int tentativas;                // falhas ate agora
} WsItem;
typedef struct { WsItem it[WS_FILA_MAX]; int n; } WsFila;

// Identidade opaca da configuracao, vinculada a conta e perfil. Nao guarda
// credenciais; o envio resolve esta identidade na lista atual de addons.
void ws_destino(const char *base, const char *usuario, int perfil, char out[20]);

// Segundos ate a tentativa seguinte depois de `falhas` falhas seguidas:
// 30 s, 2 min, 10 min, 1 h, 6 h (e 6 h dai em diante ate o item vencer).
long long ws_espera_s(int falhas);

// Enfileira para a identidade `addon`. 1 = entrou; 0 = duplicado ou invalido.
// Cheia: o mais antigo sai. Guarda apenas identidade e caminho sem query.
int  ws_fila_empurrar(WsFila *f, const char *addon, const char *caminho, const char *id,
                      const char *corpo, long long agoraS);
void ws_fila_limpar(WsFila *f);

// Uma linha por item, separada por TAB; nenhum campo carrega TAB ou quebra.
// Devolve malloc (free) ou NULL. carregar aceita o que serializar escreveu e
// ignora linha malformada. Diarios antigos sem vinculo de conta/perfil sao
// descartados; a versao atual nao persiste URLs nem credenciais.
char *ws_fila_serializar(const WsFila *f);
void  ws_fila_carregar(WsFila *f, const char *texto);

// Tira itens cujo destino ja nao e autorizado pela lista e contexto atuais.
int  ws_fila_podar(WsFila *f, int (*ainda)(const char *addon, void *u), void *u);

// Transporte: devolve o status HTTP, 0 para falha de rede ou prazo, ou
// WS_AUSENTE quando o destino nao esta na lista de agora (o diario segura o
// item para nao perder backlog contra uma lista que nao e a da conta).
// Escreve em *retryAfterS o Retry-After (0 se nao veio). Roda com o que o
// chamador segura; o transporte real solta a trava durante o pedido.
typedef int (*WsEnvio)(const char *addon, const char *caminho, const char *corpo,
                       int *retryAfterS, void *u);

// Descarta o vencido e tenta tudo o que venceu agora. Por addon a ordem e
// FIFO: enquanto o item da frente espera, os de tras do MESMO addon esperam.
// 200/204 entrega; 401/403 segura o addon por WS_SEGURA_S; 429, 5xx e falha de
// rede voltam a tentar (espera >= Retry-After); WS_AUSENTE segura sem contar
// tentativa; outro status descarta o item. Devolve quantas requisicoes fez.
int  ws_fila_passo(WsFila *f, long long agoraS, WsEnvio envia, void *u);
// Quando a proxima cabeca de addon vence (s Unix); -1 se a fila esta vazia.
long long ws_fila_proxima(const WsFila *f);

#endif
