/* THE SUBTITLE CLOCK DURING TORBOX BURN-IN (owner 15/11/2026, log_18).
 *
 * TorBox's playlist says every segment is 5.000 s long; the real segments
 * alternate 6.006 / 4.004 s (first video PTS of segment k = 0.021 + 5.005 k,
 * +1.0 s on odd k). The TV player ties the first frame of the segment it lands
 * in to that segment's PLAYLIST start, so after a seek its clock runs behind
 * the burned picture by
 *
 *     PTS(k) - playlist start(k) - TorBox's first PTS
 *
 * (log_18: 146.167 - 145.000 - 0.021 = 1.146 s; subtitles ~1 s late, a manual
 * Delay of -1 s fixed them, and was wrong again once burn-in turned Off).
 * Nuvio draws its subtitles on the player's clock: this module says how much
 * to add to it while burn-in plays (0 otherwise).
 *
 * THE MODEL IS "LANDING" (owner 16/11, after the TV test with the three-way
 * test switch): the correction of the segment the player landed in, fixed
 * until the next open/seek. "Per segment" (re-tied at every segment) and
 * "Off" were both wrong on the TV and are gone with the switch.
 *
 * Fed by video_tpk.c (start/end, every open and seek of the burned stream) and
 * by whoever reads TorBox's segments: dts/tbaudio_tpk.c while Nuvio serves the
 * picture (passthrough), else video_tpk.c's own head reads. Thread-safe. Pure:
 * compiles everywhere (tests/tbrelogio.sh). */
#ifndef NUVIO_TBRELOGIO_H
#define NUVIO_TBRELOGIO_H

/* A burned stream starts (a new TorBox URL for the player): forgets the old
 * one. Returns its generation; calls carrying an older one are ignored. */
unsigned tbr_comecar(void);
unsigned tbr_geracao(void);
/* Burn-in ended (back to the original file): the correction is 0 again. */
void   tbr_encerrar(void);
int    tbr_ativo(void);

/* The segment lengths of the playlist the TV plays (EXTINF, in order). */
void   tbr_segmentos(unsigned ger, const double *dur, int n);
/* TorBox's first video PTS (segment 0). */
void   tbr_origem(unsigned ger, double pts0);
/* First video PTS of segment `seg`. */
void   tbr_pts(unsigned ger, int seg, double pts);
/* Nuvio served segment `seg` to the player (passthrough): checks the first one
 * after an open/seek against the segment the target falls in, and logs. */
void   tbr_servido(unsigned ger, int seg);

/* The TV player opens or seeks the burned stream at `alvo` (playlist time). */
void   tbr_ancorar(double alvo);

/* What the head reader should fetch next: 1 with *seg = a segment whose PTS
 * is needed (segment 0 first, for the origin, then the landing one), 0 nothing. */
int    tbr_falta(unsigned *ger, int *seg);

/* Seconds to ADD to the player's clock (playlist time) to get the time of the
 * burned picture: the segment the player landed in at the last open/seek sets
 * it (Landing). 0 when burn-in is off or the PTS it needs is not known yet. */
double tbr_correcao(void);

/* ---- pure ------------------------------------------------------------------ */
/* Segment of `t` in `n` segments of lengths `dur` (-1 when n == 0; the last
 * one past the end, 0 before the start). */
int    tbr_segmento_de(const double *dur, int n, double t, double *inicio);
#endif
