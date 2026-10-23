// Host-side scratch dir for tests: $TMPDIR (trailing slashes stripped) or /tmp.
// nvt("/x") -> "<tmp>/x" in a small ring of static buffers (valid for the next
// 64 calls); nvt_set(buf, n, "/x") copies it into a caller buffer, which is what
// char dir[] = "...XXXXXX" + mkdtemp() needs.
#ifndef NVTMP_H
#define NVTMP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NVT_MAX 1024

static inline const char *nvt(const char *suf) {
  static char ring[64][NVT_MAX + 256];
  static unsigned next;
  char *b = ring[next++ % 64];
  const char *t = getenv("TMPDIR");
  size_t n;
  if (!t || !*t) t = "/tmp";
  n = strlen(t);
  while (n > 1 && t[n - 1] == '/') n--;
  if (n > NVT_MAX - 1) n = NVT_MAX - 1;
  snprintf(b, NVT_MAX + 256, "%.*s%s", (int)n, t, suf);
  return b;
}
#define nvt_dir() nvt("")
static inline void nvt_set(char *dst, size_t cap, const char *suf) { snprintf(dst, cap, "%s", nvt(suf)); }
#endif
