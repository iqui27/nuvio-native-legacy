/* AUDIO FROM ANOTHER SOURCE: THE SUBTITLE MATCH, first level of the sync
 * (owner 17/11/2026).
 *
 * The donor's audio is placed against the picture by its subtitles first:
 * an embedded text subtitle track of the playing file and one in the same
 * language of the donor, both as their seek index lists them (the start of
 * every indexed block, doa_legendas: no byte more is downloaded), are aligned
 * by the subtitle sync engine of subtitle-rewrite (autosync_alinhar, the one
 * the add-on subtitles use, starts-only mode LEGENDA_DOC_FIM_ESTIMADO). The
 * result (file = k * donor + desloc) is applied at once; the audio match then
 * refines it inside +-1 s over ~40 s of playback (doa_dopasuj_fino).
 *
 * Runs on its own thread (the engine is bounded to 16 s of work); the main
 * thread polls doadorleg_resultado. */
#ifndef NV_DOADORLEG_H
#define NV_DOADORLEG_H
#include <stddef.h>
#include "dts/doador_tpk.h"

/* Starts for the session now playing (the donor's track plays): `pos` the
 * playhead (s, the file's time), for another cut the piece around it. */
void doadorleg_iniciar(double pos);
void doadorleg_cancelar(void);
/* 0 nothing yet / not started, 1 found (*k, *desloc), -1 not possible or not
 * sure (why in `motivo`). Each outcome is returned once. */
int  doadorleg_resultado(double *k, double *desloc, double *conf, char *motivo, size_t n);

/* Pure parts, for the test. The pairs worth trying (file track, donor track),
 * best first: same language (English first), then the same kind (SDH with
 * SDH, forced never), the line counts closest, SRT before ASS. Returns how
 * many (up to max). */
int  doadorleg_pares(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb,
                     int (*casa)(const char *, const char *), int *ia, int *ib, int max);
/* Pairs in two languages (forced never), best first; tried after the shared-
 * language pairs, with confidence 0.90 needed. */
int  doadorleg_pares_cruzados(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb, int *ia, int *ib, int max);
/* No line-count gate (owner 18/11): the two tracks' line counts (DoaLegInfo
 * falas, bursts merged) as smaller/larger, and the confidence a pair needs:
 * 0.90 in two languages or over 2x apart, else 0 (the engine's own bar). */
double doadorleg_razao(const DoaLegInfo *a, const DoaLegInfo *b);
double doadorleg_conf_min(const DoaLegInfo *a, const DoaLegInfo *b, int cruzado);
/* Start times -> a line from the two-point map. `donorDe(t)` = the donor's
 * time at the file's t. Fills k (file seconds per donor second) and desloc. */
int  doadorleg_reta(double t1, double d1, double t2, double d2, double *k, double *desloc);
/* The same with what the containers say: mesmaVel (one frame rate in both)
 * allows speed 1 only. The slope snaps to a real ratio (1, 24/23.976, 25/24,
 * 25/23.976 and their inverses) within 800 ppm, else 0; desloc at t1. */
int  doadorleg_reta_v(double t1, double d1, double t2, double d2, int mesmaVel, double *k, double *desloc);
/* One pair (start times of the file's track and of the donor's): 1 found. */
int  doadorleg_alinhar(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                       double pos, double *k, double *desloc, char *motivo, size_t mn, int (*cancelar)(void *), void *u);
int  doadorleg_alinhar_v(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                         double pos, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                         int (*cancelar)(void *), void *u);
/* Phrase starts: only the starts `gap` s after the one kept before (owner
 * 18/11: a fansub's ASS against CR's). Returns the count written to o. */
int  doadorleg_frases(const double *t, int n, double gap, double *o);
/* One pair as the match tries it: the plain match, then (an ASS side, or line
 * counts under 0.8 of each other) the phrase starts with confidence 0.90:
 * first in a window of +-5 min of the file around the playhead (`pos`), then,
 * only when semCorte (the files are as long), over the whole episode. */
int  doadorleg_alinhar_par(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                           double pos, double confMin, int mesmaVel, double razao, int semCorte, double *k, double *desloc,
                           double *conf, char *motivo, size_t mn, int (*cancelar)(void *), void *u);
/* THE EDIT MAP (owner 18/11; DoaMapa in dts/doador_tpk.h): once per donor,
 * after the first answer (kIni, dIni: its line), from the same pair. The whole
 * episode's pieces, else sliding phrase-start windows; each edit narrowed and
 * switched in the middle of its gap. Returns the stretches (1: the donor is in
 * one piece, nothing more to do; 0 refused, why in `motivo`: no stretch found,
 * or more than DOA_MAPA_MAX). Logs its time and every window. */
int  doadorleg_mapa(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                    int mesmaVel, double confMin, double kIni, double dIni, DoaMapa *out, char *motivo, size_t mn,
                    int (*cancelar)(void *), void *u);
/* The map the worker built after the first answer: 0 nothing (yet), 1 a map
 * (out->n >= 1), -1 none (why in `motivo`). Each outcome once. */
int  doadorleg_mapa_resultado(DoaMapa *out, char *motivo, size_t n);
/* The failed pairs' points for the log (doadorleg_pontos_ler reads them back:
 * the host replay, tests/doadorleg.c). */
void doadorleg_pontos_log(const char *quem, int k, const DoaLegInfo *l, const double *t, int n);
int  doadorleg_pontos_ler(const char *linha, char *quem, size_t nq, int *k, int *parte, double *t, int max, int *n);
#endif
