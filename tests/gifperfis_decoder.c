// Real shared decoder; expose queue readiness without guessing worker timing.
#define NV_ANDROID
#include <stdio.h>
#include <string.h>
// Since 211efce22 Android TVs under 3 GB (MemTotal) get no animation worker:
// run as a 4 GB TV, the device class these avatars animate on.
static FILE *meminfo4gb(const char *path, const char *mode) {
  static const char m[] = "MemTotal: 4194304 kB\n";
  if (!strcmp(path, "/proc/meminfo")) return fmemopen((void *)m, sizeof m - 1, "r");
  return fopen(path, mode);
}
#define fopen meminfo4gb
#include "../src/gif.c"
#undef fopen
int gifperfis_ready(void) {
  GifFaixa frame;
  return gif_fio_pegar(fio, &frame);
}
int gifperfis_started(void) { return mostrou; }
