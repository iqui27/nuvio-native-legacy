// Personal media server: Jellyfin (F11, first slice of 1.8) and Emby.
//
// EMBY SHARES THIS MODULE. Emby is the project Jellyfin forked from, so the
// protocol layer is one code path with a server kind (JfConta.tipo): the
// differences are the X-Emby-* auth headers, the /emby API root, per-user item
// routes, no Quick Connect, numeric item ids and "mediasource_<n>" source ids.
// The app-integration layer (state, workers, Home rows, sources, check-ins)
// is an instance (JfInst) so a profile can have a Jellyfin AND an Emby server
// at once; the jellyfin_* functions drive instance 0 and the emby_* ones
// instance 1. Ids differ by namespace ("jf:" / "em:", jfid.h).
//
// WHAT IT DOES. Connects one Jellyfin server per Nuvio profile on this device
// (Quick Connect when the server allows it, else username/password), shows the
// movie/series libraries as Home rows, fills the existing title page from the
// server item, negotiates playback with POST /Items/{id}/PlaybackInfo and a
// device profile per backend, and reports start/progress/stop so the server's
// resume state stays the authority for these items. Plex is NOT here:
// it has its own contract (plex.h; docs/plans/media-servers-1.8/README.md).
//
// SECRETS. The password only lives in the login request and is wiped right
// after. The access token is stored per device AND profile in
// jellyfin-p<N>.txt (mode 0600, POSIX), never synced to the Nuvio account,
// telemetry or memory. That file is local storage, not a vault: on LG, Samsung
// and Android without a keystore bridge it is as safe as the app's data dir.
// The token goes in the Authorization header of API calls and, only because
// the LG/Samsung pipelines cannot attach arbitrary headers, in the query of
// player URLs (ApiKey= on Jellyfin, api_key= on Emby: jf_anexar_apikey). Logs never print the token, the base path, the media path or
// full titles: [jellyfin] lines carry operation, status and latency only.
//
// CONCURRENCY. Every request runs on a worker with a RedeJob from a group
// whose generation advances on profile switch, logout and server change. A
// result is published only if its job is still current AND the generation it
// captured is the current one: late answers are dropped, never applied to the
// wrong profile. No mutex is held during network waits.
//
// PLATFORMS. Needs the strict request API (rede_pedir). WGT's synchronous XHR
// cannot honour deadlines/cancellation, so jellyfin_disponivel() is 0 there and
// the UI says "unavailable on this platform".
#ifndef NV_JELLYFIN_H
#define NV_JELLYFIN_H
#include <stddef.h>
#include "catalogo.h"
#include "jfid.h"
#include "rede.h"
#include "streams.h"

// ---------------------------------------------------------------- results
enum {
  JF_OK = 0,
  JF_ERR_REDE = -1,        // transport/deadline: keep the token, try later
  JF_ERR_AUTH = -2,        // 401: token revoked/expired, sign in again
  JF_ERR_HTTP = -3,        // other 4xx/5xx
  JF_ERR_FORMATO = -4,     // answered, but not a Jellyfin answer we understand
  JF_ERR_CANCELADO = -5,   // job cancelled or generation advanced
  JF_ERR_INDISPONIVEL = -6,// backend without the strict request API (WGT)
  JF_ERR_ENTRADA = -7,     // bad URL/argument, refused before any I/O
  JF_ERR_EXPIRADO = -8,    // Quick Connect code unknown/expired (404)
  JF_ERR_QC_DESLIGADO = -9,// Quick Connect disabled on the server
  JF_ERR_OUTRO_SERVIDOR = -10 // item id belongs to another server
};

// Backend used to build the device profile. Declared conservatively from what
// each pipeline is known to open; none of these was proven on a TV here.
typedef enum {
  JF_BACKEND_HOST = 0,  // Mac/desktop build: H.264/AAC MP4 only
  JF_BACKEND_ANDROID,   // Media3/ExoPlayer
  JF_BACKEND_WEBOS,     // LG uMediaServer
  JF_BACKEND_TPK,       // Samsung native .tpk
  JF_BACKEND_WGT        // Samsung web: unavailable (no strict HTTP)
} JfBackend;
JfBackend jellyfin_backend(void);
const char *jellyfin_backend_nome(JfBackend b);

enum { JF_TIPO_JELLYFIN = 0, JF_TIPO_EMBY = 1 };

// One connection: everything needed to talk to the server as one user.
typedef struct {
  int  tipo;               // JF_TIPO_*
  char base[512];          // scheme://host[:port][/prefix], no trailing '/'
  char servidorId[40];     // Id of /System/Info/Public, 32 hex
  char servidorNome[96];
  char versao[24];
  char usuarioId[40];
  char usuarioNome[96];
  char token[80];          // never logged
  char dispositivoId[80];  // per device + profile (Jellyfin binds tokens to it)
  char dispositivoNome[48];
} JfConta;

typedef struct {
  char id[40];
  char nome[96];
  char tipo[16];           // "movie" | "series" (from CollectionType)
} JfBiblioteca;

#define JF_FONTES_MAX 8
typedef struct {
  char itemId[40];
  char fonteId[64];        // MediaSourceId (Emby: "mediasource_<n>")
  char sessaoId[64];       // PlaySessionId
  int  metodo;             // JF_METODO_*
  char url[2048];          // absolute, may carry api_key: never log
} JfSessaoPlay;
enum { JF_METODO_DIRETO = 0, JF_METODO_STREAM, JF_METODO_TRANSCODE };
const char *jellyfin_metodo_nome(int metodo);   // DirectPlay/DirectStream/Transcode

typedef struct {
  int n;
  JfSessaoPlay sessao[JF_FONTES_MAX];
  Stream fonte[JF_FONTES_MAX];
} JfPlayback;

// ------------------------------------------------- blocking protocol layer
// Pure request/parse functions over an explicit JfConta, used by the worker
// and by tests against tests/jellyfin_server.py. All BLOCK; `job` may be NULL
// in tests. None of them logs secrets.
int jf_url_normalizar(const char *entrada, char *dst, size_t tam);
// Same, for a server kind: Emby drops a trailing "/emby" (re-added per request).
int jf_url_normalizar_tipo(int tipo, const char *entrada, char *dst, size_t tam);
const char *jf_prefixo(const JfConta *c);   // "" Jellyfin, "/emby" Emby
const char *jf_tipo_nome(int tipo);         // "Jellyfin" / "Emby"
void jf_cabecalho_auth(const JfConta *c, char *dst, size_t tam);
int jf_publico(const char *base, RedeJob *job, JfConta *c);
int jf_qc_habilitado(const JfConta *c, RedeJob *job);
int jf_qc_iniciar(const JfConta *c, RedeJob *job, char *segredo, size_t ns,
                  char *codigo, size_t nc);
// 1 approved, 0 pending, <0 JF_ERR_* (EXPIRADO when the server forgot it).
int jf_qc_conferir(const JfConta *c, RedeJob *job, const char *segredo);
int jf_qc_autenticar(JfConta *c, RedeJob *job, const char *segredo);
// Wipes `senha` (and the request body copy) before returning, success or not.
int jf_autenticar_senha(JfConta *c, RedeJob *job, const char *usuario, char *senha);
int jf_bibliotecas(const JfConta *c, RedeJob *job, JfBiblioteca *out, int max);
// Items of one library, newest first. *total = TotalRecordCount (paging).
int jf_itens(const JfConta *c, RedeJob *job, const char *bibliotecaId,
             int inicio, int limite, CatItem *out, int max, int *total);
int jf_detalhe(const JfConta *c, RedeJob *job, const char *itemId, CatItem *out);
int jf_episodios(const JfConta *c, RedeJob *job, const char *serieId,
                 CatEp *out, int max);
// Pure parsers over a response body already in memory (the request functions
// above call them; tests feed fixtures). They never touch the network.
int jf_ler_autenticacao(JfConta *c, char *corpo, size_t n);   // wipes corpo
int jf_ler_bibliotecas(const char *corpo, size_t n, JfBiblioteca *out, int max);
int jf_ler_itens(const JfConta *c, const char *corpo, size_t n, CatItem *out, int max, int *total);
int jf_ler_detalhe(const JfConta *c, const char *corpo, size_t n, CatItem *out);
int jf_ler_episodios(const JfConta *c, const char *corpo, size_t n, CatEp *out, int max);
int jf_ler_playbackinfo(const JfConta *c, const char *itemLimpo, const char *corpo, size_t n,
                        JfPlayback *out);
int jf_perfil_dispositivo(JfBackend b, char *json, size_t tam);
// Token in player URLs: "ApiKey=" for Jellyfin 10.8+ and 12.x (12.0 refuses
// the legacy "api_key=" in the query), "api_key=" for Emby and Jellyfin <10.8.
// jf_anexar_apikey also renames a token the server already put in the URL.
int  jf_apikey_moderna(const JfConta *c);
void jf_anexar_apikey(const JfConta *c, char *url, size_t tam);
int jf_playbackinfo(const JfConta *c, RedeJob *job, const char *itemId,
                    JfBackend b, JfPlayback *out);
// evento: JF_REL_*; positions in seconds, converted to int64 ticks inside.
enum { JF_REL_INICIO = 0, JF_REL_PROGRESSO, JF_REL_PAUSA, JF_REL_RETOMA, JF_REL_FIM };
int jf_reportar(const JfConta *c, RedeJob *job, int evento,
                const JfSessaoPlay *s, double posSeg);
int jf_sair(const JfConta *c, RedeJob *job);
long long jf_ticks(double seg);
double jf_seg(long long ticks);

// ------------------------------------------------------- app integration
int  jellyfin_disponivel(void);     // strict HTTP present on this backend
void jellyfin_carregar(void);       // reads the active profile's connection
void jellyfin_perfil_trocou(void);  // cancels in-flight work, reloads
void jellyfin_esquecer(void);       // sign out this profile (best-effort server logout)
void jellyfin_esquecer_todos(void); // Nuvio account logout: every profile file
void jellyfin_encerrar(void);       // joins workers (tests, app exit)

typedef enum {
  JF_EST_SEM_SERVIDOR = 0,
  JF_EST_VERIFICANDO,    // checking /System/Info/Public
  JF_EST_SERVIDOR_OK,    // server known, not signed in
  JF_EST_QC_CODIGO,      // showing the Quick Connect code, polling
  JF_EST_ENTRANDO,       // password/quick connect exchange in flight
  JF_EST_CONECTADO,
  JF_EST_EXPIROU,        // server said 401: sign in again
  JF_EST_ERRO            // last action failed; detail says why
} JfEstado;
// Snapshot for the Settings rows. `detalhe` is display data (server name and
// version, or the Quick Connect code), never a secret; failures are codes
// (jellyfin_ultimo_erro) so the UI can translate them.
JfEstado jellyfin_estado(char *detalhe, size_t tam);
const char *jellyfin_servidor_curto(void);  // host[:port] for display, "" none
const char *jellyfin_usuario(void);
int  jellyfin_conectado(void);
int  jellyfin_qc_permitido(void);           // known after the server check
int  jellyfin_ultimo_erro(void);            // JF_ERR_* behind JF_EST_ERRO

// Non-blocking actions (queued on the control worker). 0 if refused.
int  jellyfin_definir_servidor(const char *url);
int  jellyfin_entrar_quick_connect(void);
int  jellyfin_entrar_senha(const char *usuario, char *senha);  // wipes senha
void jellyfin_cancelar_entrada(void);
void jellyfin_recarregar_bibliotecas(void);

// Home rows. Copies the current snapshot without network; rows have empty
// `base` (fixed rows: they do not consume the addon row limit).
#define JF_FIL_MAX 6
#define JF_POR_FILEIRA 24
unsigned jellyfin_fileiras_versao(void);
int  jellyfin_fileiras_copiar(CatItem *itens, int maxItens, CatFileira *fils,
                              int maxFils, int *nItens);
int  jellyfin_chave_fileira(const char *chave);   // "jellyfin_<lib>"

// Title page (BLOCKS; descoberta's episode thread). Fills cast/overview/
// seasons and the episode list. -1 cancelled/other server/failed.
int  jellyfin_ficha(CatItem *item, CatEp *eps, int maxEps);

// Sources for one target id (movie or episode jf id). pedir returns at once;
// colher is polled on the UI thread: JF_FONTES_* and, when ready, a malloc'd
// list the caller frees (may be empty).
enum { JF_FONTES_NADA = 0, JF_FONTES_PENDENTE, JF_FONTES_PRONTO, JF_FONTES_FALHOU };
int  jellyfin_fontes_pedir(const char *alvo);
int  jellyfin_fontes_colher(const char *alvo, Stream **lista, int *n);

// Playback check-ins, called by the player on the UI thread with the URL it is
// playing. Unknown URLs are ignored (addon sources never match). Progress is
// sent every 10 s while playing and immediately on pause/resume; stop ends the
// session and asks the server to release a transcode.
#define JF_PROGRESSO_MS 10000u
void jellyfin_reproducao_tick(const char *url, double posSeg, double durSeg, int tocando);
void jellyfin_reproducao_fim(const char *url, double posSeg, double durSeg);
// For tests: queued check-ins not yet sent.
int  jellyfin_relatorios_pendentes(void);

// ------------------------------------------------------------- Emby twin
// The same integration API for the Emby instance (see the note at the top).
// Differences: no emby_entrar_quick_connect (it always refuses), Home row keys
// start with "emby_", ids with "em:", and the token file is emby-p<N>.txt.
#define JF_EMBY_DECL(P)                                                                  \
  void P##_carregar(void);                                                               \
  void P##_perfil_trocou(void);                                                          \
  void P##_esquecer(void);                                                               \
  void P##_esquecer_todos(void);                                                         \
  void P##_encerrar(void);                                                               \
  JfEstado P##_estado(char *detalhe, size_t tam);                                        \
  const char *P##_servidor_curto(void);                                                  \
  const char *P##_usuario(void);                                                         \
  int P##_conectado(void);                                                               \
  int P##_qc_permitido(void);                                                            \
  int P##_ultimo_erro(void);                                                             \
  int P##_definir_servidor(const char *url);                                             \
  int P##_entrar_quick_connect(void);                                                    \
  int P##_entrar_senha(const char *usuario, char *senha);                                \
  void P##_cancelar_entrada(void);                                                       \
  void P##_recarregar_bibliotecas(void);                                                 \
  unsigned P##_fileiras_versao(void);                                                    \
  int P##_chave_fileira(const char *chave);                                              \
  int P##_fileiras_copiar(CatItem *itens, int maxItens, CatFileira *fils, int maxFils,   \
                          int *nItens);                                                  \
  int P##_ficha(CatItem *item, CatEp *eps, int maxEps);                                  \
  int P##_fontes_pedir(const char *alvo);                                                \
  int P##_fontes_colher(const char *alvo, Stream **lista, int *n);                       \
  void P##_reproducao_tick(const char *url, double posSeg, double durSeg, int tocando);  \
  void P##_reproducao_fim(const char *url, double posSeg, double durSeg);                \
  int P##_relatorios_pendentes(void);
JF_EMBY_DECL(emby)

#endif
