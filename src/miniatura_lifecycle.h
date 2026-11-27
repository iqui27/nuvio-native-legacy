#ifndef MINIATURA_LIFECYCLE_H
#define MINIATURA_LIFECYCLE_H

#include <stdint.h>

typedef struct { uint32_t pausedSince; int waiting, attempted; } MiniPauseAdmission;

// Refining an existing hardware session allocates no additional source decoder.
// Keep the normal floor; retain extra headroom when hardware is absent.
static inline int mini_refine_memory_ok(long availableKB, long floorKB, int ready) {
  return availableKB < 0 || availableKB >= floorKB + (ready ? 0 : 100L * 1024);
}

// A scrub/buffer transition is never a creation window. One attempt per genuine
// pause: failures/releases cannot cause a retry loop while playback runs or rests.
static inline int mini_pause_admit(MiniPauseAdmission *p, uint32_t now,
                                  int confirmed, int scrubbing, int buffering) {
  if (!confirmed || scrubbing || buffering) {
    p->waiting = 0;
    if (!confirmed && !scrubbing && !buffering) p->attempted = 0;
    return 0;
  }
  if (!p->waiting) { p->pausedSince = now; p->waiting = 1; }
  if (p->attempted || (uint32_t)(now - p->pausedSince) < 2000u) return 0;
  p->attempted = 1;
  return 1;
}

// ---- Screen check decisions (pure, tested in tests/miniatura_lifecycle.c) ----
// 09/11/2026: the same copy measured -0.45..+2.34 s across nine checks. A best sample on the window's EDGE was counted
// as a match and moved the next window, so votes walked together from a bad start and "agreed". A vote now needs a peak
// bracketed by lower samples at distinct decoded times; an edge only says where to look next.
enum { MINI_PICO_FRACO = 0, MINI_PICO_OK = 1, MINI_PICO_AMBIGUO = 2, MINI_PICO_BORDA_ANTES = 3, MINI_PICO_BORDA_DEPOIS = 4 };
// Outcomes of a check. Only ACEITA and REJEITADA say anything about the copy or its add-on.
enum { MINI_TELA_INCONCLUSIVA = -2, MINI_TELA_SEM_QUADROS = -1, MINI_TELA_REJEITADA = 0, MINI_TELA_ACEITA = 1 };
#define MINI_PICO_PROEMINENCIA 0.02
#define MINI_PICO_BORDA_FOLGA 0.15    // an edge this far under the match level is a miss, not a trail worth following
#define MINI_PICO_CONTRASTE 0.10      // peak over the window's lowest sample: a still shot scores high at every time
// Wide searches that found nothing similar before a copy counts as wrong. They only see keyframes, so a shot with no
// keyframe finds nothing on a correct copy (09/11/2026, Se7en: three in a row at r=0.24, then r=0.83 at exact frames).
#define MINI_LARGAS_REJEITA 6

// Keep samples ascending by decoded time; a second sample of the same decoded frame adds no evidence. Returns the count.
static inline int mini_pico_inserir(double *cs, double *ps, int n, int max, double c, double p) {
  int i = 0;
  while (i < n && ps[i] < p) i++;
  if ((i < n && ps[i] == p) || n >= max) return n;
  for (int k = n; k > i; k--) { cs[k] = cs[k - 1]; ps[k] = ps[k - 1]; }
  cs[i] = c; ps[i] = p;
  return n + 1;
}

// Samples ascending by decoded time. *pico = the best sample's index.
static inline int mini_pico_avaliar(const double *cs, int n, double minCorr, int *pico) {
  if (n < 1) return MINI_PICO_AMBIGUO;
  int m = 0;
  for (int k = 1; k < n; k++) if (cs[k] > cs[m]) m = k;
  *pico = m;
  if (n < 2) return MINI_PICO_AMBIGUO;
  if (m == 0 || m == n - 1) {
    if (cs[m] < minCorr - MINI_PICO_BORDA_FOLGA) return MINI_PICO_FRACO;
    if (cs[m] == cs[m == 0 ? 1 : m - 1]) return MINI_PICO_AMBIGUO;
    return m == 0 ? MINI_PICO_BORDA_ANTES : MINI_PICO_BORDA_DEPOIS;
  }
  if (cs[m] <= cs[m - 1] || cs[m] <= cs[m + 1]) return MINI_PICO_AMBIGUO;   // a plateau has no single time
  double fundo = cs[m - 1] < cs[m + 1] ? cs[m - 1] : cs[m + 1];
  if (cs[m] - fundo < MINI_PICO_PROEMINENCIA) return MINI_PICO_AMBIGUO;     // a still shot looks the same everywhere
  double menor = cs[0];
  for (int k = 0; k < n; k++) {
    if (cs[k] < menor) menor = cs[k];
    // a second hill as high as this one: no single time (Se7en 0.66 0.67 0.68 0.64 0.68 voted a false +4 s twice)
    if ((k < m - 1 || k > m + 1) && cs[k] > cs[m] - MINI_PICO_PROEMINENCIA) return MINI_PICO_AMBIGUO;
  }
  if (cs[m] - menor < MINI_PICO_CONTRASTE) return MINI_PICO_AMBIGUO;
  if (cs[m] < minCorr) return MINI_PICO_FRACO;
  return MINI_PICO_OK;
}

// Parabola through the bracketed peak; the time stays within its neighbours.
static inline double mini_pico_tempo(const double *cs, const double *ps, int m) {
  double den = cs[m - 1] - 2 * cs[m] + cs[m + 1];
  double s = den < -1e-9 ? 0.5 * (cs[m - 1] - cs[m + 1]) / den : 0;
  if (s > 1) s = 1;
  if (s < -1) s = -1;
  return ps[m] + s * (s > 0 ? ps[m + 1] - ps[m] : ps[m] - ps[m - 1]);
}

// The largest group of votes within tol of each other. A vote from a bad start cannot join a later true group.
// *baixo and *alto bound the group. Ties go to the tighter group.
static inline int mini_votos_consenso(const double *offs, int n, double tol, double *baixo, double *alto) {
  int melhor = 0;
  double larg = 0;
  for (int i = 0; i < n; i++) {
    int c = 0;
    double lo = offs[i], hi = offs[i];
    for (int k = 0; k < n; k++) if (offs[k] >= offs[i] && offs[k] <= offs[i] + tol) { c++; if (offs[k] > hi) hi = offs[k]; }
    if (c > melhor || (c == melhor && hi - lo < larg)) { melhor = c; larg = hi - lo; *baixo = lo; *alto = hi; }
  }
  return melhor;
}

// No group of four came together: was the copy shown to be wrong, or did the evidence simply run out?
static inline int mini_tela_sem_consenso(int cancelada, int capturas, int votos, int consenso, int largasVazias) {
  if (cancelada) return MINI_TELA_INCONCLUSIVA;
  if (!capturas) return MINI_TELA_SEM_QUADROS;
  if (largasVazias >= MINI_LARGAS_REJEITA && consenso < 2) return MINI_TELA_REJEITADA;   // nothing like the screen nearby
  if (votos >= 4 && consenso * 2 <= votos) return MINI_TELA_REJEITADA;                   // clear peaks, no common offset
  return MINI_TELA_INCONCLUSIVA;
}

// The copy/add-on memory learns only from a real verdict.
static inline int mini_tela_anota(int resultado) { return resultado == MINI_TELA_ACEITA || resultado == MINI_TELA_REJEITADA; }
#endif
