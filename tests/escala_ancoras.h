// Shared capture probe: every recorded rectangle is also drawn by production GL.
#ifndef ESCALA_ANCORAS_TEST_H
#define ESCALA_ANCORAS_TEST_H
#include "gfx.h"
#include "text.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
void shot_begin(const char *name, float scale);
void shot_end(void);
void shot_anchor(const char *name, GfxRect r, int edges);
GfxRect shot_text(TxtLinha line);
GfxRect shot_box(GfxRect expected);
GfxRect shot_text_bounds(void);
GfxRect shot_component_bounds(GfxRect area);
void shot_disjoint(const char *name, GfxRect a, GfxRect b);
void shot_contains(const char *name, GfxRect outer, GfxRect inner);
void shot_check(const char *name, int ok);
void shot_reset(void);
void shot_no_text_overlap(void);
void fixture_home(float scale, int focused);
void fixture_detail(float scale, int section);
void fixture_search(float scale);
void fixture_settings(float scale, int category);
#endif
