// Personal-server MATCHING INDEX (srvidx): which catalogue titles a server has.
//
// One index per profile and server. Each movie, series and (fetched on
// demand) episode of the server becomes one row with its external ids
// (IMDb/TMDB/TVDB), a hash of the normalised title, the year, the best known
// quality and the watched flag. A regular catalogue title ("tt...",
// "tmdb:...", with ":season:episode") is looked up here WITHOUT NETWORK: only
// a hit makes the server be asked for playback info. No hit, no request.
//
// MATCH RULES (plan §4.3), in this order:
//   1. IMDb id and kind (movie/series);
//   2. TMDB id and kind;
//   3. TVDB id (series);
//   4. no external id at all: normalised title + year (±1) + kind, ONLY when
//      exactly one row of the index has them (srvidx_achar_titulo; ambiguous
//      is no match). The title comes from the page the person opened.
//   Episodes: the series by the rules above, then (season, episode).
//
// DISK. srvidx-p<N>-<kind>.txt in the data dir (kind "jf", "em", "px": one
// server per kind per profile until F6), plain text, written with
// dados_gravar_leve (re-obtainable). The server tag inside the file must
// match the connected server, or the file is ignored. It holds item ids and
// external ids, never a title, a path, an address or a token. Deleted with
// the server's sign-out and with the Nuvio account logout.
//
// INCREMENTAL. `marca` is the newest server save time seen; the next pass
// asks only for items saved since then, and a count check decides when a
// full rebuild is needed (deletions are not reported incrementally).
//
// Pure module: no network, no locks. The owner (jellyfin.c / plex.c) keeps
// its index under its own mutex.
#ifndef NV_SRVIDX_H
#define NV_SRVIDX_H
#include <stddef.h>

#define SRVIDX_ITEM 40
#define SRVIDX_MAX 50000          // rows per server (movies+series+episodes)

enum { SRVIDX_HDR_SDR = 0, SRVIDX_HDR_HDR10, SRVIDX_HDR_HDR10P, SRVIDX_HDR_DV, SRVIDX_HDR_HLG };

typedef struct {
  char imdb[12];             // "tt0111161" or ""
  long tmdb, tvdb;           // 0 = none
  char tipo;                 // 'm' movie, 's' series, 'e' episode
  unsigned char hdr;         // SRVIDX_HDR_* of the best version (0 unknown/SDR)
  unsigned char visto;       // the server says watched
  unsigned short altura;     // 2160/1080/720/480 of the best version, 0 unknown
  short ano;                 // 0 unknown
  short temporada, episodio; // 'e' only
  unsigned tituloHash;       // srvidx_titulo_hash, 0 = none
  long long atualizado;      // server save time (unix seconds), 0 unknown
  char item[SRVIDX_ITEM];    // the server's item id (hex Jellyfin, numeric Emby/Plex)
  char serie[SRVIDX_ITEM];   // 'e': the series item id
} SrvIdxItem;

typedef struct {
  SrvIdxItem *v;
  int n, cap;
  int *tab, ntab;    // open-addressing table (kind+item -> row+1): upsert stays O(1)
  long long marca;   // newest `atualizado` seen (incremental passes ask from here)
  int total;         // movies+series the server reported on the last full pass
} SrvIdx;

void srvidx_iniciar(SrvIdx *x);
void srvidx_limpar(SrvIdx *x);
// Insert or replace (same kind and item id). 0 when full or invalid.
int  srvidx_por(SrvIdx *x, const SrvIdxItem *e);
// Moves everything of `de` into `x` (rows of `de` win), keeps x->marca the newest.
void srvidx_juntar(SrvIdx *x, const SrvIdx *de);
int  srvidx_n_tipo(const SrvIdx *x, char tipo);
// Rules 1-3. -1 none. `tipo` 'm' or 's'.
int  srvidx_achar(const SrvIdx *x, const char *imdb, long tmdb, long tvdb, char tipo);
// Rule 4: -1 none, -2 more than one candidate (ambiguous: never a match).
int  srvidx_achar_titulo(const SrvIdx *x, unsigned hash, int ano, char tipo);
// The episode row of a series item, -1 none.
int  srvidx_achar_ep(const SrvIdx *x, const char *serie, int temporada, int episodio);
int  srvidx_n_eps(const SrvIdx *x, const char *serie);

// Normalised title hash: lowercase, Latin accents folded, punctuation and
// spaces dropped, one leading article dropped ("The Office" == "office",
// "Spider-Man" == "spiderman"). 0 for an empty title.
unsigned srvidx_titulo_hash(const char *titulo);
// "tt0111161", "tt0944947:2:5", "tmdb:278", "tmdb:1399:1:3" -> ids. 0 when not
// an external id.
int  srvidx_id_titulo(const char *id, char imdb[16], long *tmdb, int *temp, int *ep);
// ISO-8601 "2026-10-01T12:34:56.0000000Z" -> unix seconds (UTC). 0 on error.
long long srvidx_iso(const char *iso);
// The four first digits of "2019 · 2 temporadas", 0 when absent.
int  srvidx_ano_de(const char *meta);
// 1 when the URL's host is on the local network (private IPv4, localhost,
// *.local, a bare host name, a plex.direct name of a private address).
int  srvidx_host_local(const char *url);

// Text form (tests and disk). serializar returns malloc'd text.
char *srvidx_serializar(const SrvIdx *x, const char *tag);
int   srvidx_desserializar(SrvIdx *x, const char *txt, const char *tag);
// Disk: srvidx-p<perfil>-<tipoSrv>.txt. ler returns rows read (0 = none or
// another server's file).
int  srvidx_gravar(const SrvIdx *x, int perfil, const char *tipoSrv, const char *tag);
int  srvidx_ler(SrvIdx *x, int perfil, const char *tipoSrv, const char *tag);
void srvidx_apagar(int perfil, const char *tipoSrv);
#endif
