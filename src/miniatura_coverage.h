#ifndef MINIATURA_COVERAGE_H
#define MINIATURA_COVERAGE_H

#include <stdint.h>

/* Nominal target slots reference independently owned frames. This helper never
 * owns a JPEG or duplicates its pointer: multiple slots may name one frame. */
enum {
  MINI_COVER_UNRESOLVED = 0,
  MINI_COVER_PENDING,
  MINI_COVER_FAILED,
  MINI_COVER_UNAVAILABLE,
  MINI_COVER_COVERED
};

#define MINI_COVER_GRID_MS INT64_C(10000)
#define MINI_COVER_TOLERANCE_MS INT64_C(5000)

typedef struct {
  int count;
  int64_t durationMs;
  int *frame;
  unsigned char *state;
} MiniCoverage;

static inline void mini_coverage_init(MiniCoverage *c, int count, int64_t durationMs,
                                      int *frame, unsigned char *state) {
  c->count = count > 0 && frame && state ? count : 0;
  c->durationMs = durationMs;
  c->frame = frame;
  c->state = state;
  for (int i = 0; i < c->count; i++) {
    frame[i] = -1;
    state[i] = MINI_COVER_UNRESOLVED;
  }
}

static inline int mini_coverage_slot_valid(const MiniCoverage *c, int slot) {
  return c && c->frame && c->state && slot >= 0 && slot < c->count;
}

/* A final nominal target beyond EOF represents EOF, never invented media time. */
static inline int64_t mini_coverage_target_ms(const MiniCoverage *c, int slot) {
  if (!mini_coverage_slot_valid(c, slot) || c->durationMs < 0) return -1;
  int64_t target = (int64_t)slot * MINI_COVER_GRID_MS;
  return target > c->durationMs ? c->durationMs : target;
}

static inline int mini_coverage_valid(const MiniCoverage *c, int slot, int64_t ptsMs) {
  int64_t target = mini_coverage_target_ms(c, slot);
  if (target < 0 || ptsMs < 0 || ptsMs > c->durationMs) return 0;
  int64_t drift = ptsMs >= target ? ptsMs - target : target - ptsMs;
  return drift <= MINI_COVER_TOLERANCE_MS;
}

/* A failed later attempt must not erase a previously verified binding. */
static inline void mini_coverage_mark(MiniCoverage *c, int slot, unsigned char state) {
  if (!mini_coverage_slot_valid(c, slot) || c->state[slot] == MINI_COVER_COVERED) return;
  if (state > MINI_COVER_UNAVAILABLE) return;
  c->frame[slot] = -1;
  c->state[slot] = state;
}

static inline int mini_coverage_bind(MiniCoverage *c, int slot, int frameIndex, int64_t ptsMs) {
  if (!mini_coverage_slot_valid(c, slot)) return 0;
  if (frameIndex < 0 || frameIndex >= c->count || !mini_coverage_valid(c, slot, ptsMs)) {
    mini_coverage_mark(c, slot, MINI_COVER_FAILED);
    return 0;
  }
  c->frame[slot] = frameIndex;
  c->state[slot] = MINI_COVER_COVERED;
  return 1;
}

static inline int mini_coverage_covered_count(const MiniCoverage *c) {
  if (!c || !c->frame || !c->state) return 0;
  int covered = 0;
  for (int slot = 0; slot < c->count; slot++)
    covered += c->state[slot] == MINI_COVER_COVERED && c->frame[slot] >= 0 && c->frame[slot] < c->count;
  return covered;
}

/* Slots with no keyframe near them at all: no thumbnail can ever cover them. */
static inline int mini_coverage_unavailable_count(const MiniCoverage *c) {
  if (!c || !c->state) return 0;
  int n = 0;
  for (int slot = 0; slot < c->count; slot++) n += c->state[slot] == MINI_COVER_UNAVAILABLE;
  return n;
}

/* Terminal failures/unavailable cues and attempted flags do not prove coverage. */
static inline int mini_coverage_complete(const MiniCoverage *c) {
  return c && c->count > 0 && mini_coverage_covered_count(c) == c->count;
}

/* Frame timestamps identify unique owners. Unoccupied frame entries use -1. */
static inline int mini_coverage_find_frame(const int64_t *framePts, int frameCount, int64_t ptsMs) {
  if (!framePts || frameCount <= 0 || ptsMs < 0) return -1;
  for (int i = 0; i < frameCount; i++) if (framePts[i] == ptsMs) return i;
  return -1;
}

/* Append-only frame publication is O(targets), instead of rebuilding the full
 * targets x frames map after every decoded JPEG. Existing equally close frames
 * use the same lower-pool-index tie break as rebuild. Use rebuild after removals. */
static inline int mini_coverage_offer_frame(MiniCoverage *c, const int64_t *framePts,
                                           int frameCount, int frameIndex) {
  if (!c || !framePts || frameIndex < 0 || frameIndex >= frameCount || frameIndex >= c->count)
    return mini_coverage_covered_count(c);
  int64_t pts = framePts[frameIndex];
  for (int slot = 0; slot < c->count; slot++) {
    if (!mini_coverage_valid(c, slot, pts)) continue;
    int old = c->frame[slot];
    if (c->state[slot] == MINI_COVER_COVERED && old >= 0 && old < frameCount &&
        mini_coverage_valid(c, slot, framePts[old])) {
      int64_t target = mini_coverage_target_ms(c, slot);
      int64_t oldPts = framePts[old];
      int64_t drift = pts >= target ? pts - target : target - pts;
      int64_t oldDrift = oldPts >= target ? oldPts - target : target - oldPts;
      if (oldDrift < drift || (oldDrift == drift && old <= frameIndex)) continue;
    }
    mini_coverage_bind(c, slot, frameIndex, pts);
  }
  return mini_coverage_covered_count(c);
}

/* Refresh nominal coverage from immutable frames without copying JPEGs.
 * Call init first for a fresh/load map. Existing PENDING/FAILED/UNAVAILABLE
 * states survive refresh unless a valid frame now covers them, so in-flight
 * targets cannot accidentally become eligible for duplicate queueing.
 * A covered slot whose backing frame disappeared becomes UNRESOLVED.
 * Ties retain the earlier frame-pool entry; invalid/empty timestamps are ignored.
 * Returns covered nominal targets, which can exceed the unique frame count. */
static inline int mini_coverage_rebuild(MiniCoverage *c, const int64_t *framePts, int frameCount) {
  if (!c || !c->frame || !c->state) return 0;
  if (!framePts || frameCount <= 0) frameCount = 0;
  if (frameCount > c->count) frameCount = c->count;
  for (int slot = 0; slot < c->count; slot++) {
    int best = -1;
    int64_t bestDrift = MINI_COVER_TOLERANCE_MS + 1;
    int64_t target = mini_coverage_target_ms(c, slot);
    for (int frame = 0; frame < frameCount; frame++) {
      int64_t pts = framePts[frame];
      if (!mini_coverage_valid(c, slot, pts)) continue;
      int64_t drift = pts >= target ? pts - target : target - pts;
      if (drift < bestDrift) { best = frame; bestDrift = drift; }
    }
    if (best >= 0) mini_coverage_bind(c, slot, best, framePts[best]);
    else {
      c->frame[slot] = -1;
      if (c->state[slot] == MINI_COVER_COVERED) c->state[slot] = MINI_COVER_UNRESOLVED;
    }
  }
  return mini_coverage_covered_count(c);
}

#endif
