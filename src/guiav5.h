#ifndef NV_GUIAV5_H
#define NV_GUIAV5_H
#include <stddef.h>
#include <time.h>

enum { G5_LIVE, G5_MOVIE, G5_SERIES, G5_EPISODE };
typedef struct {
  char id[80], source[48], name[140], group[64], poster[480];
  char url[2048], epg[64], ext[12], plot[600], trusted_id[32], backdrop[480];
  char year[8], rating[12], genre[96];
  int kind, adult, age, season, episode;
} G5Item;
/* User-facing category; falls back to name when provider labels are absent. */
const char *g5_item_category(const G5Item *item, int kind);
/* A bounded catalog. Review scores never become parental age ratings. */
int g5_parse_xtream(const char *json, int kind, G5Item *out, int cap, int *total);
int g5_parse_xtream_item(const char *json, const char *end, int kind, G5Item *out);
int g5_parse_xtream_page(const char *json,int kind,G5Item *out,int cap,int skip,int *total);
int g5_parse_episodes(const char *json,int season,G5Item *out,int cap);
typedef struct {
  G5Item *items, pending;
  int cap, count, total, header, failed;
  size_t used;
  char line[8192];
} G5Playlist;
void g5_playlist_init(G5Playlist *p, G5Item *items, int cap);
int g5_playlist_feed(G5Playlist *p, const void *bytes, size_t n);
int g5_playlist_finish(G5Playlist *p);

#define G5_RULES_MAX 64
typedef struct {
  int hide_adult, max_age, ask_unrated, hide_locked, kids;
  char blocked_group[64], blocked_channel[80]; /* Existing saved rules. */
  char blocked_groups[G5_RULES_MAX][64], blocked_channels[G5_RULES_MAX][80];
  int group_count, channel_count;
} G5Rules;
typedef struct {
  int profile;
  time_t expires;
  char source[48], id[80];
} G5Lease;
int g5_restricted(const G5Rules *r, const G5Lease *lease, const G5Item *item,
                  int profile, time_t now);
void g5_grant(G5Lease *lease, int profile, const char *source, const char *id,
              time_t now, int seconds, int verified);
int g5_live_slots(int requested, int decoders, int provider_max, int provider_used);
int g5_http_url(const char *url);

/* --- multiview ------------------------------------------------------------
 * Pure layout/limits/playback-controller shared by the UI (guiav5ui.c) and
 * the tests. No SDL, no GL, no I/O. */
typedef struct { float x, y, w, h; } G5MvRect;
/* Largest 16:9 slots that fit the area, letterboxed (centered), never
 * squashed. mode 2: side by side; 3: big left + two stacked right; 4: 2x2.
 * Returns the slot count (mode clamped to 2..4). */
int g5_mv_layout(int mode, float x, float y, float w, float h, float gap, G5MvRect out[4]);

/* Simultaneous-stream cap of one source, -1 = unlimited. The key is the
 * guide's stable source key ("xt" Xtream, "st" Stalker, "m…" M3U, "a…"
 * addon). Xtream uses the account max_connections (<=0 or unknown: 1, the
 * safe default that never risks a ban). Stalker portals: 1. M3U files/lists
 * and addons carry no account cap. */
int g5_mv_source_cap(const char *key, int xtream_max);
/* How many of the n slots hold a channel of `key`. */
int g5_mv_source_use(const char keys[][24], int n, const char *key);

/* Slot playback controller. A slot opens ONCE per channel change and is only
 * re-opened after a real stream error, with backoff (1/3/6/10 s, 4 tries).
 * Periodic data refreshes (EPG, lists, keepalive) are not playback events and
 * never re-open a playing slot. */
enum { G5_MV_VAZIO = 0, G5_MV_ABRINDO, G5_MV_TOCANDO, G5_MV_ERRO,
       G5_MV_SEM_FONTE, G5_MV_SEM_DECODER };
typedef struct {
  char id[80], fonte[24];
  int estado, tentativas, pediu;
  unsigned proxima;      /* ms: next retry instant while estado == G5_MV_ERRO */
} G5MvSlot;
typedef struct { G5MvSlot slot[4]; int aberturas; } G5MvCtl;
/* Channel now wanted in a slot ("" clears). Same id as current: no-op. */
void g5_mv_ctl_set(G5MvCtl *c, int slot, const char *id, const char *fonte, unsigned agora);
void g5_mv_ctl_erro(G5MvCtl *c, int slot, unsigned agora);
void g5_mv_ctl_pronto(G5MvCtl *c, int slot);
void g5_mv_ctl_refresh(G5MvCtl *c);   /* periodic data refresh: never reopens */
/* 1 once when the slot must open/reopen now (edge: ABRINDO stays silent). */
int g5_mv_ctl_abrir(G5MvCtl *c, int slot, unsigned agora);
#endif
