/* Automatic sync of another version's audio: the pure part (dts_match.h).
 * Included by dts_engine.c (and by its unit test): static, no build list to
 * update. */
#include "dts_match.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* RBJ biquads, Q = 0.707 (Butterworth). Two in series per band. */
static void dts_bq_set(DtsBiquad *q, int hp, double fc, double fs) {
  double w0 = 2.0 * M_PI * fc / fs, c = cos(w0), al = sin(w0) / (2.0 * 0.7071067811865476), a0 = 1.0 + al;
  memset(q, 0, sizeof *q);
  q->b0 = (hp ? (1.0 + c) : (1.0 - c)) / 2.0 / a0;
  q->b1 = (hp ? -(1.0 + c) : (1.0 - c)) / a0;
  q->b2 = q->b0;
  q->a1 = -2.0 * c / a0;
  q->a2 = (1.0 - al) / a0;
}
static void dts_bq_pass(DtsBiquad *q) { memset(q, 0, sizeof *q); q->b0 = 1.0; }
static double dts_bq_run(DtsBiquad *q, double x) {
  double y = q->b0 * x + q->z1;
  q->z1 = q->b1 * x - q->a1 * y + q->z2;
  q->z2 = q->b2 * x - q->a2 * y;
  return y;
}
static void dts_match_env_init(DtsMatchEnv *v, int rate, float *const env[], int max) {
  memset(v, 0, sizeof *v);
  v->blk = rate / DTS_MATCH_HZ; if (v->blk < 1) v->blk = 1;
  v->max = max;
  for (int b = 0; b < DTS_MATCH_BANDS; b++) v->env[b] = env[b];
  dts_bq_pass(&v->f[0][0]); dts_bq_pass(&v->f[0][1]);
  /* A band above what the sample rate carries is left out. */
  if (200.0 < 0.45 * rate) { dts_bq_set(&v->f[1][0], 0, 200.0, rate); dts_bq_set(&v->f[1][1], 0, 200.0, rate); }
  else v->env[1] = NULL;
  if (3000.0 < 0.45 * rate) { dts_bq_set(&v->f[2][0], 1, 300.0, rate); dts_bq_set(&v->f[2][1], 0, 3000.0, rate); }
  else v->env[2] = NULL;
  if (4000.0 < 0.45 * rate) { dts_bq_set(&v->f[3][0], 1, 4000.0, rate); dts_bq_set(&v->f[3][1], 1, 4000.0, rate); }
  else v->env[3] = NULL;
}
static void dts_match_env_push(DtsMatchEnv *v, const float *mono, int n) {
  for (int i = 0; i < n && v->n < v->max; i++) {
    for (int b = 0; b < DTS_MATCH_BANDS; b++) {
      if (!v->env[b]) continue;
      double y = dts_bq_run(&v->f[b][1], dts_bq_run(&v->f[b][0], mono[i]));
      v->acc[b] += y * y;
    }
    if (++v->accn == v->blk) {
      for (int b = 0; b < DTS_MATCH_BANDS; b++)
        if (v->env[b]) { v->env[b][v->n] = (float)(v->acc[b] / v->blk); v->acc[b] = 0; }
      v->n++; v->accn = 0;
    }
  }
}

/* Onsets: the rise of the log energy from one block to the next, so loudness
 * differences between the two mixes do not matter. */
static void onsets(const float *e, int n, float *y) {
  double prev = log10((double)e[0] + 1e-10);
  y[0] = 0.0f;
  for (int i = 1; i < n; i++) {
    double v = log10((double)e[i] + 1e-10), d = v - prev;
    y[i] = d > 0.0 ? (float)d : 0.0f;
    prev = v;
  }
}

/* Optional: called now and then during a wide search (the engine sleeps a
 * moment there, so the interface keeps its frames). */
static void (*dts_match_respiro)(void);

/* Weights of the bands: speech (the band a dub changes) counts half. */
static const double dts_match_weight[DTS_MATCH_BANDS] = { 1.0, 1.0, 0.5, 1.0 };

/* win_s of this file's capture centred at `pos` (0..1 of it) against the
 * other's, offsets center_s +- radius_s. */
static int dts_match_search_at(const DtsMatchSig *m, const DtsMatchSig *d, double ratio,
                               double center_s, double radius_s, double win_s, double pos,
                               DtsMatchResult *out) {
  const double step = 1.0 / DTS_MATCH_HZ;
  int w = (int)(win_s * DTS_MATCH_HZ);
  int lags = (int)(radius_s * DTS_MATCH_HZ + 0.5), best = -1, ok = 0, nb = 0;
  int same = fabs(ratio - 1.0) < 1e-9;
  float *ym[DTS_MATCH_BANDS] = { 0 }, *yd[DTS_MATCH_BANDS] = { 0 };
  double *a[DTS_MATCH_BANDS] = { 0 }, saa[DTS_MATCH_BANDS] = { 0 }, *c = NULL;
  memset(out, 0, sizeof *out);
  if (m->n < w + 2) w = m->n - 2;           /* a shorter capture: what there is (5 s at least) */
  if (w < 5 * DTS_MATCH_HZ || d->n < w / 2 || lags < 1 || ratio <= 0.5 || ratio >= 2.0) return 0;
  int m0 = (int)(pos * m->n) - w / 2;
  if (m0 < 0) m0 = 0;
  if (m0 > m->n - w) m0 = m->n - w;
  for (int b = 0; b < DTS_MATCH_BANDS; b++) {
    if (!m->env[b] || !d->env[b]) continue;
    ym[b] = malloc(sizeof(float) * (size_t)m->n); yd[b] = malloc(sizeof(float) * (size_t)d->n);
    a[b] = malloc(sizeof(double) * (size_t)w);
    if (!ym[b] || !yd[b] || !a[b]) goto done;
    onsets(m->env[b], m->n, ym[b]); onsets(d->env[b], d->n, yd[b]);
    double s = 0;
    for (int i = 0; i < w; i++) s += ym[b][m0 + i];
    for (int i = 0; i < w; i++) { a[b][i] = ym[b][m0 + i] - s / w; saa[b] += a[b][i] * a[b][i]; }
    if (saa[b] <= 1e-12) continue;   /* silent band in this file */
    nb++;
  }
  c = malloc(sizeof *c * (size_t)(2 * lags + 1));
  if (!nb || !c) goto done;
  for (int k = -lags; k <= lags; k++) {
    double o = center_s + k * step;
    if (dts_match_respiro && radius_s > 15.0 && !((k + lags) & 255)) dts_match_respiro();
    /* this file at t pairs with the other at (t - offset) / ratio */
    double x0 = ((m->t0 + m0 * step - o) / ratio - d->t0) / step, dx = 1.0 / ratio;
    double v = -2.0;
    if (same) x0 = (double)lround(x0);
    if (x0 >= 0 && x0 + (w - 1) * dx <= d->n - 1) {
      double sum = 0, ws = 0;
      for (int b = 0; b < DTS_MATCH_BANDS; b++) {
        if (!a[b] || saa[b] <= 1e-12) continue;
        double sb = 0, sbb = 0, sab = 0;
        for (int i = 0; i < w; i++) {
          double x = x0 + i * dx, y;
          if (same) y = yd[b][(long)x];
          else {
            long j = (long)x; double f = x - j;
            y = j + 1 < d->n ? yd[b][j] * (1.0 - f) + yd[b][j + 1] * f : yd[b][j];
          }
          sb += y; sbb += y * y; sab += a[b][i] * y;
        }
        double vb = sbb - sb * sb / w;
        if (vb <= 1e-12) continue;   /* silent there in the other version */
        sum += dts_match_weight[b] * sab / sqrt(saa[b] * vb);
        ws += dts_match_weight[b];
      }
      if (ws > 0) v = sum / ws;
    }
    c[k + lags] = v;
    if (v > -2.0) { ok = 1; if (best < 0 || v > c[best]) best = k + lags; }
  }
  if (ok) {
    double p = c[best], q = -2.0;
    for (int i = 0; i < 2 * lags + 1; i++)
      if (abs(i - best) > DTS_MATCH_HZ / 4 && c[i] > q) q = c[i];
    /* Sub-block peak from its neighbours (parabola). */
    double frac = 0.0;
    if (best > 0 && best < 2 * lags && c[best - 1] > -2.0 && c[best + 1] > -2.0) {
      double den = c[best - 1] - 2.0 * p + c[best + 1];
      if (fabs(den) > 1e-12) frac = 0.5 * (c[best - 1] - c[best + 1]) / den;
      if (frac > 0.5) frac = 0.5;
      if (frac < -0.5) frac = -0.5;
    }
    out->offset_s = center_s + (best - lags + frac) * step;
    out->score = p;
    out->margin = q > -2.0 ? p - q : p;
    /* Applied on its own only when clearly right (owner's log 07/11: a 0.27
     * peak was wrong); a wide search has more chances of a lucky peak. */
    out->confident = p >= 0.35 && out->margin >= (radius_s > 15.0 ? 0.12 : 0.10);
    /* The best separate peaks, for the person to try when not confident. */
    for (int t = 0; t < DTS_MATCH_CAND; t++) {
      int bi = -1;
      for (int i = 0; i < 2 * lags + 1; i++) {
        int near = 0;
        if (c[i] <= -2.0) continue;
        for (int u = 0; u < out->ncand; u++)
          if (fabs(center_s + (i - lags) * step - out->cand_off[u]) < 0.5) near = 1;
        if (!near && (bi < 0 || c[i] > c[bi])) bi = i;
      }
      if (bi < 0) break;
      out->cand_off[out->ncand] = bi == best ? out->offset_s : center_s + (bi - lags) * step;
      out->cand_score[out->ncand++] = c[bi];
    }
  }
done:
  for (int b = 0; b < DTS_MATCH_BANDS; b++) { free(ym[b]); free(yd[b]); free(a[b]); }
  free(c);
  return ok;
}

static int dts_match_search(const DtsMatchSig *m, const DtsMatchSig *d, double ratio,
                            double center_s, double radius_s, DtsMatchResult *out) {
  return dts_match_search_at(m, d, ratio, center_s, radius_s, DTS_MATCH_WIN_S, 0.5, out);
}
/* A place found without confidence, checked on two separate halves of the
 * capture (15 s each): both must peak there again (within 60 ms) for it to be
 * taken. Two windows that do not overlap rarely agree by chance (a peak lands
 * in a 120 ms slot of +-3 s about 2 % of the time, both about 0.05 %). */
static int dts_match_verify(const DtsMatchSig *m, const DtsMatchSig *d, double ratio, double at_s) {
  DtsMatchResult a, b;
  if (!dts_match_search_at(m, d, ratio, at_s, 3.0, 15.0, 0.25, &a) ||
      !dts_match_search_at(m, d, ratio, at_s, 3.0, 15.0, 0.75, &b)) return 0;
  return fabs(a.offset_s - at_s) < 0.06 && fabs(b.offset_s - at_s) < 0.06 && a.score >= 0.10 && b.score >= 0.10;
}
#if defined(__GNUC__)
__attribute__((unused))
#endif
static int dts_match_offset(const float *main_env, int nm, double t0m,
                            const float *ext_env, int nd, double t0d,
                            double center_s, double radius_s, DtsMatchResult *out) {
  DtsMatchSig m = { { main_env, NULL, NULL, NULL }, nm, t0m }, d = { { ext_env, NULL, NULL, NULL }, nd, t0d };
  return dts_match_search(&m, &d, 1.0, center_s, radius_s, out);
}
