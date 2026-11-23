#ifndef NV_CATSNAP_H
#define NV_CATSNAP_H
#include <stdint.h>

#define CATSNAP_VERSION 1
#define CATSNAP_MAX_AGE (INT64_C(7) * 24 * 60 * 60)

static inline int catsnap_needs_fetch(int readable, unsigned version,
                                     int identity_matches, int refresh,
                                     int64_t fetched_at, int64_t now) {
  return !readable || version != CATSNAP_VERSION || !identity_matches ||
         refresh || fetched_at <= 0 || now < fetched_at ||
         now - fetched_at > CATSNAP_MAX_AGE;
}
#endif
