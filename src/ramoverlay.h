#ifndef NV_RAMOVERLAY_H
#define NV_RAMOVERLAY_H
#include <stdint.h>

/* Render-thread only; disabled means no /proc reads and no drawing. */
void ramoverlay_desenhar(uint32_t now, int enabled);
#endif
