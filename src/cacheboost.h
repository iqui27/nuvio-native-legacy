// F07 (1.8): SEEK CACHE ON DISK AND VOLUME ABOVE 100%, the state the app reads.
//
// Both features exist only where the backend can really do them. Today that is
// the Android player (NvPlayer.kt): Media3 SimpleCache + CacheDataSource over
// the existing data source chain, and a gain AudioProcessor in the audio sink.
// LG (uMS) and Samsung (AVPlay/.tpk) have no app-controlled file cache and no
// PCM path the app can scale: the UI says "Não disponível nesta TV" there, and
// docs/plans/player-1.8/CACHE-BOOST-CAPACIDADE.md records why.
//
// This module is pure state (no SDL, no JNI): the backend reports what it did
// (cacheboost_cache_relato / cacheboost_ganho_relato, any thread) and the UI
// reads snapshots. Tests: tests/cacheboost.c.
#ifndef NV_CACHEBOOST_H
#define NV_CACHEBOOST_H
#include <stddef.h>

// 1 = this build's backend implements cache and gain (Android only).
int cacheboost_suportado(void);

// --- seek cache --------------------------------------------------------------
// Settings option index (Ajustes "Cache de seek em disco") -> MB requested.
#define CB_CACHE_OPCOES 4
int cacheboost_cache_mb(int opcao);   // 0 (off), 256, 512, 1024; out of range -> 0

// What the backend did with the request in the CURRENT session.
enum {
  CB_CACHE_DESLIGADO = 0,     // setting off (or no session yet)
  CB_CACHE_ATIVO,             // writing to disk, limite = effective limit
  CB_CACHE_POUCO_ESPACO,      // statvfs before opening: did not fit -> off
  CB_CACHE_DISCO_CHEIO,       // ENOSPC mid-session -> off, playback continues
  CB_CACHE_NAO_SE_APLICA,     // live / HLS / DASH / non-HTTP: never cached
  CB_CACHE_FALHOU,            // cache init or another write error -> off
  CB_CACHE_N
};
typedef struct { int estado, pedidoMb, limiteMb, usadoMb; } CbCache;

// Backend report (any thread). Out-of-range values are clamped/ignored.
void    cacheboost_cache_relato(int estado, int pedidoMb, int limiteMb, int usadoMb);
CbCache cacheboost_cache(void);
// The player's notice for this session, ONCE: the first ENOSPC / "no space"
// report returns its sentence (Portuguese key, the caller runs i18n), then
// NULL until the next session.
const char *cacheboost_cache_aviso(void);
// "120 / 512 MB", "Desligado", "Pouco espaço"... (Portuguese keys / numbers;
// numbers need no translation). For diagnostics.
void cacheboost_cache_texto(char *b, size_t n);

// --- volume ------------------------------------------------------------------
#define CB_VOL_MIN    0
#define CB_VOL_NORMAL 100
#define CB_VOL_MAX    200
#define CB_VOL_PASSO  10
enum { CB_GANHO_DESCONHECIDO = 0, CB_GANHO_PCM, CB_GANHO_PASSTHROUGH };

// A NEW PLAYBACK SESSION (title opened): volume back to 100%, cache report
// and one-shot notice forgotten. A source change inside the same title keeps
// the volume (the backend re-applies it after the reopen).
void cacheboost_sessao(void);
int  cacheboost_volume(void);
// 200, or 100 while the audio goes out as passthrough/bitstream (the gain
// processor never sees those samples; passthrough is never switched off to
// make room for the boost).
int  cacheboost_volume_teto(void);
// LEFT/RIGHT on the volume row: one step, clamped to [0, teto]. Returns the
// new value (the caller sends it to the backend when it changed).
int  cacheboost_volume_passo(int dir);
int  cacheboost_volume_acima(void);           // > 100: the value is tinted red
// Backend report of the current audio output (any thread). PASSTHROUGH clamps
// the session volume to 100.
void cacheboost_ganho_relato(int estado);
int  cacheboost_ganho_estado(void);
// 20*log10(pct/100): 200% = +6.02 dB. pct <= 0 -> -120 (silence floor).
double cacheboost_volume_db(int pct);

// --- backend hooks -------------------------------------------------------------
// Implemented by video_android.c; the other targets get the no-ops in
// cacheboost.c. `pct` 0..200; `mb` = cache limit for the NEXT open (0 = off).
void cacheboost_backend_ganho(int pct);
void cacheboost_backend_cache(int mb);
// Warm resume (#492): the title of the NEXT open (arms with the cache; empty =
// none), and the order to drop the cache kept from the last title (profile
// switch). Android only: webOS/Tizen have no disk cache, so these are no-ops.
void cacheboost_backend_titulo(const char *id);
void cacheboost_backend_descartar(void);
// #338: buffer size in MiB for the NEXT open (0 = Auto, the backend default).
void cacheboost_backend_buffer(int mb);
// Settings "Tamanho do buffer" index (0 Auto, 1 = 64, 2 = 128, 3 = 256 MiB)
// -> MiB for the next open. Steps down the 256/128/64 ladder until free RAM
// (`livreMb`, MemAvailable; < 0 = unknown) keeps CB_BUFFER_PISO_MB after the
// buffer plus the DTS active range (4 MiB); nothing fits or unknown -> 0 (Auto). Logs any cut.
#define CB_BUFFER_PISO_MB 100
int cacheboost_buffer_mb(int opcao, long livreMb);
// Off Android: the last cacheboost_backend_buffer value (webOS DTS/DV read-ahead).
int cacheboost_backend_buffer_mb(void);

// Host captures/tests: pretend the backend supports (1) or not (0) the
// features; -1 = the build's real answer.
void cacheboost_simular_suporte(int s);
#endif
