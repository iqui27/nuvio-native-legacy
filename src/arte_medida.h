#ifndef ARTE_MEDIDA_H
#define ARTE_MEDIDA_H
#include <stddef.h>
// ARTWORK DRAW MEASUREMENT, opt-in (NV_ART_DRAW_MEASURE=1), default off.
// Pairs the decoded size of a tex_cache texture with the physical pixels one
// gfx_rect draw covers on the render target. One "[art-draw]" line per
// resource and meaningful size change, at most ARTE_MEDIDA_TETO per process.
// Fixed labels and integers only: `art=` is a process-local ordinal (first
// sight order, not persistent, not derived from URL or GL name).
// docs/performance/artwork-draw-measurement.md has the field semantics.
#define ARTE_MEDIDA_TETO 48
// Cache lookups per process (hits and misses). Each name that misses is
// remembered (up to ARTE_MEDIDA_TETO names) and not looked up again; when the
// budget runs out the probe turns itself off, like the line cap.
#define ARTE_MEDIDA_BUSCAS (4 * ARTE_MEDIDA_TETO)

// -1 = not resolved yet, 0 = off (also after the cap), 1 = on. gfx_rect tests
// this int before anything else, so the disabled path is one compare.
extern int arte_medida_ligada;
int arte_medida_resolver(void);   // reads the env once; returns the new state

// One draw. (x0,y0)-(x1,y1): the whole quad in render-target px, after layer
// scale and group transform. clip: scissor {x, y, w, h} in the same px or
// NULL. rtW/rtH: render target. texAsp: the w/h the shader uses (0 = stretch).
// contem: the shader may switch cover to contain (GFX_CARD rule). asp: the
// shader uAspect (layout w/h; differs from the px aspect when the target scale
// is not uniform). sub: the uSub window; anything but {0,0,1,1} (the
// gfx_rect_furos strips) is skipped. esc: layer scale; pxUnit: target px per
// layout unit. dims: decoded size of `tex` from the cache, called only when
// the line is not deduped and the name has not missed before. Writes the line
// to out and returns 1 when it should be printed.
int arte_medida_amostra(unsigned tex, int modo, float texAsp, int contem,
                        float asp, const float *sub,
                        float x0, float y0, float x1, float y1, const int *clip,
                        int rtW, int rtH, float esc, float pxUnit,
                        int (*dims)(unsigned tex, int *w, int *h),
                        char *out, size_t n);
#endif
