/* AUTOMATIC SYNC OF ANOTHER VERSION'S AUDIO (owner, 07/11: "Match
 * automatically", on click only). Pure part: no FFmpeg, no threads.
 *
 * A dub or a voice-over keeps the original's music and effects; only the
 * speech differs. Both audios become energy envelopes (one value per 10 ms)
 * in four bands (all, low, speech, high: the low and high bands carry the
 * music and effects a dub keeps), then onset curves (rises of the log
 * energy, which hits, cuts and music share), and the offset is the lag where
 * the curves correlate best, searched around a centre. The result is only
 * taken when its peak is clearly above the next best one: otherwise the sync
 * stays manual.
 *
 * Mapping: this file's time = other version's time * ratio + offset. ratio 1
 * is the same speed; 25/23.976 a PAL version (sped up 4 %). */
#ifndef NV_DTS_MATCH_H
#define NV_DTS_MATCH_H
#define DTS_MATCH_HZ 100        /* envelope values per second */
#define DTS_MATCH_SECONDS 40    /* captured from each audio while playing */
#define DTS_MATCH_WIN_S 20      /* compared: the middle of this file's capture */
#define DTS_MATCH_CAND 3        /* best places kept for the person to try */
#define DTS_MATCH_MAX (DTS_MATCH_HZ * DTS_MATCH_SECONDS)
#define DTS_MATCH_BANDS 4       /* all, < 200 Hz, 300-3000 Hz, > 4 kHz */

typedef struct {
  double offset_s;   /* other version's timestamps * ratio + offset = this file's */
  double score;      /* correlation at the peak, -1..1 */
  double margin;     /* peak minus the best value outside +-250 ms */
  int confident;     /* 1: apply it */
  int ncand;         /* the best separate peaks (>= 0.5 s apart), best first */
  double cand_off[DTS_MATCH_CAND], cand_score[DTS_MATCH_CAND];
} DtsMatchResult;

/* One capture: env[b] = energies per 10 ms of band b (NULL: band absent),
 * block 0 at t0 seconds in its own file's timeline. */
typedef struct {
  const float *env[DTS_MATCH_BANDS];
  int n;
  double t0;
} DtsMatchSig;

/* Band envelopes from mono samples (biquad filters), for the capture. */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } DtsBiquad;
typedef struct {
  DtsBiquad f[DTS_MATCH_BANDS][2];
  int blk, accn, n, max;
  double acc[DTS_MATCH_BANDS];
  float *env[DTS_MATCH_BANDS];
} DtsMatchEnv;

/* dts_match.inc (static, included where used):
 * void dts_match_env_init(DtsMatchEnv *, int rate, float *const env[], int max)
 * void dts_match_env_push(DtsMatchEnv *, const float *mono, int n)
 * int dts_match_search(main, other, ratio, center_s, radius_s, out)
 *   searches center_s +- radius_s; 0 when it could not compare (too little
 *   audio, or no overlap), 1 with a result (confident or not).
 * int dts_match_offset(main_env, nm, t0m, ext_env, nd, t0d, center_s, radius_s, out)
 *   one band, same speed (the first version's interface). */
#endif
