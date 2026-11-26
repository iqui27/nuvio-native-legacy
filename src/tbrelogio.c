// The subtitle clock during TorBox burn-in: see tbrelogio.h. Owner 15/11/2026.
#include "tbrelogio.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TBR_TETO_S 10.0   // a correction past this is bad data: none instead

static pthread_mutex_t M = PTHREAD_MUTEX_INITIALIZER;
static unsigned ger;                  // 0 = no burned stream yet
static int ativo;
static double *dur, *ini, *pts;       // lengths, playlist starts, first PTS (NAN = unknown)
static int n;
static double alvo;                   // the last open/seek target (playlist time)
static int conferir;                  // 1 = the next served segment is checked against `alvo`
static int pousoLog;                  // the landing correction was logged for this target
static int ruimLog;

int tbr_segmento_de(const double *d, int k, double t, double *inicio) {
  double a = 0;
  int i;
  if (!d || k <= 0) return -1;
  for (i = 0; i < k - 1; i++) {
    if (t < a + d[i]) break;
    a += d[i];
  }
  if (inicio) *inicio = a;
  return i;
}

static void limpar(void) {
  free(dur); free(ini); free(pts);
  dur = ini = pts = NULL; n = 0;
  alvo = 0; conferir = 0; pousoLog = 0; ruimLog = 0;
}

unsigned tbr_comecar(void) {
  unsigned g;
  pthread_mutex_lock(&M);
  limpar();
  g = ++ger ? ger : ++ger;   // never 0
  ativo = 1;
  conferir = 1;              // the player opens at 0 first
  pthread_mutex_unlock(&M);
  return g;
}
unsigned tbr_geracao(void) { unsigned g; pthread_mutex_lock(&M); g = ativo ? ger : 0; pthread_mutex_unlock(&M); return g; }
void tbr_encerrar(void) {
  pthread_mutex_lock(&M);
  if (ativo) { printf("[tbburn] subtitle clock: burn-in ended, no correction\n"); fflush(stdout); }
  ativo = 0;
  limpar();
  pthread_mutex_unlock(&M);
}
int tbr_ativo(void) { int v; pthread_mutex_lock(&M); v = ativo; pthread_mutex_unlock(&M); return v; }

static int valido(unsigned g) { return ativo && g == ger; }

// Correction of segment k (playlist start, first PTS and TorBox's first PTS
// known): 1 with *c.
static int correcaoDe(int k, double *c) {
  if (k < 0 || k >= n || !pts || isnan(pts[k]) || isnan(pts[0])) return 0;
  *c = pts[k] - ini[k] - pts[0];
  return 1;
}
static void logPouso(void) {
  double c, a = 0;
  int k;
  if (pousoLog || !n) return;
  k = tbr_segmento_de(dur, n, alvo, &a);
  if (!correcaoDe(k, &c)) return;
  pousoLog = 1;
  printf("[tbburn] subtitle clock: landed in segment %d at %.3f s (playlist start %.3f s, first PTS %.3f s, TorBox's first "
         "PTS %.3f s) -> subtitles %+.0f ms\n", k, alvo, a, pts[k], pts[0], c * 1000.0);
  fflush(stdout);
}

void tbr_segmentos(unsigned g, const double *d, int k) {
  int i;
  pthread_mutex_lock(&M);
  if (!valido(g) || k <= 0) { pthread_mutex_unlock(&M); return; }
  free(dur); free(ini); free(pts);
  dur = malloc(sizeof *dur * (size_t)k); ini = malloc(sizeof *ini * (size_t)k); pts = malloc(sizeof *pts * (size_t)k);
  if (!dur || !ini || !pts) { free(dur); free(ini); free(pts); dur = ini = pts = NULL; n = 0; pthread_mutex_unlock(&M); return; }
  for (i = 0; i < k; i++) { dur[i] = d[i]; ini[i] = i ? ini[i - 1] + dur[i - 1] : 0; pts[i] = NAN; }
  n = k;
  pthread_mutex_unlock(&M);
}
void tbr_pts(unsigned g, int seg, double p) {
  pthread_mutex_lock(&M);
  if (valido(g) && seg >= 0 && seg < n && isfinite(p)) { pts[seg] = p; logPouso(); }
  pthread_mutex_unlock(&M);
}
void tbr_origem(unsigned g, double p0) { tbr_pts(g, 0, p0); }

void tbr_servido(unsigned g, int seg) {
  pthread_mutex_lock(&M);
  if (valido(g) && conferir && n) {
    int k = tbr_segmento_de(dur, n, alvo, NULL);
    conferir = 0;
    printf("[tbburn] subtitle clock: first segment the player asked for after %.1f s: %d (%s %d)\n", alvo, seg,
           seg == k ? "as expected:" : "NOT the expected", k);
    fflush(stdout);
  }
  pthread_mutex_unlock(&M);
}

void tbr_ancorar(double a) {
  pthread_mutex_lock(&M);
  if (ativo && isfinite(a)) {
    alvo = a < 0 ? 0 : a; conferir = 1; pousoLog = 0;
    logPouso();
  }
  pthread_mutex_unlock(&M);
}

int tbr_falta(unsigned *g, int *seg) {
  int r = 0, k;
  pthread_mutex_lock(&M);
  if (ativo && n) {
    k = tbr_segmento_de(dur, n, alvo, NULL);
    if (isnan(pts[0])) { *seg = 0; r = 1; }
    else if (k >= 0 && isnan(pts[k])) { *seg = k; r = 1; }
    if (r) *g = ger;
  }
  pthread_mutex_unlock(&M);
  return r;
}

double tbr_correcao(void) {
  double c = 0;
  int k, ok;
  pthread_mutex_lock(&M);
  if (!ativo || !n) { pthread_mutex_unlock(&M); return 0; }
  k = tbr_segmento_de(dur, n, alvo, NULL);
  ok = correcaoDe(k, &c);
  if (ok && fabs(c) > TBR_TETO_S) {
    if (!ruimLog++) { printf("[tbburn] subtitle clock: correction %+.0f ms is not believable: none\n", c * 1000.0); fflush(stdout); }
    c = 0;
  }
  pthread_mutex_unlock(&M);
  return ok ? c : 0;
}
