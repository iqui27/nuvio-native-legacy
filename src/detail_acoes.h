// Action row (detail.c): spacing around a focused button. Pure, so
// tests/detail_acoes_gap.c can sample it.
#ifndef NV_DETAIL_ACOES_H
#define NV_DETAIL_ACOES_H

// Focused buttons grow around their centre (desenhaBotao, scale `s`). The
// layout must advance by the growth on the right, and shift by it on the left,
// or the gap to the neighbours shrinks by it (Play pill: 18 -> ~0).
static inline float det_foco_extra(float w, float s) { return w * (s - 1.0f) * 0.5f; }

// Distance from a button's layout x to the next button's x.
static inline float det_avanco(float w, int focado, float s, float gap) {
  return w + (focado ? det_foco_extra(w, s) : 0.0f) + gap;
}
#endif
