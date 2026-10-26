// Media URLs are owned by value copies, bounded per source/list, never global.
#ifndef NV_MEDIAURL_H
#define NV_MEDIAURL_H
#include <stddef.h>
#include <string.h>

// Up to 8191 signed bytes, including query; oversized sources are skipped.
#define NV_MEDIA_URL_MAX 8192

static inline int nv_media_url_guardar(const char *url, char *dst, size_t tam) {
  size_t len = strlen(url);
  if (len >= NV_MEDIA_URL_MAX || len >= tam) return 0;
  memmove(dst, url, len + 1);
  return 1;
}

static inline const char *nv_media_url(const char *url) { return url; }
#endif
