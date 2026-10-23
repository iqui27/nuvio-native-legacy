#ifndef NV_SRVPREFS_H
#define NV_SRVPREFS_H
// Empty selection string means all libraries (compatible with existing accounts).
// Explicit selection is newline-delimited IDs. Pure, shared by both adapters.
#include <string.h>
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
