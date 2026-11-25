/* Automatic sync of another version's audio (src/dts/dts_match.c): a shared
 * "music and effects" bed with different speech on top is found at the right
 * offset; unrelated audio is not trusted. */
#include "../src/dts/dts_match.inc"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned seed = 12345;
static double rnd(void) { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xFFFF) / 65536.0; }

/* Bursts that decay (hits, cuts, music notes) at random places. */
static void bursts(float *e, int n, int count, double amp, unsigned s) {
  seed = s;
  for (int k = 0; k < count; k++) {
    int at = (int)(rnd() * n);
    double a = amp * (0.3 + rnd());
    for (int i = at; i < n && i < at + 60; i++) e[i] += (float)(a * exp(-(i - at) / 12.0));
  }
}

/* Decaying hits at times list[] (seconds), sampled at t. */
static double bed_at(double t, const double *list, const double *amp, int n) {
  double v = 1e-4;
  for (int i = 0; i < n; i++) { double d = t - list[i]; if (d >= 0 && d < 0.6) v += amp[i] * exp(-d / 0.12); }
  return v;
}

int main(void) {
  enum { N = DTS_MATCH_MAX, SHIFT = 70 };   /* 0.70 s */
  static float bed[N + 400], m[N], d[N], other[N];
  DtsMatchResult r;
  bursts(bed, N + 400, 900, 1.0, 1);
  for (int i = 0; i < N; i++) { m[i] = 1e-4f + bed[i + 200]; d[i] = 1e-4f + bed[i + 200 - SHIFT]; }
  bursts(m, N, 500, 0.6, 2);    /* this file's speech */
  bursts(d, N, 500, 0.6, 3);    /* the dub's speech: elsewhere */
  /* The other version is 0.7 s later: its timestamps need -0.7 s. Same window start. */
  assert(dts_match_offset(m, N, 100.0, d, N, 100.0, 0.0, 10.0, &r) == 1);
  printf("dub 0.70 s late: offset %.3f s score %.2f margin %.2f confident %d\n", r.offset_s, r.score, r.margin, r.confident);
  assert(r.confident && fabs(r.offset_s + 0.70) < 0.02);
  /* Searched around the current offset: the same audio with its timestamps
   * 2 s earlier (the other version's clock) needs +2 s more: +1.30 s. */
  assert(dts_match_offset(m, N, 100.0, d, N, 98.0, 2.0, 10.0, &r) == 1);
  assert(r.confident && fabs(r.offset_s - 1.30) < 0.02);
  /* A voice-over over the original ducked low: the right offset is among the
   * candidates offered to the person, trusted or not. */
  { static float lk[N];
    for (int i = 0; i < N; i++) lk[i] = 1e-4f + 0.25f * bed[i + 200 - SHIFT];
    bursts(lk, N, 700, 1.2, 5);
    assert(dts_match_offset(m, N, 100.0, lk, N, 100.0, 0.0, 10.0, &r) == 1);
    printf("voice-over: offset %.3f s score %.2f margin %.2f confident %d, candidates", r.offset_s, r.score, r.margin, r.confident);
    int hit = 0;
    for (int i = 0; i < r.ncand; i++) { printf(" %.2f(%.2f)", r.cand_off[i], r.cand_score[i]); hit |= fabs(r.cand_off[i] + 0.70) < 0.03; }
    printf("\n");
    assert(r.ncand >= 2 && hit && fabs(r.cand_off[1] - r.cand_off[0]) >= 0.5);
    /* The two halves of the capture agree on the right place only. */
    DtsMatchSig sm = { { m, NULL, NULL, NULL }, N, 100.0 }, sl = { { lk, NULL, NULL, NULL }, N, 100.0 };
    int good = dts_match_verify(&sm, &sl, 1.0, -0.70), bad = 0;
    for (int i = 0; i < r.ncand; i++) if (fabs(r.cand_off[i] + 0.70) >= 0.03) bad += dts_match_verify(&sm, &sl, 1.0, r.cand_off[i]);
    printf("voice-over checked on two halves: right %d, wrong ones passing %d\n", good, bad);
    assert(good && !bad); }
  /* Unrelated audio: no trusted result. */
  bursts(other, N, 1400, 1.0, 99);
  for (int i = 0; i < N; i++) other[i] += 1e-4f;
  assert(dts_match_offset(m, N, 100.0, other, N, 100.0, 0.0, 10.0, &r) == 1);
  printf("unrelated: score %.2f margin %.2f confident %d\n", r.score, r.margin, r.confident);
  assert(!r.confident);
  /* Too little audio: no comparison at all. */
  assert(dts_match_offset(m, 500, 0, d, 500, 0, 0, 10, &r) == 0);
  /* Unrelated: none of its candidates passes the check either. */
  assert(dts_match_offset(m, N, 100.0, other, N, 100.0, 0.0, 10.0, &r) == 1);
  { DtsMatchSig sm = { { m, NULL, NULL, NULL }, N, 100.0 }, so = { { other, NULL, NULL, NULL }, N, 100.0 };
    for (int i = 0; i < r.ncand; i++) assert(!dts_match_verify(&sm, &so, 1.0, r.cand_off[i])); }
  /* A bed in continuous time, sampled on each file's own clock: for a PAL
   * version (sped up 25/23.976) and for a far offset (another cut). */
  {
    enum { EV = 4000 };
    static double at[EV], amp[EV], sp1[EV], sp2[EV];
    static float mm[DTS_MATCH_MAX], dd[DTS_MATCH_MAX], wide[140 * DTS_MATCH_HZ];
    seed = 7;
    for (int i = 0; i < EV; i++) { at[i] = rnd() * 2000.0; amp[i] = 0.3 + rnd(); sp1[i] = rnd() * 2000.0; sp2[i] = rnd() * 2000.0; }
    #define BED(t, list) bed_at((t), (list), amp, EV)
    const double k = 25.0 / (24000.0 / 1001.0), o = 3.25, t0m = 1200.0;
    for (int i = 0; i < DTS_MATCH_MAX; i++) {
      double t = t0m + i * 0.01;
      mm[i] = (float)(BED(t, at) + 0.6 * BED(t, sp1));
    }
    /* The other version's clock: d, with this file's time d * k + o. */
    double t0d = (t0m - o) / k - 5.0;
    for (int j = 0; j < DTS_MATCH_MAX; j++) {
      double dt = t0d + j * 0.01;
      dd[j] = (float)(BED(dt * k + o, at) + 0.6 * BED(dt, sp2));
    }
    DtsMatchSig sm = { { mm, NULL, NULL, NULL }, DTS_MATCH_MAX, t0m }, sd = { { dd, NULL, NULL, NULL }, DTS_MATCH_MAX, t0d };
    assert(dts_match_search(&sm, &sd, k, 0.0, 10.0, &r) == 1);
    printf("PAL version, ratio %.5f: offset %.3f s score %.2f margin %.2f confident %d\n", k, r.offset_s, r.score, r.margin, r.confident);
    assert(r.confident && fabs(r.offset_s - o) < 0.03);
    /* Taken at the same speed, the 4 % drift smears it over the window. */
    double local = t0m + 15.0 - (t0m + 15.0 - o) / k;
    assert(dts_match_search(&sm, &sd, 1.0, local, 10.0, &r) == 1);
    printf("PAL version at ratio 1: score %.2f margin %.2f confident %d\n", r.score, r.margin, r.confident);
    assert(!r.confident);
    /* Another cut: the other version is 47.3 s later, found in +-60 s. */
    const double far = -47.3;
    for (int j = 0; j < 140 * DTS_MATCH_HZ; j++) {
      double dt = t0m + 15.0 - far - 70.0 + j * 0.01;
      wide[j] = (float)(BED(dt + far, at) + 0.6 * BED(dt, sp2));
    }
    DtsMatchSig sw = { { wide, NULL, NULL, NULL }, 140 * DTS_MATCH_HZ, t0m + 15.0 - far - 70.0 };
    assert(dts_match_search(&sm, &sw, 1.0, 0.0, 60.0, &r) == 1);
    printf("another cut, 47.3 s: offset %.3f s score %.2f margin %.2f confident %d\n", r.offset_s, r.score, r.margin, r.confident);
    assert(r.confident && fabs(r.offset_s - far) < 0.03);
    /* Unrelated audio over +-60 s: still refused. */
    for (int j = 0; j < 140 * DTS_MATCH_HZ; j++) wide[j] = (float)BED(j * 0.01 + 500.0, sp2);
    assert(dts_match_search(&sm, &sw, 1.0, 0.0, 60.0, &r) == 1);
    printf("unrelated over +-60 s: score %.2f margin %.2f confident %d\n", r.score, r.margin, r.confident);
    assert(!r.confident);
  }
  /* Band envelopes: a 100 Hz tone lands in the low band, not the high one. */
  {
    static float b0[400], b1[400], b2[400], b3[400], tone[48000];
    float *env[DTS_MATCH_BANDS] = { b0, b1, b2, b3 };
    DtsMatchEnv v;
    for (int i = 0; i < 48000; i++) tone[i] = (float)sin(2 * M_PI * 100.0 * i / 48000.0);
    dts_match_env_init(&v, 48000, env, 400);
    dts_match_env_push(&v, tone, 48000);
    assert(v.n == 100 && v.env[1] && v.env[3]);
    printf("100 Hz tone: all %.3f low %.3f speech %.3f high %.5f\n", b0[50], b1[50], b2[50], b3[50]);
    assert(b1[50] > 0.3 && b3[50] < 0.001 && b2[50] < 0.1);
    dts_match_env_init(&v, 8000, env, 400);   /* 8 kHz: no high band */
    assert(v.env[1] && v.env[2] && !v.env[3]);
  }
  puts("dts_match: shared bed found at the right offset, PAL speed and far cuts found, unrelated audio refused");
  return 0;
}
