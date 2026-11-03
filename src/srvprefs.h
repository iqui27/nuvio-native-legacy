#ifndef NV_SRVPREFS_H
#define NV_SRVPREFS_H
// Empty selection string means all libraries (compatible with existing accounts).
// Explicit selection is newline-delimited IDs. Pure, shared by both adapters.
#include <string.h>
#include <stdlib.h>
#include <strings.h>
// Zero-initialized/legacy accounts retain the existing negotiation and ceilings.
typedef struct { int direto, bitrate; } SrvPlayback;
#define SRV_JF_BITRATE_MAX 120000000
#define SRV_PX_BITRATE_MAX 20000000
static inline int srv_playback_bitrate(SrvPlayback p, int cap) {
  return p.bitrate >= 1000000 && p.bitrate <= cap ? p.bitrate : cap;
}
static inline SrvPlayback srv_playback_ler(const char *direct, const char *bitrate, int cap) {
  char *end;
  long n = strtol(bitrate, &end, 10);
  SrvPlayback p = { !strcmp(direct, "1"), 0 };
  if (*bitrate && !*end && n >= 1000000 && n <= cap) p.bitrate = (int)n;
  return p;
}
static inline int srv_biblioteca_selecionada(const char *selection, const char *id) {
  if (!selection || !selection[0]) return 1;
  size_t n = strlen(id);
  for (const char *p = selection; p && *p; p = strchr(p, '\n')) {
    if (*p == '\n') p++;
    if (!strncmp(p, id, n) && (!p[n] || p[n] == '\n')) return 1;
    if (!strchr(p, '\n')) break;
  }
  return 0;
}
#endif
