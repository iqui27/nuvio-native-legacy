/* Host-only playback diagnostics. Never copy URL paths, queries or userinfo. */
#ifndef NV_RANGE_DIAG_H
#define NV_RANGE_DIAG_H
#include <stddef.h>
#include <string.h>

#define NV_RANGE_DIAG_HOST_MAX 254
static inline int nv_range_diag_dns(const char *s, size_t n) {
  size_t label = 0, last = 0;
  int dots = 0;
  if (!n || n >= NV_RANGE_DIAG_HOST_MAX) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '.') {
      if (!label || s[i - 1] == '-') return 0;
      label = 0; last = i + 1; dots++;
    } else {
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-') ||
          (!label && c == '-') || ++label > 63) return 0;
    }
  }
  if (!dots || label < 2 || s[n - 1] == '-') return 0;
  /* A letter-only final label excludes numeric addresses and opaque tokens. */
  for (size_t i = last; i < n; i++)
    if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))) return 0;
  return 1;
}
static inline void nv_range_diag_host(const char *url, char out[NV_RANGE_DIAG_HOST_MAX]) {
  const char *p = url ? strstr(url, "://") : NULL, *end, *host;
  size_t n;
  strcpy(out, "-");
  if (!p || !((p - url == 4 && !memcmp(url, "http", 4)) ||
              (p - url == 5 && !memcmp(url, "https", 5)))) return;
  host = p + 3; end = host + strcspn(host, "/?#");
  for (p = host; p < end; p++) if (*p == '@') host = p + 1;
  if (host < end && *host == '[') { strcpy(out, "address"); return; }
  p = memchr(host, ':', (size_t)(end - host));
  if (p) end = p;
  n = (size_t)(end - host);
  if (n && host[n - 1] == '.') n--;
  if (!nv_range_diag_dns(host, n)) { strcpy(out, "address"); return; }
  for (size_t i = 0; i < n; i++)
    out[i] = host[i] >= 'A' && host[i] <= 'Z' ? (char)(host[i] + ('a' - 'A')) : host[i];
  out[n] = 0;
}
#endif
