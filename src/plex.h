// Personal media server: Plex.
//
// WHAT IT DOES. Signs in through the plex.tv PIN flow (the TV shows a 4-letter
// code, the person types it at plex.tv/link on a phone: no password typing with
// the remote), finds the person's Plex Media Server through plex.tv/api/v2/
// resources, picks a connection (local first, then direct remote, relay last),
// shows the movie/show libraries as Home rows, fills the title page from the
// server item, plays the media Part (direct play, only when this TV's codec
// table AND the server's universal decision allow it) or an HLS stream, and
// reports timeline check-ins so the server's resume state stays the authority.
// It also keeps the matching index (srvidx.h: Guid imdb/tmdb/tvdb, title +
// year, episodes on demand) so that a regular catalogue title ("tt...") that
// the server has gets the server's file as a source (plex_consultar, through
// the servidores origin in addons.c).
//
// JSON, NOT XML. Plex answers XML by default and JSON when the request says
// "Accept: application/json"; every request here does, so the parsers are the
// same small JSON walkers jellyfin.c uses (jsraiz.h).
//
// SECRETS. Two tokens exist: the plex.tv account token and the server access
// token (equal for a server the person owns, different for a shared one). Both
// live in plex-p<N>.txt (mode 0600, per device AND profile), never in the Nuvio
// account, telemetry or memory. API calls send them in the X-Plex-Token header.
// KNOWN LIMIT: poster/stream URLs must carry X-Plex-Token in the query because
// the image loader and the LG/Samsung pipelines cannot attach headers; those
// URLs are never printed (rede_url_log redacts the whole query) and only live
// in memory, but a poster URL does carry a usable token while it lives.
//
// CONCURRENCY. Same model as jellyfin.h: one control worker and one check-in
// worker, a RedeJob group whose generation advances on profile switch/logout,
// results published only if still current. No mutex is held during I/O.
#ifndef NV_PLEX_H
#define NV_PLEX_H
#include <stddef.h>
#include "catalogo.h"
#include "jfid.h"
#include "jellyfin.h"
#include "rede.h"
#include "streams.h"

enum {
  PX_OK = 0,
  PX_ERR_REDE = -1,
  PX_ERR_AUTH = -2,        // 401/403: token revoked, sign in again
  PX_ERR_HTTP = -3,
  PX_ERR_FORMATO = -4,     // answered, but not an answer we understand
  PX_ERR_CANCELADO = -5,
  PX_ERR_INDISPONIVEL = -6,// backend without the strict request API (WGT)
  PX_ERR_ENTRADA = -7,
  PX_ERR_EXPIRADO = -8,    // PIN expired/unknown (404 or past expiresAt)
  PX_ERR_SEM_SERVIDOR = -9,// the account has no reachable Plex Media Server
  PX_ERR_OUTRO_SERVIDOR = -10
};

// ------------------------------------------------------------ PIN state machine
// Pure (clock injected) so tests walk it without a network. The control worker
// drives it: create -> poll every PXPIN_INTERVALO_MS -> linked | expired |
// cancelled | too many transport failures.
#define PXPIN_INTERVALO_MS 2000u
#define PXPIN_FALHAS_MAX 4          // consecutive transport failures tolerated
#define PXPIN_TETO_PADRAO_S 900     // plex.tv "expiresIn" when it says nothing
typedef enum {
  PXP_OCIOSO = 0,
  PXP_ESPERANDO,   // code on screen, polling
  PXP_LIGADO,      // token received
  PXP_EXPIRADO,    // code unknown/expired: ask for a new one
  PXP_CANCELADO,
  PXP_ERRO         // too many consecutive transport failures
} PxPinEstado;
typedef struct {
  PxPinEstado est;
  char id[24];            // numeric pin id (plex.tv returns a number)
  char codigo[8];         // the 4 characters shown to the person
  unsigned long long expiraMs, proximoMs;
  int falhas;
  char token[96];         // set when LIGADO; never logged
} PxPin;
void pxpin_iniciar(PxPin *p);
// Called with the answer of POST /pins. expiraSeg <= 0 means "not stated".
// Returns 0 (and stays OCIOSO) if id/codigo are empty.
int  pxpin_criado(PxPin *p, unsigned long long agora, const char *id, const char *codigo,
                  int expiraSeg);
// 1 when the worker should poll now (WAITING and the interval has passed).
int  pxpin_vencido(const PxPin *p, unsigned long long agora);
// Feed one poll result: 1 linked (token set), 0 pending, PX_ERR_EXPIRADO the
// server forgot the pin, any other negative a transport/server failure.
void pxpin_resultado(PxPin *p, unsigned long long agora, int resp, const char *token);
void pxpin_cancelar(PxPin *p);
// Clock tick without a poll: WAITING past its deadline becomes EXPIRADO.
void pxpin_relogio(PxPin *p, unsigned long long agora);

// ------------------------------------------------------------------ data
typedef struct {
  char uri[300];           // what plex.tv advertises (usually https://<ip>.<hash>.plex.direct:port)
  char addr[64];           // bare address, used to try plain http on the LAN
  int  porta;
  int  local, relay, https;
} PxConexao;
#define PX_CON_MAX 8
typedef struct {
  char nome[96];
  char id[48];             // clientIdentifier == machineIdentifier (hex)
  char versao[24];
  char token[96];          // accessToken for THIS server
  int dono;                // owned by the signed-in account
  int nCon;
  PxConexao con[PX_CON_MAX];
} PxServidor;
#define PX_SERVIDORES_MAX 8

typedef struct {
  char base[300];          // chosen connection, scheme://host:port, no trailing '/'
  char servidorId[48];     // machineIdentifier
  char servidorNome[96];
  char versao[24];
  char usuario[96];        // plex.tv display name (never the e-mail)
  char tokenConta[96];     // plex.tv account token — never logged
  char token[96];          // server access token — never logged
  char dispositivoId[80];  // X-Plex-Client-Identifier, per device + profile
  char dispositivoNome[48];
} PxConta;

typedef struct {
  char id[24];             // section key
  char nome[96];
  char tipo[16];           // "movie" | "series"
} PxBiblioteca;

#define PX_FONTES_MAX 8
typedef struct {
  char ratingKey[24];
  int  midia;              // index of the Media entry
  int  metodo;             // PX_METODO_*
  char sessaoId[48];       // X-Plex-Session-Identifier
  long long duracaoMs;
  char url[2048];          // absolute, carries the token: never log
} PxSessaoPlay;
// DIRETO: the Part as is. STREAM: HLS where the server copies the video and
// converts only audio/subtitles. TRANSCODE: HLS, video converted too.
enum { PX_METODO_DIRETO = 0, PX_METODO_TRANSCODE, PX_METODO_STREAM };
// What decides direct play for one Media version (px_ler_midias): container,
// video codec, the SELECTED audio track and the selected subtitle, if any.
typedef struct {
  char cont[16], vcod[16], acod[16], leg[24];
} PxMidiaInfo;
enum { PX_DEC_DESCONHECIDO = -1, PX_DEC_NAO = 0, PX_DEC_DIRETO = 1 };
typedef struct {
  int n;
  PxSessaoPlay sessao[PX_FONTES_MAX];
  Stream fonte[PX_FONTES_MAX];
} PxPlayback;

// ----------------------------------------------- pure parsers / helpers
// All work on a response body in memory, never touch the network.
int px_ler_pin(const char *corpo, size_t n, char *id, size_t ni, char *codigo, size_t nc,
               int *expiraSeg, char *token, size_t nt);
int px_ler_recursos(const char *corpo, size_t n, PxServidor *out, int max);
// Candidate base URLs in the order they should be probed: local raw http,
// local uri, direct remote, relay last. Returns how many were written.
int px_candidatas(const PxServidor *s, char out[][320], int max);
int px_ler_identidade(const char *corpo, size_t n, char *id, size_t ni, char *versao, size_t nv);
int px_ler_usuario(const char *corpo, size_t n, char *nome, size_t tam);
int px_ler_bibliotecas(const char *corpo, size_t n, PxBiblioteca *out, int max);
int px_ler_itens(const PxConta *c, const char *corpo, size_t n, CatItem *out, int max, int *total);
// Matching index rows (srvidx.h) of a listing: ratingKey, Guid[] (imdb, tmdb,
// tvdb), title hash, year, updatedAt, viewCount, the first Media's height.
// `serieRk` for an allLeaves answer (episode rows), else NULL. *total =
// totalSize. Returns items seen on this page.
int px_ler_indice(const char *corpo, size_t n, const char *serieRk, SrvIdx *x, int *total);
// ratingKey of the episode with this season/episode in an allLeaves answer:
// 1 found, 0 not there, PX_ERR_FORMATO on a bad body.
int px_ler_episodio_rk(const char *corpo, size_t n, int temp, int ep, char *rk, size_t tam);
int px_ler_detalhe(const PxConta *c, const char *corpo, size_t n, CatItem *out);
int px_ler_episodios(const PxConta *c, const char *corpo, size_t n, CatEp *out, int max);
// `direto[i]` is the server's decision for version i (PX_DEC_*), or NULL when
// none was asked. Direct play is listed only when px_direto_suportado says
// this TV opens the version AND the decision is not PX_DEC_NAO; otherwise the
// version gets the HLS entry (direct stream or transcode).
int px_ler_fontes(const PxConta *c, const char *rk, const char *corpo, size_t n,
                  const signed char *direto, PxPlayback *out);
int px_ler_midias(const char *corpo, size_t n, PxMidiaInfo *out, int max);
int px_direto_suportado(JfBackend b, const PxMidiaInfo *m);
int px_video_suportado(JfBackend b, const char *vcod);
int px_perfil_extra(JfBackend b, char *dst, size_t tam);
int px_ler_decisao(const char *corpo, size_t n);           // PX_DEC_*
// Item id the person's title carries -> external ids. Accepts "tt123",
// "tt123:2:5", "tmdb:278", "tmdb:278:1:3". 0 when not an external id.
int px_id_titulo(const char *id, char imdb[16], long *tmdb, int *temp, int *ep);
long long px_ms(double seg);
const char *px_metodo_nome(int metodo);

// ------------------------------------------------- blocking protocol layer
int px_pin_criar(const PxConta *c, RedeJob *job, char *id, size_t ni, char *codigo, size_t nc,
                 int *expiraSeg);
// 1 linked (token written), 0 pending, <0 PX_ERR_*.
int px_pin_conferir(const PxConta *c, RedeJob *job, const char *id, char *token, size_t nt);
int px_recursos(const PxConta *c, RedeJob *job, PxServidor *out, int max);
int px_escolher(PxConta *c, RedeJob *job, const PxServidor *s);   // probes candidates
int px_bibliotecas(const PxConta *c, RedeJob *job, PxBiblioteca *out, int max);
int px_itens(const PxConta *c, RedeJob *job, const char *secao, int inicio, int limite,
             CatItem *out, int max, int *total);
int px_indice(const PxConta *c, RedeJob *job, const char *secao, int inicio, int limite,
              long long desde, SrvIdx *x, int *total);
int px_indice_episodios(const PxConta *c, RedeJob *job, const char *serieRk, SrvIdx *x);
int px_achar_episodio(const PxConta *c, RedeJob *job, const char *serieRk, int temp, int ep,
                      char *rk, size_t tam);
int px_detalhe(const PxConta *c, RedeJob *job, const char *rk, CatItem *out);
int px_episodios(const PxConta *c, RedeJob *job, const char *serieRk, CatEp *out, int max);
int px_decisao(const PxConta *c, RedeJob *job, const char *rk, int midia, JfBackend b);
// Item metadata, then one decision per version this TV could open as is.
int px_fontes(const PxConta *c, RedeJob *job, const char *rk, PxPlayback *out);
enum { PX_REL_INICIO = 0, PX_REL_PROGRESSO, PX_REL_PAUSA, PX_REL_RETOMA, PX_REL_FIM };
int px_reportar(const PxConta *c, RedeJob *job, int evento, const PxSessaoPlay *s, double posSeg);

// ------------------------------------------------------- app integration
int  plex_disponivel(void);
void plex_carregar(void);
void plex_perfil_trocou(void);
void plex_esquecer(void);          // sign out this profile (forgets the tokens)
void plex_esquecer_todos(void);    // Nuvio account logout: every profile file
void plex_encerrar(void);

typedef enum {
  PX_EST_SEM_CONTA = 0,
  PX_EST_CODIGO,         // showing the PIN code, polling plex.tv
  PX_EST_ENTRANDO,       // linked; finding the server
  PX_EST_CONECTADO,
  PX_EST_EXPIROU,        // 401: sign in again
  PX_EST_ERRO            // last action failed (plex_ultimo_erro)
} PxEstado;
// `detalhe` is display data: the PIN code while PX_EST_CODIGO, the server name
// when connected. Never a secret.
PxEstado plex_estado(char *detalhe, size_t tam);
const char *plex_servidor_nome(void);
const char *plex_usuario(void);
int  plex_conectado(void);
int  plex_ultimo_erro(void);
int  plex_n_servidores(void);

int  plex_entrar(void);               // starts the PIN flow (queued)
void plex_cancelar_entrada(void);
int  plex_proximo_servidor(void);     // cycle to the next server of the account

#define PX_FIL_MAX 6
#define PX_POR_FILEIRA 24
unsigned plex_fileiras_versao(void);
int  plex_fileiras_copiar(CatItem *itens, int maxItens, CatFileira *fils, int maxFils, int *nItens);
int  plex_chave_fileira(const char *chave);   // "plex_<section>"
void plex_recarregar_bibliotecas(void);

// Title page of a Plex-native item ("px:..."). BLOCKS. -1 failed.
int  plex_ficha(CatItem *item, CatEp *eps, int maxEps);

// Sources for a Plex-native target. Same contract as jellyfin_fontes_*.
enum { PX_FONTES_NADA = 0, PX_FONTES_PENDENTE, PX_FONTES_PRONTO, PX_FONTES_FALHOU };
int  plex_fontes_pedir(const char *alvo);
int  plex_fontes_colher(const char *alvo, Stream **lista, int *n);

// Sources for a REGULAR catalogue title the server has (matched by imdb/tmdb).
// BLOCKS (runs in addons.c's extra-source thread). Returns the number of
// sources, 0 when the server does not have it or the index is not ready, -1
// when cancelled. `cancelado` is polled; list is malloc'd for the caller.
// `titulo`/`ano` feed the last match rule (title + year), may be NULL/0.
int  plex_consultar(const char *id, const char *tipo, const char *titulo, int ano,
                    int (*cancelado)(void *), void *ctx, Stream **saida);
int  plex_casamento_ativo(void);       // connected and the index has entries
// Offline: the index has this title; fills `info` (name, quality, LAN) when not NULL.
int  plex_tem(const char *id, const char *tipo, SrvAchado *info);
int  plex_indice_n(void);              // entries in the index (tests)

#define PX_PROGRESSO_MS 10000u
void plex_reproducao_tick(const char *url, double posSeg, double durSeg, int tocando);
void plex_reproducao_fim(const char *url, double posSeg, double durSeg);
int  plex_relatorios_pendentes(void);

#endif
