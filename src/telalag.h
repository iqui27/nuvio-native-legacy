// GPU QUEUE LIMIT (.tpk, owner 25/10/2026).
//
// A GL fence after each frame's last draw, polled (never waited on) at the
// next frames. The limit waits at the start of a frame until the GPU is at
// most one frame behind, but only where it helped on the S95C: Render
// resolution Auto at 75 % on heavy signs (telalag.c, limite).
//
// Needs an OpenGL ES 3 context (fences are core there). Elsewhere, and off the
// .tpk, every function is a no-op.
#ifndef NV_TELALAG_H
#define NV_TELALAG_H

void telalag_quadro_inicio(void);   // GL thread, before the frame's first draw (waits if limited)
void telalag_quadro_fim(void);      // GL thread, right before the swap

#endif
