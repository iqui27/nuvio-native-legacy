#ifndef NV_GUIAV5_H
#define NV_GUIAV5_H
#include <stddef.h>
#include <time.h>

enum { G5_LIVE, G5_MOVIE, G5_SERIES, G5_EPISODE };
typedef struct {
  char id[80], source[48], name[140], group[64], poster[480];
  char url[2048], epg[64], ext[12], plot[600], trusted_id[32], backdrop[480];
  int kind, adult, age, season, episode;
} G5Item;
/* User-facing category; falls back to name when provider labels are absent. */
const char *g5_item_category(const G5Item *item, int kind);
/* A bounded catalog. Review scores never become parental age ratings. */
int g5_parse_xtream(const char *json, int kind, G5Item *out, int cap, int *total);
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

typedef struct {
  int hide_adult, max_age, ask_unrated, hide_locked, kids;
  char blocked_group[64], blocked_channel[80];
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
#endif
