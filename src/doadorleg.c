// See doadorleg.h.
#include "doadorleg.h"
#include "autosync.h"
#include "legenda.h"
#include "linguas.h"
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DL_PARES   4        // pairs tried, best first
#define DL_MAXPTS  6000     // points per track (the engine takes up to 8000 cues)
#define DL_FIM_MAX 4.0      // an estimated end: the next start, at most this after its own
// Pairs in two different languages (owner 17/11: the match compares when lines
// start, and translations of one release mostly follow the same speech): after
// the shared-language ones, and only with this much confidence. The same bar
// for a pair whose line counts are over 2x apart (owner 18/11: no count gate).
#define DL_CONF_CRUZADO 0.90

static int casaPadrao(const char *a, const char *b) { return a[0] && b[0] && !strcasecmp(a, b); }
static int contem(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}
static int ehSdh(const DoaLegInfo *l) { return contem(l->nome, "sdh") || contem(l->nome, "hearing") || contem(l->nome, "cc"); }
static int ehAss(const DoaLegInfo *l) { return !strcmp(l->codec, "ass") || !strcmp(l->codec, "ssa"); }

// The lines a track has once its bursts are one (DoaLegInfo.falas; n when
// not counted).
static int falasDe(const DoaLegInfo *l) { return l->falas > 0 ? l->falas : l->n; }
static int ehIngles(const char *l) {
  return !strcasecmp(l, "en") || !strcasecmp(l, "eng") || !strncasecmp(l, "en-", 3) || !strncasecmp(l, "en_", 3);
}
double doadorleg_razao(const DoaLegInfo *a, const DoaLegInfo *b) {
  int x = falasDe(a), y = falasDe(b);
  if (x <= 0 || y <= 0) return 0;
  return x > y ? (double)y / x : (double)x / y;
}
// NO LINE-COUNT GATE (owner 18/11, log 4: a fansub's English ASS, 1368
// points, against a CR English ASS, 395, and a French ASS, 673, made "0
// pairs"): every pair of full tracks is tried, the closest line counts (after
// the bursts are merged) first; the match's own confidence decides, and a
// pair over 2x apart needs DL_CONF_LONGE (doadorleg_conf_min).
// ENGLISH FIRST (owner 18/11, "line up the English vs English subtitles and
// adjust from there"): an English pair before any other shared language.
int doadorleg_pares(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb,
                    int (*casa)(const char *, const char *), int *ia, int *ib, int max) {
  struct { int i, j; double p; } c[64], t;
  int n = 0, i, j, k;
  if (!casa) casa = casaPadrao;
  for (i = 0; i < na; i++)
    for (j = 0; j < nb && n < 64; j++) {
      double r;
      if (a[i].forcada || b[j].forcada) continue;
      // The same language (an undetermined one only with an undetermined one).
      if (a[i].lingua[0] || b[j].lingua[0]) {
        if (!a[i].lingua[0] || !b[j].lingua[0] || !(casa(a[i].lingua, b[j].lingua) || casa(b[j].lingua, a[i].lingua))) continue;
      }
      r = doadorleg_razao(&a[i], &b[j]);   // 1 = as many lines
      c[n].i = i; c[n].j = j;
      c[n].p = (ehIngles(a[i].lingua) || ehIngles(b[j].lingua) ? 100.0 : 0.0) + r * 10.0 +
               (ehSdh(&a[i]) == ehSdh(&b[j]) ? 5.0 : 0.0) + (!ehAss(&a[i]) && !ehAss(&b[j]) ? 2.0 : 0.0) +
               (!a[i].lingua[0] ? -3.0 : 0.0);
      n++;
    }
  for (i = 1; i < n; i++) for (k = i; k > 0 && c[k].p > c[k - 1].p; k--) { t = c[k]; c[k] = c[k - 1]; c[k - 1] = t; }
  for (i = 0; i < n && i < max; i++) { ia[i] = c[i].i; ib[i] = c[i].j; }
  return n < max ? n : max;
}

int doadorleg_pares_cruzados(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb, int *ia, int *ib, int max) {
  struct { int i, j; double p; } c[64], t;
  int n = 0, i, j, k;
  for (i = 0; i < na; i++)
    for (j = 0; j < nb && n < 64; j++) {
      double r;
      if (a[i].forcada || b[j].forcada) continue;
      r = doadorleg_razao(&a[i], &b[j]);
      c[n].i = i; c[n].j = j;
      // Closest line counts first; SDH with SDH; SRT before ASS (typeset ASS
      // splits a line into many starts); English on one side next.
      c[n].p = r * 10.0 + (ehSdh(&a[i]) == ehSdh(&b[j]) ? 3.0 : 0.0) + (!ehAss(&a[i]) && !ehAss(&b[j]) ? 2.0 : 0.0) +
               (ehIngles(a[i].lingua) || ehIngles(b[j].lingua) ? 1.0 : 0.0);
      n++;
    }
  for (i = 1; i < n; i++) for (k = i; k > 0 && c[k].p > c[k - 1].p; k--) { t = c[k]; c[k] = c[k - 1]; c[k - 1] = t; }
  for (i = 0; i < n && i < max; i++) { ia[i] = c[i].i; ib[i] = c[i].j; }
  return n < max ? n : max;
}
double doadorleg_conf_min(const DoaLegInfo *a, const DoaLegInfo *b, int cruzado) {
  return cruzado || doadorleg_razao(a, b) < 0.5 ? DL_CONF_CRUZADO : 0.0;
}

int doadorleg_reta(double t1, double d1, double t2, double d2, double *k, double *desloc) {
  return doadorleg_reta_v(t1, d1, t2, d2, 0, k, desloc);
}
// ONLY REAL SPEEDS (owner's log 9, Fight Club French, 18/11): two releases of
// one film run at the same speed or at a film/TV ratio, nothing in between.
// Two subtitle tracks written by different people drift a few hundred ms
// against each other, and the two-point line read that as +354 ppm: a 2 s
// drift by the end of the film, and a needless reopen at the "new speed".
// The measured slope snaps to the nearest real ratio (within 800 ppm) or the
// line is refused; with the same frame rate in both containers it is 1. The
// offset is taken at t1 (the playhead), where the viewer is.
int doadorleg_reta_v(double t1, double d1, double t2, double d2, int mesmaVel, double *k, double *desloc) {
  static const double CONH[] = { 1.0, 24000.0 / 23976.0, 25.0 / 24.0, 25000.0 / 23976.0,
                                 23976.0 / 24000.0, 24.0 / 25.0, 23976.0 / 25000.0 };
  double kk, melhor = 0, dist = 1e9;
  unsigned i;
  if (!isfinite(d1) || !isfinite(d2) || fabs(d2 - d1) < 30.0) return 0;
  kk = (t2 - t1) / (d2 - d1);
  if (kk < 0.95 || kk > 1.05) return 0;
  for (i = 0; i < (mesmaVel ? 1u : sizeof CONH / sizeof *CONH); i++)
    if (fabs(kk - CONH[i]) < dist) { dist = fabs(kk - CONH[i]); melhor = CONH[i]; }
  if (dist > (mesmaVel ? 0.003 : 0.0008)) return 0;
  if (dist > 0.00005)
    printf("[doadorleg] measured speed %.6f is no real ratio: %.6f used%s\n", kk, melhor, mesmaVel ? " (same frame rate in both files)" : "");
  *k = melhor;
  *desloc = t1 - melhor * d1;
  return 1;
}

// A track's start times as a timing-only document: each line runs to the next
// start (at most DL_FIM_MAX). ASS: a burst (karaoke, a typeset sign drawn in
// pieces) keeps its first point only.
static LegendaDocumento *documento(const double *t, int n, int ass, const char *lingua, const char *quem, double *span, double j0, double j1) {
  LegendaCue *v = calloc((size_t)n, sizeof *v);
  LegendaDocumentoInfo info;
  LegendaDocumento *d;
  int i, m = 0;
  if (!v) return NULL;
  for (i = 0; i < n; i++) {
    double ini = t[i], fim;
    if (ini < 0 || (m && ini - v[m - 1].inicio < (ass ? DOA_SURTO_ASS : DOA_SURTO_SRT))) continue;
    fim = i + 1 < n ? t[i + 1] - 0.05 : ini + 2.0;
    if (fim > ini + DL_FIM_MAX) fim = ini + DL_FIM_MAX;
    if (fim < ini + 0.3) fim = ini + 0.3;
    v[m].inicio = ini; v[m].fim = fim;
    snprintf(v[m].texto, sizeof v[m].texto, "line %05d", m);
    m++;
  }
  memset(&info, 0, sizeof info);
  snprintf(info.idioma, sizeof info.idioma, "%s", lingua);
  snprintf(info.origem, sizeof info.origem, "%s index", quem);
  snprintf(info.identidade, sizeof info.identidade, "doador:%s:%d", quem, m);
  info.flags = LEGENDA_DOC_COMPLETO | LEGENDA_DOC_FIM_ESTIMADO;
  // A window (the file's side only): the engine proves the offset inside it
  // (a cut elsewhere does not matter), speed 1, +-10 s.
  if (j1 > j0) { info.flags |= LEGENDA_DOC_JANELA; info.janelaInicio = j0; info.janelaFim = j1; }
  if (span) { span[0] = m ? v[0].inicio : 0; span[1] = m ? v[m - 1].inicio : 0; }
  d = m >= 20 ? legenda_documento_de_cues(v, m, &info) : NULL;
  free(v);
  return d;
}

typedef struct {
  int ia[DL_PARES], ib[DL_PARES], n, cruzado[DL_PARES], nCruz;
  DoaLegInfo a[DOA_LEG_MAX], b[DOA_LEG_MAX];
  double *ta[DL_PARES], *tb[DL_PARES]; int na[DL_PARES], nb[DL_PARES];
  double pos;
  int mesmaVel;          // both containers at one frame rate (doa_diferenca): speed 1
  int semCorte;          // the two files are as long (doa_diferenca's cut within 1 s): the whole-episode phrase pass may run
  unsigned ger;
} Pedido;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint geracao;
static int estado;               // 0 nothing, 1 working, 2 found, 3 not
static double rk, rd, rConf;
static char rMotivo[160];
static int mapaEstado;           // 0 nothing, 2 a map, 3 none (doadorleg_mapa_resultado)
static DoaMapa mapaPronto;
static char mapaMotivo[160];

static int cancelado(void *u) { return (unsigned)(uintptr_t)u != atomic_load(&geracao); }

static void terminar(unsigned ger, int achou, double k, double d, double conf, const char *motivo) {
  pthread_mutex_lock(&trava);
  if (ger == atomic_load(&geracao)) {
    estado = achou ? 2 : 3; rk = k; rd = d; rConf = conf;
    snprintf(rMotivo, sizeof rMotivo, "%s", motivo ? motivo : "");
  }
  pthread_mutex_unlock(&trava);
}

static int alinharMin(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double confMin, double j0, double j1, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                      int (*cancelar)(void *), void *u);
int doadorleg_alinhar(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double *k, double *desloc, char *motivo, size_t mn, int (*cancelar)(void *), void *u) {
  return alinharMin(ta, na, assA, la, tb, nb, assB, lb, pos, 0.0, 0, 0, 0, k, desloc, NULL, motivo, mn, cancelar, u);
}
int doadorleg_alinhar_v(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                        double pos, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                        int (*cancelar)(void *), void *u) {
  return alinharMin(ta, na, assA, la, tb, nb, assB, lb, pos, 0.0, 0, 0, mesmaVel, k, desloc, conf, motivo, mn, cancelar, u);
}
// The match itself. `quieto`: no log lines (the edit map's windows log one
// line each themselves). `guardar`: the engine's map of an ACCEPTED result
// is handed over (the caller frees it), with *res its result.
static int alinharTudo(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                       double pos, double confMin, double j0, double j1, int mesmaVel, double *k, double *desloc, double *conf,
                       char *motivo, size_t mn, int (*cancelar)(void *), void *u, int quieto, AutoSyncMapa **guardar,
                       AutoSyncResultado *res);
static int alinharMin(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double confMin, double j0, double j1, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                      int (*cancelar)(void *), void *u) {
  return alinharTudo(ta, na, assA, la, tb, nb, assB, lb, pos, confMin, j0, j1, mesmaVel, k, desloc, conf, motivo, mn, cancelar, u, 0, NULL, NULL);
}
static int alinharTudo(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                       double pos, double confMin, double j0, double j1, int mesmaVel, double *k, double *desloc, double *conf,
                       char *motivo, size_t mn, int (*cancelar)(void *), void *u, int quieto, AutoSyncMapa **guardar,
                       AutoSyncResultado *res) {
  double sa[2], sb[2];
  LegendaDocumento *ref = documento(ta, na, assA, la, "file", sa, j0, j1);
  LegendaDocumento *doc = documento(tb, nb, assB, lb, "donor", sb, 0, 0);
  AutoSyncConfig cfg = autosync_config(AUTOSYNC_THOROUGH);
  AutoSyncMapa *mapa = NULL;
  AutoSyncResultado r;
  struct timespec t0, t1;
  int achou = 0;
  if (!ref || !doc) {
    snprintf(motivo, mn, "too few lines");
    legenda_documento_liberar(ref); legenda_documento_liberar(doc);
    return 0;
  }
  clock_gettime(CLOCK_MONOTONIC, &t0);
  cfg.manterMs = 0;   // even a small offset counts: the audio refines from it
  r = autosync_alinhar(doc, ref, &cfg, cancelar, u, &mapa);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  if (res) *res = r;
  if (!quieto)
  printf("[doadorleg] file %d points (%.0f-%.0f s) / donor %d points (%.0f-%.0f s): %s (%s, offset %d ms, scale %.6f, pieces %d, "
         "confidence %.2f, coverage %.2f) in %.0f ms\n", na, sa[0], sa[1], nb, sb[0], sb[1],
         r.estado == AUTOSYNC_ACCEPTED ? "ACCEPTED" : "not accepted", autosync_motivo(r.motivo), r.offsetMs, r.escala, r.trechos,
         r.confianca, r.cobertura, (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6);
  if (r.estado == AUTOSYNC_ACCEPTED && r.confianca < confMin) {
    snprintf(motivo, mn, "confidence %.2f under the %.2f this pair needs", r.confianca, confMin);
    if (!quieto)
    printf("[doadorleg] confidence %.2f is under the %.2f this pair needs (two languages or line counts over 2x apart), not used\n", r.confianca, confMin);
  } else if (r.estado == AUTOSYNC_ACCEPTED) {
    // Two points of the file's time, from the playhead on (another cut: the
    // piece the viewer is in), mapped to the donor's.
    double t1s = pos > sa[0] + 60 ? pos : sa[0] + (sa[1] - sa[0]) * 0.25, t2s = t1s + 600.0, d1, d2;
    if (t2s > sa[1]) { t2s = t1s; t1s = t2s - 600.0; }
    if (t1s < sa[0]) t1s = sa[0];
    d1 = mapa ? autosync_mapa_tempo(mapa, t1s) : t1s + r.offsetMs / 1000.0;
    d2 = mapa ? autosync_mapa_tempo(mapa, t2s) : t2s + r.offsetMs / 1000.0;
    if (d1 > AUTOSYNC_SEM_LEGENDA / 2 && d2 > AUTOSYNC_SEM_LEGENDA / 2 && doadorleg_reta_v(t1s, d1, t2s, d2, mesmaVel, k, desloc)) achou = 1;
    else if (!mapa) { *k = 1.0; *desloc = -r.offsetMs / 1000.0; achou = 1; }
    else snprintf(motivo, mn, "accepted, but no real speed around %.0f s", pos);
    if (achou && conf) *conf = r.confianca;
    if (achou && !quieto)
      printf("[doadorleg] the file's %.1f s = the donor's %.3f s, %.1f s = %.3f s: file = %.6f x donor %+.3f s\n", t1s, d1, t2s, d2, *k, *desloc);
  } else snprintf(motivo, mn, "%s", autosync_motivo(r.motivo));
  fflush(stdout);
  // The edit map wants the engine's pieces even where the line around the
  // playhead could not be drawn (a gap at that point).
  if (guardar && r.estado == AUTOSYNC_ACCEPTED && r.confianca >= confMin) { *guardar = mapa; mapa = NULL; }
  autosync_mapa_liberar(mapa);
  legenda_documento_liberar(ref); legenda_documento_liberar(doc);
  return achou;
}

// PHRASE STARTS (owner 18/11, log 4: a fansub's ASS against CR's): when the
// two tracks were written apart (one splits a line in two, carries typeset
// signs or karaoke, the other joins lines), their starts disagree too often
// for the engine. What both still share is where a phrase starts after a
// pause: both tracks keep only starts DL_FRASE_S after the one kept before,
// and 0.90 confidence is needed.
// AROUND THE PLAYHEAD FIRST (owner 18/11, log 6: a CR release against a
// fansub refused as "region_disagreement", and the whole-episode phrase pass
// gave a wrong +4774 ms in two pieces that the 0.90 bar only just stopped):
// the file's side is a window of +-DL_JANELA_S around the playhead, so a cut
// elsewhere in the episode does not matter (the offset is needed where the
// viewer is; another cut is the follow's). Synthetic fansub/CR timelines with
// a cut: found 65-90 %, never wrong; the whole-episode pass was wrong in 3 of
// ~300 runs with a cut. It runs only when the lengths agree (semCorte).
#define DL_FRASE_S  2.5
#define DL_JANELA_S 300.0
#define DL_FRENTE_ANTES 30.0
#define DL_FRENTE_S 270.0
int doadorleg_frases(const double *t, int n, double gap, double *o) {
  int i, m = 0;
  double u = -1e18;
  for (i = 0; i < n; i++) {
    if (t[i] < 0 || t[i] - u < gap) continue;
    o[m++] = t[i]; u = t[i];
  }
  return m;
}
int doadorleg_alinhar_par(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                          double pos, double confMin, int mesmaVel, double razao, int semCorte, double *k, double *desloc,
                          double *conf, char *motivo, size_t mn, int (*cancelar)(void *), void *u) {
  double *fa, *fb, cm = confMin > DL_CONF_CRUZADO ? confMin : DL_CONF_CRUZADO;
  int achou = alinharMin(ta, na, assA, la, tb, nb, assB, lb, pos, confMin, 0, 0, mesmaVel, k, desloc, conf, motivo, mn, cancelar, u), ma, mb;
  if (achou || (cancelar && cancelar(u)) || !(assA || assB || razao < 0.8)) return achou;
  fa = malloc(sizeof *fa * (size_t)(na > 0 ? na : 1)); fb = malloc(sizeof *fb * (size_t)(nb > 0 ? nb : 1));
  if (fa && fb) {
    char m1[64], m2[64] = "", m3[64] = "";
    double j0, j1, ini = na ? ta[0] : 0, fim = na ? ta[na - 1] : 0;
    snprintf(m1, sizeof m1, "%s", motivo);
    ma = doadorleg_frases(ta, na, DL_FRASE_S, fa); mb = doadorleg_frases(tb, nb, DL_FRASE_S, fb);
    // The window: +-DL_JANELA_S around the playhead, inside the track, 2*DL_JANELA_S wide when it fits.
    j0 = pos - DL_JANELA_S; j1 = pos + DL_JANELA_S;
    if (j0 < ini) { j1 += ini - j0; j0 = ini; }
    if (j1 > fim) { j0 -= j1 - fim; j1 = fim; }
    if (j0 < ini) j0 = ini;
    printf("[doadorleg] phrase starts only (%.1f s apart) around the playhead (%.0f-%.0f s of the file): file %d / donor %d, confidence 0.90 needed\n",
           DL_FRASE_S, j0, j1, ma, mb);
    achou = j1 - j0 >= 120.0 &&
            alinharMin(fa, ma, 0, la, fb, mb, 0, lb, pos, cm, j0, j1, mesmaVel, k, desloc, conf, m2, sizeof m2, cancelar, u);
    // FROM THE PLAYHEAD ON (owner 18/11, log 13: a WEB release cut another
    // way, -7.0/-8.9/-7.9/-9.9 s by stretch; +-5 min held two of them and was
    // refused): only when +-5 min refuses, a window that starts DL_FRENTE_ANTES
    // before the playhead and runs DL_FRENTE_S on (what plays next). Log 13
    // replay: -9.914 s (0.92; the audio said -10.164, the refine closes it).
    // Synthetic CR/fansub with a cut: 11/12 -> 12/12 found, 0 wrong, unrelated
    // still refused; as the only window it found 8/12, so it stays second.
    if (!achou && j1 - j0 >= 120.0 && !(cancelar && cancelar(u))) {
      double f0 = pos - DL_FRENTE_ANTES, f1 = pos + DL_FRENTE_S;
      if (f0 < ini) f0 = ini;
      if (f1 > fim) f1 = fim;
      if (f1 - f0 >= 120.0 && (f0 > j0 + 30.0 || f1 < j1 - 30.0)) {
        char m4[64] = "";
        printf("[doadorleg] phrase starts from the playhead on (%.0f-%.0f s of the file): the +-5 min window held more than one offset\n",
               f0, f1);
        achou = alinharMin(fa, ma, 0, la, fb, mb, 0, lb, pos, cm, f0, f1, mesmaVel, k, desloc, conf, m4, sizeof m4, cancelar, u);
        if (!achou) snprintf(m2 + strlen(m2), sizeof m2 - strlen(m2), ", ahead: %.20s", m4);
      }
    }
    if (!achou && j1 - j0 < 120.0) snprintf(m2, sizeof m2, "the track is too short for a window");
    if (!achou && !(cancelar && cancelar(u))) {
      if (semCorte) {
        printf("[doadorleg] phrase starts over the whole episode (the two files are as long): confidence 0.90 needed\n");
        achou = alinharMin(fa, ma, 0, la, fb, mb, 0, lb, pos, cm, 0, 0, mesmaVel, k, desloc, conf, m3, sizeof m3, cancelar, u);
      } else snprintf(m3, sizeof m3, "not tried (the lengths differ: another cut)");
    }
    if (!achou) snprintf(motivo, mn, "%.40s; around the playhead: %.40s; whole: %.40s", m1, m2, m3);
  }
  free(fa); free(fb);
  return achou;
}

// ---- THE EDIT MAP (owner 18/11) --------------------------------------------
// "If file matches up good why would we need to rescan every time wasting
// cpu, better yet we can just scan the subs and know where the major
// differences are and adjust for them." Once per donor, after the first
// answer: where the two releases differ (an intro, recap, preview or eyecatch
// one of them lacks), each stretch gets its own offset. A donor in one piece
// does no further work.
//  a. The whole-episode match; ACCEPTED, its pieces (up to 4).
//  b. Refused, or 4 pieces (there may be more): phrase-start windows of
//     DL_MAPA_JAN_S every DL_MAPA_PASSO_S over the whole file, one offset
//     each (the engine's window mode, confidence 0.90). The donor's points are
//     moved by the first answer's offset first: the window mode searches
//     +-10 s around 0 only.
//  c. Between two stretches that differ, the edit is narrowed with 60 s and
//     then 20 s windows (the engine refuses windows under 80 s: these count
//     the file's phrase starts that land on a donor phrase start at one offset
//     or the other), then the last line of one and the first of the next: the
//     switch goes at the middle of that gap (owner: inside an OP or eyecatch
//     without lines, its middle; no audio check there).
//  More than DOA_MAPA_MAX stretches: refused (a wrong title or a broken pair).
#define DL_MAPA_JAN_S   240.0
#define DL_MAPA_PASSO_S 120.0
#define DL_JUNTA_S      0.3     // offsets this close are one stretch
#define DL_TOL_S        0.35    // a phrase start lands on the other's within this
typedef struct { double o, t0, t1; int n; } Corrida;   // offset, evidence from t0 to t1, windows/lines behind it

static int perto(const double *v, int n, double x, double tol) {
  int lo = 0, hi = n;
  while (lo < hi) { int m = (lo + hi) / 2; if (v[m] < x - tol) lo = m + 1; else hi = m; }
  return lo < n && v[lo] <= x + tol;
}
// The file's phrase starts in [t0, t1) that land on a donor phrase start at
// offset A and at B (file = donor + o).
static void contar(const double *fa, int ma, const double *fb, int mb, double oA, double oB, double t0, double t1,
                   int *n, int *hA, int *hB) {
  int i;
  *n = *hA = *hB = 0;
  for (i = 0; i < ma; i++) {
    if (fa[i] < t0 || fa[i] >= t1) continue;
    (*n)++;
    if (perto(fb, mb, fa[i] - oA, DL_TOL_S)) (*hA)++;
    if (perto(fb, mb, fa[i] - oB, DL_TOL_S)) (*hB)++;
  }
}
// BY LINE COUNT (owner's log 13: every engine window over 0-180 s was
// refused or ambiguous, 61-78 s there lines up elsewhere): where no engine
// window was accepted, the offset within +-DL_PICO_RAIO_S of the first answer
// that lands the most of the file's phrase starts on a donor phrase start.
// Taken only when clear: DL_PICO_MIN lines or more, DL_PICO_RAZAO times the
// best other offset (over 1 s away), and twice a neighbour stretch's offset.
#define DL_PICO_RAIO_S 15.0
#define DL_PICO_TOL_S  0.30
#define DL_PICO_MIN    10
#define DL_PICO_RAZAO  1.35
static int pico(const double *fa, int ma, const double *fb, int mb, double a, double b, double centro, double *o, int *h, int *h2) {
  double x, melhor = 0, soma = 0;
  int i, hm = 0, hs = 0, q = 0;
  for (x = centro - DL_PICO_RAIO_S; x <= centro + DL_PICO_RAIO_S + 1e-9; x += 0.05) {
    int c = 0;
    for (i = 0; i < ma; i++) if (fa[i] >= a && fa[i] < b && perto(fb, mb, fa[i] - x, DL_PICO_TOL_S)) c++;
    if (c > hm) { if (fabs(x - melhor) > 1.0) hs = hm; hm = c; melhor = x; }
    else if (c > hs && fabs(x - melhor) > 1.0) hs = c;
  }
  // The second best again, now that the best is known (it may have come first).
  hs = 0;
  for (x = centro - DL_PICO_RAIO_S; x <= centro + DL_PICO_RAIO_S + 1e-9; x += 0.05) {
    int c = 0;
    if (fabs(x - melhor) <= 1.0) continue;
    for (i = 0; i < ma; i++) if (fa[i] >= a && fa[i] < b && perto(fb, mb, fa[i] - x, DL_PICO_TOL_S)) c++;
    if (c > hs) hs = c;
  }
  // Centred on the lines it matched (the 0.05 s grid's plateau).
  for (i = 0; i < ma; i++) {
    int lo = 0, hi = mb;
    double y;
    if (fa[i] < a || fa[i] >= b) continue;
    y = fa[i] - melhor;
    while (lo < hi) { int m = (lo + hi) / 2; if (fb[m] < y - DL_PICO_TOL_S) lo = m + 1; else hi = m; }
    if (lo < mb && fb[lo] <= y + DL_PICO_TOL_S) { soma += fa[i] - fb[lo] - melhor; q++; }
  }
  *o = q ? melhor + soma / q : melhor; *h = hm; *h2 = hs;
  return hm;
}
// +1 a line that lands on the donor at A only, -1 at B only, 0 both or none.
static int rotulo(const double *fb, int mb, double t, double oA, double oB, double tol) {
  int x = perto(fb, mb, t - oA, tol), y = perto(fb, mb, t - oB, tol);
  return x && !y ? 1 : !x && y ? -1 : 0;
}
// One narrowing pass: windows of `len` every `passo` over [*r0, *r1), each
// scored (lines at A only) - (lines at B only); the split between two
// windows that most agrees (A before, B after) leaves the two windows around
// it. 1 narrowed.
static int estreitarPasso(const double *fa, int ma, const double *fb, int mb, double oA, double oB, double len, double passo,
                          double *r0, double *r1) {
  double w0[256], sc[256], tot = 0, acc = 0, melhor = -1e18;
  int nw = 0, k, i, kk = -1;
  double w;
  for (w = *r0; w < *r1 && nw < 256; w += passo) {
    double b = w + len > *r1 ? *r1 : w + len;
    int v = 0;
    for (i = 0; i < ma; i++) if (fa[i] >= w && fa[i] < b) v += rotulo(fb, mb, fa[i], oA, oB, DL_TOL_S);
    w0[nw] = w; sc[nw] = v; tot += v; nw++;
    if (b >= *r1) break;
  }
  if (nw < 3) return 0;
  for (k = 1; k < nw; k++) {
    double v;
    acc += sc[k - 1];
    v = acc - (tot - acc);
    if (v > melhor) { melhor = v; kk = k; }
  }
  if (kk < 1 || melhor <= 0) return 0;
  *r0 = w0[kk - 1]; *r1 = w0[kk] + len < *r1 ? w0[kk] + len : *r1;
  return 1;
}
// The split of points t[0..n) that most agrees (A-only before, B-only after):
// the index of the first point after it.
static int dividir(const double *t, int n, const double *fb, int mb, double oA, double oB, double tol) {
  int j, i, melhor = 0;
  double tot = 0, acc = 0, mv = -1e18;
  for (i = 0; i < n; i++) tot += rotulo(fb, mb, t[i], oA, oB, tol);
  for (j = 0; j <= n; j++) {
    double v = acc - (tot - acc);
    if (v > mv) { mv = v; melhor = j; }
    if (j < n) acc += rotulo(fb, mb, t[j], oA, oB, tol);
  }
  return melhor;
}
// Where the file goes from offset A to B, between the evidence of the two
// (r0..r1): 60 s windows, then 20 s, then the phrase starts, then every line
// (bursts merged) between the last phrase at A and the first at B. Returns the
// switch (the file's time, the middle of the gap between the last line at A
// and the first at B); *uA / *pB those two lines.
static double estreitar(const double *ta, int na, const double *tbAll, int nbAll, const double *fa, int ma, const double *fb, int mb,
                        double oA, double oB, double r0, double r1, double *uA, double *pB, int *passos) {
  double a = r0, b = r1, ult = -1, pri = -1, v[4096];
  int i, n = 0, d;
  *passos = 0;
  if (estreitarPasso(fa, ma, fb, mb, oA, oB, 60.0, 20.0, &a, &b)) (*passos)++;
  if (estreitarPasso(fa, ma, fb, mb, oA, oB, 20.0, 5.0, &a, &b)) (*passos)++;
  // The phrase starts, 10 s around what the windows left.
  for (i = 0; i < ma && n < 4096; i++) if (fa[i] >= a - 10.0 && fa[i] < b + 10.0) v[n++] = fa[i];
  d = dividir(v, n, fb, mb, oA, oB, DL_TOL_S);
  for (i = d - 1; i >= 0; i--) if (rotulo(fb, mb, v[i], oA, oB, DL_TOL_S) > 0) { ult = v[i]; break; }
  for (i = d; i < n; i++) if (rotulo(fb, mb, v[i], oA, oB, DL_TOL_S) < 0) { pri = v[i]; break; }
  // None on a side within the region (an OP or a preview without lines
  // there): the nearest one beyond it.
  if (ult < 0) for (i = ma - 1; i >= 0; i--) if (fa[i] < (d < n ? v[d] : b) && rotulo(fb, mb, fa[i], oA, oB, DL_TOL_S) > 0) { ult = fa[i]; break; }
  if (ult < 0) ult = d > 0 ? v[d - 1] : a;
  if (pri < 0) for (i = 0; i < ma; i++) if (fa[i] > ult && rotulo(fb, mb, fa[i], oA, oB, DL_TOL_S) < 0) { pri = fa[i]; break; }
  if (pri < 0 || pri <= ult) pri = d < n && v[d] > ult ? v[d] : (b > ult ? b : ult);
  // Close together (dialogue on both sides, no long gap): every line from a
  // little before the last phrase at A to a little after the first at B, the
  // last at A and the first at B against all the donor's lines (a phrase goes
  // on after its start; one stray phrase match moves the split no more).
  if (pri - ult <= 30.0) {
    double w[1024], u2 = -1, p2 = -1, x0 = ult - 20.0, x1 = pri + 8.0;
    int m = 0, e;
    for (i = 0; i < na && m < 1024; i++)
      if (ta[i] >= x0 && ta[i] <= x1 && (!m || ta[i] - w[m - 1] >= DOA_SURTO_ASS)) w[m++] = ta[i];
    if (m >= 2) {
      e = dividir(w, m, tbAll, nbAll, oA, oB, 0.25);
      for (i = e - 1; i >= 0; i--) if (rotulo(tbAll, nbAll, w[i], oA, oB, 0.25) > 0) { u2 = w[i]; break; }
      for (i = e; i < m; i++) if (rotulo(tbAll, nbAll, w[i], oA, oB, 0.25) < 0) { p2 = w[i]; break; }
      if (u2 >= 0 && p2 > u2) { ult = u2; pri = p2; }
    }
    *uA = ult; *pB = pri;
    return (ult + pri) / 2.0;
  }
  // A long gap (an OP, an eyecatch or a preview without lines on one side):
  // its middle, after the last line at A has run on (the file's points that
  // follow it closely, a phrase, at most 10 s).
  { double fim = ult;
    for (i = 0; i < na; i++) if (ta[i] > fim && ta[i] < pri && ta[i] - fim < DL_FRASE_S && ta[i] - ult < 10.0) fim = ta[i];
    if (fim > ult) fim += 1.0;
    if (fim > pri) fim = pri;
    *uA = ult; *pB = pri;
    return (fim + pri) / 2.0; }
}
static int cmpDouble(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double mediana(double *v, int n) {
  qsort(v, (size_t)n, sizeof *v, cmpDouble);
  return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}
// a. The engine's whole-episode map, sampled at the file's phrase starts:
// runs of one offset (a few stray lines inside a run are its anchors' noise).
static int corridasDoMapa(const AutoSyncMapa *mp, const double *fa, int ma, Corrida *c, int max) {
  double v[4096];
  int nc = 0, nv = 0, i, fora = 0;
  double ref = 0, ini = 0, ult = 0;
  for (i = 0; i <= ma; i++) {
    double o = 0, d;
    int ok = 0;
    if (i < ma) { d = autosync_mapa_tempo(mp, fa[i]); ok = d > AUTOSYNC_SEM_LEGENDA / 2; o = fa[i] - d; }
    if (i < ma && !ok) continue;
    if (i < ma && nv && fabs(o - ref) <= 0.5) { if (nv < 4096) v[nv++] = o; ult = fa[i]; fora = 0; continue; }
    if (i < ma && nv && ++fora < 3) continue;   // a stray line: the next ones decide
    if (nv >= 4 && nc < max) { c[nc].o = mediana(v, nv); c[nc].t0 = ini; c[nc].t1 = ult; c[nc].n = nv; nc++; }
    nv = 0; fora = 0;
    if (i < ma) { ref = o; ini = ult = fa[i]; v[nv++] = o; }
  }
  return nc;
}

int doadorleg_mapa(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                   int mesmaVel, double confMin, double kIni, double dIni, DoaMapa *out, char *motivo, size_t mn,
                   int (*cancelar)(void *), void *u) {
  struct timespec q0, q1;
  double *fa = NULL, *fb = NULL, *fbS = NULL, cm = confMin > DL_CONF_CRUZADO ? confMin : DL_CONF_CRUZADO, ini, fim;
  Corrida c[64];
  int nc = 0, ma, mb, i, viaJanelas = 0, pecas = 0;
  AutoSyncMapa *mp = NULL;
  AutoSyncResultado r;
  char m[96] = "";
  clock_gettime(CLOCK_MONOTONIC, &q0);
  memset(out, 0, sizeof *out);
  out->k = 1.0;
  motivo[0] = 0;
  if (fabs(kIni - 1.0) > 1e-9) {
    // Another speed: one line, as the first answer (the map is for cuts).
    out->n = 1; out->k = kIni; out->t[0].ini = -1e18; out->t[0].fim = 1e18; out->t[0].desloc = dIni;
    printf("[doadorleg] edit map: 1 stretch (another speed, %.6f: one line)\n", kIni);
    fflush(stdout);
    return 1;
  }
  fa = malloc(sizeof *fa * (size_t)(na > 0 ? na : 1)); fb = malloc(sizeof *fb * (size_t)(nb > 0 ? nb : 1));
  fbS = malloc(sizeof *fbS * (size_t)(nb > 0 ? nb : 1));
  if (!fa || !fb || !fbS || na < 20 || nb < 20) {
    snprintf(motivo, mn, "%s", !fa || !fb || !fbS ? "out of memory" : "too few lines");
    free(fa); free(fb); free(fbS);
    return 0;
  }
  ma = doadorleg_frases(ta, na, DL_FRASE_S, fa);
  mb = doadorleg_frases(tb, nb, DL_FRASE_S, fb);
  ini = ta[0]; fim = ta[na - 1];
  // a. The whole episode.
  memset(&r, 0, sizeof r);
  // (Its pieces are wanted even where no line can be drawn at one place: the
  // map comes back whenever the engine ACCEPTED.)
  alinharTudo(ta, na, assA, la, tb, nb, assB, lb, ini, confMin, 0, 0, mesmaVel, &(double){1}, &(double){0}, NULL, m, sizeof m,
              cancelar, u, 1, &mp, &r);
  if (mp) {
    pecas = r.trechos;
    if (r.tipo == AUTOSYNC_T_ESCALA || fabs(r.escala - 1.0) > 1e-6) {
      // Another speed over the whole episode: the first answer's line, one stretch.
      autosync_mapa_liberar(mp);
      printf("[doadorleg] edit map: 1 stretch (the whole episode at another speed, %.6f: the first answer's line)\n", r.escala);
      out->n = 1; out->t[0].ini = -1e18; out->t[0].fim = 1e18; out->t[0].desloc = dIni;
      fflush(stdout);
      free(fa); free(fb); free(fbS);
      return 1;
    } else {
      nc = corridasDoMapa(mp, fa, ma, c, 64);
      printf("[doadorleg] edit map: whole episode ACCEPTED (%d piece(s), confidence %.2f): %d run(s) of one offset\n", r.trechos, r.confianca, nc);
    }
  } else
    printf("[doadorleg] edit map: whole episode %s: sliding windows\n", m[0] ? m : "not accepted");
  autosync_mapa_liberar(mp);
  // b. Windows: the whole match refused, or held the most pieces it can.
  if (!(cancelar && cancelar(u)) && (nc == 0 || pecas >= 4)) {
    Corrida cj[64];
    int ncj = 0, nj = 0, aceitas = 0;
    double w, jo[64], jc[64], j0s[64], j1s[64];
    for (i = 0; i < mb; i++) fbS[i] = fb[i] + dIni;   // the donor moved onto the file's time by the first answer
    // Two sizes: DL_MAPA_JAN_S every DL_MAPA_PASSO_S over the whole file, then
    // half of that where none was accepted (a window holding an edit or an
    // OP is refused; a short stretch, 0-180 s in log 13, needs a smaller one).
    for (int tam = 0; tam < 2; tam++) {
      double L = tam ? DL_MAPA_JAN_S / 2 : DL_MAPA_JAN_S, P = tam ? DL_MAPA_PASSO_S / 2 : DL_MAPA_PASSO_S;
      int nAntes = nj;
      for (w = ini; nj < 64; w += P) {
        double a = w, b = w + L, k = 1, d = 0, cf = 0, cobre = 0;
        char mm[64] = "";
        int ok, q;
        if (b > fim) { b = fim; a = fim - L < ini ? ini : fim - L; }
        if (cancelar && cancelar(u)) break;
        // The second size only where no accepted window of the first covers
        // most of it.
        if (tam) {
          for (q = 0; q < nAntes; q++)
            if (!isnan(jo[q])) { double x0 = j0s[q] > a ? j0s[q] : a, x1 = j1s[q] < b ? j1s[q] : b; if (x1 - x0 > cobre) cobre = x1 - x0; }
          if (cobre > (b - a) * 0.75) { if (b >= fim) break; continue; }
        }
        ok = b - a >= 100.0 && alinharTudo(fa, ma, 0, la, fbS, mb, 0, lb, (a + b) / 2, cm, a, b, 1, &k, &d, &cf, mm, sizeof mm,
                                           cancelar, u, 1, NULL, NULL) && fabs(k - 1.0) < 1e-9;
        j0s[nj] = a; j1s[nj] = b; jo[nj] = ok ? d + dIni : NAN; jc[nj] = cf; nj++;
        if (ok) {
          aceitas++;
          printf("[doadorleg] map window %.0f-%.0f s: %+.3f s (confidence %.2f)\n", a, b, d + dIni, cf);
        } else printf("[doadorleg] map window %.0f-%.0f s: %s\n", a, b, mm[0] ? mm : "refused");
        if (b >= fim) break;
      }
    }
    // By line count where no engine window was accepted (DL_PICO_*): the
    // half-size grid again, each window not covered by an accepted one.
    { int nEng = nj;
      double vizinhos[64];
      int nv = 0;
      for (i = 0; i < nEng; i++) if (!isnan(jo[i]) && nv < 64) vizinhos[nv++] = jo[i];
      for (w = ini; nj < 64; w += DL_MAPA_PASSO_S / 2) {
        double a = w, b = w + DL_MAPA_JAN_S / 2, cobre = 0, o = 0, viz = 0;
        int h = 0, h2 = 0, n = 0, hv = 0, q, x, y;
        if (b > fim) { b = fim; a = fim - DL_MAPA_JAN_S / 2 < ini ? ini : fim - DL_MAPA_JAN_S / 2; }
        if (cancelar && cancelar(u)) break;
        for (q = 0; q < nEng; q++)
          if (!isnan(jo[q])) { double x0 = j0s[q] > a ? j0s[q] : a, x1 = j1s[q] < b ? j1s[q] : b; if (x1 - x0 > cobre) cobre = x1 - x0; }
        if (cobre > (b - a) * 0.75) { if (b >= fim) break; continue; }
        pico(fa, ma, fbS, mb, a, b, 0.0, &o, &h, &h2);
        o += dIni;
        for (q = 0; q < ma; q++) if (fa[q] >= a && fa[q] < b) n++;
        // The best known offset nearby (the engine's), on the same lines.
        for (q = 0; q < nv; q++) {
          int c = 0;
          contar(fa, ma, fb, mb, vizinhos[q], vizinhos[q], a, b, &x, &c, &y);
          if (c > hv) { hv = c; viz = vizinhos[q]; }
        }
        if (h >= DL_PICO_MIN && h >= DL_PICO_RAZAO * h2 && 10 * h >= 4 * n && (h >= 2 * hv || fabs(o - viz) <= DL_JUNTA_S)) {
          j0s[nj] = a; j1s[nj] = b; jo[nj] = o; jc[nj] = 0; nj++; aceitas++;
          if (nv) printf("[doadorleg] map window %.0f-%.0f s: %+.3f s by line count (%d of %d lines, next best %d, the nearest stretch's %+.2f: %d)\n",
                         a, b, o, h, n, h2, viz, hv);
          else printf("[doadorleg] map window %.0f-%.0f s: %+.3f s by line count (%d of %d lines, next best %d)\n", a, b, o, h, n, h2);
        } else if (h >= DL_PICO_MIN / 2)
          printf("[doadorleg] map window %.0f-%.0f s: line count not clear (%+.3f s: %d of %d lines, next best %d, the nearest stretch's: %d)\n",
                 a, b, o, h, n, h2, hv);
        else printf("[doadorleg] map window %.0f-%.0f s: too few lines for a line count (%d of %d)\n", a, b, h, n);
        if (b >= fim) break;
      } }
    // In the file's order (by their middles).
    for (i = 1; i < nj; i++)
      for (int q = i; q > 0 && j0s[q] + j1s[q] < j0s[q - 1] + j1s[q - 1]; q--) {
        double t;
        t = j0s[q]; j0s[q] = j0s[q - 1]; j0s[q - 1] = t; t = j1s[q]; j1s[q] = j1s[q - 1]; j1s[q - 1] = t;
        t = jo[q]; jo[q] = jo[q - 1]; jo[q - 1] = t; t = jc[q]; jc[q] = jc[q - 1]; jc[q - 1] = t;
      }
    // Runs: accepted windows in a row (refused ones skipped) within DL_JUNTA_S
    // of the run's first; its offset is the median of its windows'.
    for (i = 0; i < nj; i++) {
      double v[64];
      int k, q = 0;
      if (isnan(jo[i])) continue;
      if (!ncj || fabs(jo[i] - cj[ncj - 1].o) > DL_JUNTA_S) {
        if (ncj >= 64) break;
        cj[ncj].o = jo[i]; cj[ncj].t0 = j0s[i]; cj[ncj].t1 = j1s[i]; cj[ncj].n = 1; ncj++;
        continue;
      }
      cj[ncj - 1].t1 = j1s[i]; cj[ncj - 1].n++;
      // The engine's windows give the offset when the run has any (the line
      // count's is ~0.1 s coarser); the line count's only alone.
      for (int eng = 1; eng >= 0 && !q; eng--)
        for (k = 0; k <= i; k++)
          if (!isnan(jo[k]) && (jc[k] > 0) == eng && j0s[k] >= cj[ncj - 1].t0 && fabs(jo[k] - cj[ncj - 1].o) <= DL_JUNTA_S && q < 64)
            v[q++] = jo[k];
      if (q) cj[ncj - 1].o = mediana(v, q);
    }
    if (ncj > 0 && (nc == 0 || ncj > nc)) { memcpy(c, cj, sizeof(Corrida) * (size_t)ncj); nc = ncj; viaJanelas = 1; }
    if (!aceitas && !nc) snprintf(motivo, mn, "no window accepted (%d tried)", nj);
  }
  // Neighbours within DL_JUNTA_S are one stretch. A stretch whose own lines
  // land more often at a neighbour's offset than at its own is not one (a
  // window over an edit can be accepted at an offset between the two, 0.94
  // in a synthetic test): dropped, and its neighbours may join.
  for (int mudou = 1; mudou && nc > 0;) {
    mudou = 0;
    for (i = 1; i < nc; i++)
      if (fabs(c[i].o - c[i - 1].o) <= DL_JUNTA_S) {
        c[i - 1].o = (c[i - 1].o * c[i - 1].n + c[i].o * c[i].n) / (c[i - 1].n + c[i].n);
        c[i - 1].t1 = c[i].t1; c[i - 1].n += c[i].n;
        memmove(&c[i], &c[i + 1], sizeof *c * (size_t)(nc - i - 1)); nc--; i--; mudou = 1;
      }
    for (i = 0; i < nc && nc > 1; i++) {
      int proprio = 0, viz = 0, x, y, h, j;
      double ov = 0;
      contar(fa, ma, tb, nb, c[i].o, c[i].o, c[i].t0, c[i].t1, &x, &proprio, &y);
      for (j = i - 1; j <= i + 1; j += 2) {
        if (j < 0 || j >= nc) continue;
        contar(fa, ma, tb, nb, c[j].o, c[j].o, c[i].t0, c[i].t1, &x, &h, &y);
        if (h > viz) { viz = h; ov = c[j].o; }
      }
      if (viz >= proprio) {
        printf("[doadorleg] edit map: the stretch at %.0f-%.0f s (%+.2f s, %d window(s)) dropped: its lines land at %+.2f (%d) as much as at its own (%d)\n",
               c[i].t0, c[i].t1, c[i].o, c[i].n, ov, viz, proprio);
        memmove(&c[i], &c[i + 1], sizeof *c * (size_t)(nc - i - 1)); nc--; i--; mudou = 1;
      }
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &q1);
  if (!nc) {
    if (!motivo[0]) snprintf(motivo, mn, "no stretch found");
    printf("[doadorleg] edit map: refused in %.0f ms (%s): the playhead's offset only\n",
           (q1.tv_sec - q0.tv_sec) * 1000.0 + (q1.tv_nsec - q0.tv_nsec) / 1e6, motivo);
  } else if (nc > DOA_MAPA_MAX) {
    snprintf(motivo, mn, "more than %d stretches (%d): a wrong title or a broken pair", DOA_MAPA_MAX, nc);
    printf("[doadorleg] edit map: refused in %.0f ms (%s): the playhead's offset only\n",
           (q1.tv_sec - q0.tv_sec) * 1000.0 + (q1.tv_nsec - q0.tv_nsec) / 1e6, motivo);
    nc = 0;
  } else {
    char lin[1024];
    size_t o = 0;
    out->n = nc; out->k = 1.0;
    for (i = 0; i < nc; i++) { out->t[i].desloc = c[i].o; out->t[i].ini = -1e18; out->t[i].fim = 1e18; }
    // c. Each edit narrowed; the switch in the middle of its gap.
    for (i = 1; i < nc; i++) {
      double r0, r1, uA, pB, sw;
      int ps;
      if (viaJanelas) { r0 = c[i - 1].t1 - DL_MAPA_JAN_S; r1 = c[i].t0 + DL_MAPA_JAN_S; }
      else { r0 = c[i - 1].t1 - 60.0; r1 = c[i].t0 + 60.0; }
      if (r0 < c[i - 1].t0) r0 = c[i - 1].t0;
      if (r1 > c[i].t1) r1 = c[i].t1;
      if (r1 <= r0) { double t = r0; r0 = r1; r1 = t; }
      sw = estreitar(ta, na, tb, nb, fa, ma, fb, mb, c[i - 1].o, c[i].o, r0, r1, &uA, &pB, &ps);
      if (sw <= (i > 1 ? out->t[i - 1].ini : -1e17)) sw = (r0 + r1) / 2;
      printf("[doadorleg] edit %d: %+.2f -> %+.2f s between %.0f and %.0f s, %s: last line %.1f s, next %.1f s: switch at %.1f s\n", i,
             c[i - 1].o, c[i].o, r0, r1, ps == 2 ? "narrowed by 60 s then 20 s windows" : ps == 1 ? "narrowed by 60 s windows" : "windows did not narrow it",
             uA, pB, sw);
      out->t[i - 1].fim = sw; out->t[i].ini = sw;
    }
    for (i = 0; i < nc && o < sizeof lin - 48; i++)
      o += (size_t)snprintf(lin + o, sizeof lin - o, "%s%.0f-%.0f s %+.2f", i ? ", " : "", i ? out->t[i].ini : 0.0,
                            i + 1 < nc ? out->t[i].fim : fim, out->t[i].desloc);
    clock_gettime(CLOCK_MONOTONIC, &q1);
    printf("[doadorleg] edit map: %d stretch%s in %.0f ms%s: %s\n", nc, nc == 1 ? "" : "es",
           (q1.tv_sec - q0.tv_sec) * 1000.0 + (q1.tv_nsec - q0.tv_nsec) / 1e6,
           viaJanelas ? " (windows)" : " (whole episode)", lin);
  }
  fflush(stdout);
  free(fa); free(fb); free(fbS);
  return nc;
}

// THE POINTS OF A FAILED PAIR (owner 18/11): both tracks' index starts in the
// log, so a real pair can be replayed on the host (tests/doadorleg.c reads
// them back). Centiseconds, the first absolute, then deltas, base 36, "."
// between; DL_PONTOS_LINHA values a line.
#define DL_PONTOS_LINHA 240
void doadorleg_pontos_log(const char *quem, int k, const DoaLegInfo *l, const double *t, int n) {
  static const char dig[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  int partes = (n + DL_PONTOS_LINHA - 1) / DL_PONTOS_LINHA, pt, i;
  long ant = 0;
  printf("[doadorleg] points %s %d: %s %s \"%.30s\" n=%d\n", quem, k, l && l->lingua[0] ? l->lingua : "und", l ? l->codec : "?",
         l ? l->nome : "", n);
  for (pt = 0; pt < partes; pt++) {
    char b[DL_PONTOS_LINHA * 8 + 1];
    size_t o = 0;
    for (i = pt * DL_PONTOS_LINHA; i < n && i < (pt + 1) * DL_PONTOS_LINHA; i++) {
      long c = lround(t[i] * 100.0), d = c - ant;
      char tmp[16];
      int m = 0;
      ant = c;
      if (d < 0) { b[o++] = '-'; d = -d; }
      do { tmp[m++] = dig[d % 36]; d /= 36; } while (d && m < 15);
      while (m) b[o++] = tmp[--m];
      b[o++] = '.';
    }
    if (o) o--;
    b[o] = 0;
    printf("[doadorleg] points %s %d part %d/%d: %s\n", quem, k, pt + 1, partes, b);
  }
  fflush(stdout);
}
// One "points ... part" line back: 1 read (*n points added after the *n already in t).
int doadorleg_pontos_ler(const char *linha, char *quem, size_t nq, int *k, int *parte, double *t, int max, int *n) {
  const char *p = strstr(linha, "[doadorleg] points ");
  char q[16];
  int kk, pp, tot, usado = 0;
  long c = 0;
  if (!p || sscanf(p, "[doadorleg] points %15s %d part %d/%d: %n", q, &kk, &pp, &tot, &usado) < 4 || !usado) return 0;
  p += usado;
  if (pp == 1) *n = 0;
  else if (*n > 0) c = lround(t[*n - 1] * 100.0);
  while (*p && *p != '\n' && *n < max) {
    long d = 0;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p && *p != '.' && *p != '\n') { d = d * 36 + (*p <= '9' ? *p - '0' : *p - 'a' + 10); p++; }
    c += neg ? -d : d;
    t[(*n)++] = c / 100.0;
    if (*p == '.') p++;
  }
  if (quem && nq) snprintf(quem, nq, "%s", q);
  if (k) *k = kk;
  if (parte) *parte = pp;
  return 1;
}

static void *fio(void *u) {
  Pedido *p = u;
  int i, achou = 0;
  double k = 1, d = 0, conf = 0;
  char motivo[160] = "no pair of subtitle tracks in the same language";
  for (i = 0; i < p->n && !achou && !cancelado((void *)(uintptr_t)p->ger); i++) {
    const DoaLegInfo *a = &p->a[p->ia[i]], *b = &p->b[p->ib[i]];
    char m[120] = "";
    double cm = doadorleg_conf_min(a, b, p->cruzado[i]);
    printf("[doadorleg] pair %d%s: file %s %s \"%.30s\" (%d lines) / donor %s %s \"%.30s\" (%d lines)%s\n", i + 1,
           p->cruzado[i] ? " (two languages)" : "", a->lingua[0] ? a->lingua : "und", a->codec, a->nome, a->falas,
           b->lingua[0] ? b->lingua : "und", b->codec, b->nome, b->falas,
           cm > 0 && !p->cruzado[i] ? " (line counts over 2x apart: confidence 0.90 needed)" : "");
    achou = doadorleg_alinhar_par(p->ta[i], p->na[i], ehAss(a), a->lingua, p->tb[i], p->nb[i], ehAss(b), b->lingua, p->pos,
                                  cm, p->mesmaVel, doadorleg_razao(a, b), p->semCorte, &k, &d, &conf, m, sizeof m, cancelado,
                                  (void *)(uintptr_t)p->ger);
    if (!achou) snprintf(motivo, sizeof motivo, "pair %d (%s %s / %s %s): %s", i + 1, a->lingua, a->codec, b->lingua, b->codec, m);
  }
  if (!achou && !cancelado((void *)(uintptr_t)p->ger)) {
    // For the host replay: where the viewer was, the lengths, every pair's tracks once.
    int j, q, ja;
    printf("[doadorleg] points: at %.1f s, same frame rate %d, as long %d, %d pair(s)\n", p->pos, p->mesmaVel, p->semCorte, p->n);
    for (j = 0; j < p->n; j++) {
      for (q = 0, ja = 0; q < j; q++) if (p->ia[q] == p->ia[j]) ja = 1;
      if (!ja) doadorleg_pontos_log("file", p->ia[j], &p->a[p->ia[j]], p->ta[j], p->na[j]);
      for (q = 0, ja = 0; q < j; q++) if (p->ib[q] == p->ib[j]) ja = 1;
      if (!ja) doadorleg_pontos_log("donor", p->ib[j], &p->b[p->ib[j]], p->tb[j], p->nb[j]);
    }
  }
  terminar(p->ger, achou, k, d, conf, achou ? "" : motivo);
  // THE EDIT MAP (owner 18/11): from the pair that answered, right after the
  // answer (which is the offset at the playhead while this runs). Its points
  // go to the log first, so any map can be replayed on the host.
  if (achou && !cancelado((void *)(uintptr_t)p->ger)) {
    int j = i - 1, n;
    const DoaLegInfo *a = &p->a[p->ia[j]], *b = &p->b[p->ib[j]];
    DoaMapa mp;
    char m[160] = "";
    printf("[doadorleg] points: at %.1f s, same frame rate %d, as long %d, 1 pair(s) (the edit map's)\n", p->pos, p->mesmaVel, p->semCorte);
    doadorleg_pontos_log("file", p->ia[j], a, p->ta[j], p->na[j]);
    doadorleg_pontos_log("donor", p->ib[j], b, p->tb[j], p->nb[j]);
    n = doadorleg_mapa(p->ta[j], p->na[j], ehAss(a), a->lingua, p->tb[j], p->nb[j], ehAss(b), b->lingua, p->mesmaVel,
                       doadorleg_conf_min(a, b, p->cruzado[j]), k, d, &mp, m, sizeof m, cancelado, (void *)(uintptr_t)p->ger);
    pthread_mutex_lock(&trava);
    if (p->ger == atomic_load(&geracao)) {
      mapaEstado = n > 0 ? 2 : 3; mapaPronto = mp;
      snprintf(mapaMotivo, sizeof mapaMotivo, "%s", n > 0 ? "" : m);
    }
    pthread_mutex_unlock(&trava);
  } else {
    pthread_mutex_lock(&trava);
    if (p->ger == atomic_load(&geracao)) { mapaEstado = 3; snprintf(mapaMotivo, sizeof mapaMotivo, "no subtitle answer"); }
    pthread_mutex_unlock(&trava);
  }
  for (i = 0; i < p->n; i++) { free(p->ta[i]); free(p->tb[i]); }
  free(p);
  return NULL;
}
int doadorleg_mapa_resultado(DoaMapa *out, char *motivo, size_t n) {
  int v = 0;
  pthread_mutex_lock(&trava);
  if (mapaEstado == 2 || mapaEstado == 3) {
    v = mapaEstado == 2 ? 1 : -1;
    if (out) *out = mapaPronto;
    if (motivo && n) snprintf(motivo, n, "%s", mapaMotivo);
    mapaEstado = 0;
  }
  pthread_mutex_unlock(&trava);
  return v;
}

void doadorleg_iniciar(double pos) {
  Pedido *p = calloc(1, sizeof *p);
  int na, nb, i;
  pthread_t t;
  unsigned ger = atomic_fetch_add(&geracao, 1) + 1;
  pthread_mutex_lock(&trava); estado = 1; rMotivo[0] = 0; mapaEstado = 0; pthread_mutex_unlock(&trava);
  if (!p) { terminar(ger, 0, 1, 0, 0, "out of memory"); return; }
  p->ger = ger; p->pos = pos;
  { int corte = 0, vel = 1, ppm = 0;
    int ok = doa_diferenca(&corte, &vel, &ppm);
    p->mesmaVel = ok && !vel; p->semCorte = ok && abs(corte) <= 1; }
  na = doa_legendas(0, p->a, DOA_LEG_MAX);
  nb = doa_legendas(1, p->b, DOA_LEG_MAX);
  // Shared-language pairs first (English first), then pairs in two
  // languages fill what is left of DL_PARES.
  p->n = doadorleg_pares(p->a, na, p->b, nb, ling_casa, p->ia, p->ib, DL_PARES);
  for (i = 0; i < p->n; i++) p->cruzado[i] = 0;
  if (p->n < DL_PARES && na && nb) {
    int ca[DL_PARES * 4], cb[DL_PARES * 4], nc = doadorleg_pares_cruzados(p->a, na, p->b, nb, ca, cb, DL_PARES * 4), j, q;
    for (j = 0; j < nc && p->n < DL_PARES; j++) {
      int ja = 0;
      if (p->a[ca[j]].lingua[0] && p->b[cb[j]].lingua[0] &&
          (ling_casa(p->a[ca[j]].lingua, p->b[cb[j]].lingua) || ling_casa(p->b[cb[j]].lingua, p->a[ca[j]].lingua))) continue;
      for (q = 0; q < p->n; q++) if (p->ia[q] == ca[j] && p->ib[q] == cb[j]) ja = 1;
      if (ja) continue;
      p->ia[p->n] = ca[j]; p->ib[p->n] = cb[j]; p->cruzado[p->n++] = 1; p->nCruz++;
    }
  }
  printf("[doadorleg] subtitle match: the file has %d indexed text track(s), the donor %d: %d pair(s) to try%s\n", na, nb, p->n,
         p->nCruz ? " (pairs in two languages last, confidence 0.90 needed)" : "");
  fflush(stdout);
  for (i = 0; i < p->n; i++) {
    p->ta[i] = malloc(sizeof(double) * DL_MAXPTS); p->tb[i] = malloc(sizeof(double) * DL_MAXPTS);
    p->na[i] = p->ta[i] ? doa_legenda_tempos(0, p->ia[i], p->ta[i], DL_MAXPTS) : 0;
    p->nb[i] = p->tb[i] ? doa_legenda_tempos(1, p->ib[i], p->tb[i], DL_MAXPTS) : 0;
  }
  if (!p->n) {
    terminar(ger, 0, 1, 0, 0, na && nb ? "no pair of full subtitle tracks (only forced or sign tracks)" : !na ? "the file's subtitles are not in its index"
                                                                         : "the donor's subtitles are not in its index");
    free(p);
    return;
  }
  if (pthread_create(&t, NULL, fio, p) != 0) {
    terminar(ger, 0, 1, 0, 0, "no thread");
    for (i = 0; i < p->n; i++) { free(p->ta[i]); free(p->tb[i]); }
    free(p);
    return;
  }
  pthread_detach(t);
}
void doadorleg_cancelar(void) {
  atomic_fetch_add(&geracao, 1);
  pthread_mutex_lock(&trava); estado = 0; mapaEstado = 0; pthread_mutex_unlock(&trava);
}
int doadorleg_resultado(double *k, double *desloc, double *conf, char *motivo, size_t n) {
  int v = 0;
  pthread_mutex_lock(&trava);
  if (estado == 2 || estado == 3) {
    v = estado == 2 ? 1 : -1;
    if (k) *k = rk;
    if (desloc) *desloc = rd;
    if (conf) *conf = rConf;
    if (motivo && n) snprintf(motivo, n, "%s", rMotivo);
    estado = 0;
  }
  pthread_mutex_unlock(&trava);
  return v;
}
