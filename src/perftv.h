// Opt-in TV measurement. Called only on the render thread; no user data in logs.
#ifndef NV_PERFTV_H
#define NV_PERFTV_H
#include "fpscena.h"

typedef struct {
  FpsAmostra frames;
  char screen[64];
  double elapsed;
  long text, blur, glass;
  int over33;
} PerfTv;

extern int perftv_enabled;
extern int perftv_uploads, perftv_blur, perftv_glass;
// Flush on screen change, 3 seconds, or the bounded sample capacity.
int perftv_frame(PerfTv *p, const char *screen, double ms, int uploads,
                 int text, int blur, int glass, char *out, size_t cap);
int perftv_flush(PerfTv *p, char *out, size_t cap);
#endif
