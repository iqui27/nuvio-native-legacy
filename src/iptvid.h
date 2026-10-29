#ifndef NV_IPTVID_H
#define NV_IPTVID_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Account zero retains the original IDs stored in favourites/history. Added
// accounts carry a fixed-width identity, independent of their name/order.
static inline int iptvid_ler(const char *prefixo, const char *id, unsigned *fonte,
                             char *original, unsigned n) {
  size_t k = strlen(prefixo);
  if (!id || strncmp(id, prefixo, k)) return 0;
  const char *p = id + k;
  unsigned f = 0;
  if (*p == '@') {
    p++;
    for (int i = 0; i < 8; i++, p++) {
      unsigned char c = (unsigned char)*p;
      unsigned v = c >= '0' && c <= '9' ? c - '0' :
                   c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                   c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
      if (v > 15) return 0;
      f = (f << 4) | v;
    }
    if (*p++ != ':' || f == UINT32_MAX) return 0;
  }
  if (!*p) return 0;
  if (fonte) *fonte = f;
  if (original) {
    int m = snprintf(original, n, "%s%s", prefixo, p);
    if (m < 0 || (unsigned)m >= n) return 0;
  }
  return 1;
}
static inline int iptvid_montar(const char *prefixo, unsigned fonte, const char *original,
                               char *out, unsigned n) {
  size_t k = strlen(prefixo);
  if (!original || strncmp(original, prefixo, k)) return 0;
  int m = fonte ? snprintf(out, n, "%s@%08x:%s", prefixo, fonte, original + k)
                : snprintf(out, n, "%s", original);
  return m >= 0 && (unsigned)m < n;
}
#endif
