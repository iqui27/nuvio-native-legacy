#include <assert.h>
#include "../src/miniatura_lifecycle.h"

int main(void) {
  const long floorKB = 300L * 1024;
  assert(mini_refine_memory_ok(345L * 1024, floorKB, 1));
  assert(!mini_refine_memory_ok(345L * 1024, floorKB, 0));
  assert(!mini_refine_memory_ok(floorKB - 1, floorKB, 1));
  assert(mini_refine_memory_ok(floorKB, floorKB, 1));
  assert(mini_refine_memory_ok(401L * 1024, floorKB, 0));
  assert(mini_refine_memory_ok(-1, floorKB, 1));
  MiniPauseAdmission p = {0};
  assert(!mini_pause_admit(&p, 0, 0, 0, 0));
  assert(!mini_pause_admit(&p, 10, 1, 1, 0));
  assert(!mini_pause_admit(&p, 4000, 1, 1, 0));
  assert(!mini_pause_admit(&p, 5000, 1, 0, 1));
  assert(!mini_pause_admit(&p, 6000, 1, 0, 0));
  assert(!mini_pause_admit(&p, 7999, 1, 0, 0));
  assert(mini_pause_admit(&p, 8000, 1, 0, 0));
  // Timeout, memory release or failure cannot grant another attempt.
  assert(!mini_pause_admit(&p, 20000, 1, 0, 0));
  assert(!mini_pause_admit(&p, 21000, 0, 1, 0));
  assert(!mini_pause_admit(&p, 22000, 1, 0, 0));
  assert(!mini_pause_admit(&p, 25000, 1, 0, 0));
  assert(!mini_pause_admit(&p, 26000, 0, 0, 0));
  assert(!mini_pause_admit(&p, 27000, 1, 0, 0));
  assert(mini_pause_admit(&p, 29000, 1, 0, 0));

  // ---- screen check ----
  int m = -1;
  double t1[] = {0.20, 0.45, 0.70};                       // rising all the way to the edge: never a match
  assert(mini_pico_avaliar(t1, 3, 0.55, &m) == MINI_PICO_BORDA_DEPOIS && m == 2);
  double t2[] = {0.70, 0.45, 0.20};
  assert(mini_pico_avaliar(t2, 3, 0.55, &m) == MINI_PICO_BORDA_ANTES && m == 0);
  double t3[] = {0.20, 0.45, 0.70, 0.40};                 // one more sample brackets it
  assert(mini_pico_avaliar(t3, 4, 0.55, &m) == MINI_PICO_OK && m == 2);
  double t4[] = {0.05, 0.20, 0.30};                       // a low edge is a miss, not a trail
  assert(mini_pico_avaliar(t4, 3, 0.55, &m) == MINI_PICO_FRACO);
  double t5[] = {0.10, 0.40, 0.12};                       // a clear peak under the match level
  assert(mini_pico_avaliar(t5, 3, 0.55, &m) == MINI_PICO_FRACO);
  double t6[] = {0.80, 0.81, 0.80};                       // a still shot: high, but no single time
  assert(mini_pico_avaliar(t6, 3, 0.55, &m) == MINI_PICO_AMBIGUO);
  double t7[] = {0.60, 0.80, 0.80, 0.60};                 // a plateau
  assert(mini_pico_avaliar(t7, 4, 0.55, &m) == MINI_PICO_AMBIGUO);
  double t8[] = {0.80};
  assert(mini_pico_avaliar(t8, 1, 0.55, &m) == MINI_PICO_AMBIGUO);
  assert(mini_pico_avaliar(t8, 0, 0.55, &m) == MINI_PICO_AMBIGUO);
  double t9[] = {0.80, 0.80, 0.20};                       // a tie at the edge points nowhere
  assert(mini_pico_avaliar(t9, 3, 0.55, &m) == MINI_PICO_AMBIGUO);
  double t10[] = {0.66, 0.67, 0.68, 0.64, 0.68};          // Se7en's false +4 s: a second hill as high as the peak
  assert(mini_pico_avaliar(t10, 5, 0.55, &m) == MINI_PICO_AMBIGUO);
  double t11[] = {0.64, 0.67, 0.64};                      // a bump in a still shot
  assert(mini_pico_avaliar(t11, 3, 0.55, &m) == MINI_PICO_AMBIGUO);
  double t12[] = {-0.09, -0.09, 0.81, 0.83, 0.80};        // a real shot: high plateau after a cut
  assert(mini_pico_avaliar(t12, 5, 0.55, &m) == MINI_PICO_OK && m == 3);
  double t13[] = {-0.32, 0.80, 0.79};                     // just after a cut: a real vote, its time good to about a second
  assert(mini_pico_avaliar(t13, 3, 0.55, &m) == MINI_PICO_OK && m == 1);
  double t14[] = {-0.03, 0.85, 0.96, 0.80};
  assert(mini_pico_avaliar(t14, 4, 0.55, &m) == MINI_PICO_OK && m == 2);

  // a repeated decoded frame adds no evidence, so it cannot fake a bracket
  double cs[8], ps[8];
  int n = 0;
  n = mini_pico_inserir(cs, ps, n, 8, 0.5, 2000);
  n = mini_pico_inserir(cs, ps, n, 8, 0.8, 1000);
  n = mini_pico_inserir(cs, ps, n, 8, 0.9, 2000);
  n = mini_pico_inserir(cs, ps, n, 8, 0.3, 3000);
  assert(n == 3 && ps[0] == 1000 && ps[1] == 2000 && ps[2] == 3000 && cs[1] == 0.5);
  assert(mini_pico_avaliar(cs, n, 0.55, &m) == MINI_PICO_BORDA_ANTES);
  double full[1], fullP[1];
  n = mini_pico_inserir(full, fullP, 0, 1, 0.5, 1);
  assert(mini_pico_inserir(full, fullP, n, 1, 0.6, 2) == 1);

  // the peak's time stays between its neighbours, and leans to the stronger one
  double c1[] = {0.2, 0.8, 0.2}, p1[] = {1000, 2000, 3000};
  assert(mini_pico_tempo(c1, p1, 1) == 2000);
  double c2[] = {0.2, 0.8, 0.6};
  double tt = mini_pico_tempo(c2, p1, 1);
  assert(tt > 2000 && tt < 3000);

  // Se7en's walk could never win once real peaks appear: the three true votes outvote the bad start
  double lo = 0, hi = 0;
  double v1[] = {-4.31, 0.27, 0.63, 0.32};
  assert(mini_votos_consenso(v1, 4, 3.0, &lo, &hi) == 3 && lo == 0.27 && hi == 0.63);
  double v2[] = {-4.31, 0.27, 0.63, 0.32, 0.87};
  assert(mini_votos_consenso(v2, 5, 3.0, &lo, &hi) == 4);
  double v3[] = {0.0, 2.0, 4.0, 6.0};
  assert(mini_votos_consenso(v3, 4, 3.0, &lo, &hi) == 2);
  double v4[] = {0.0, 2.9, 10.0, 10.1};                    // equal groups: the tighter one
  assert(mini_votos_consenso(v4, 4, 3.0, &lo, &hi) == 2 && lo == 10.0);
  assert(mini_votos_consenso(v4, 0, 3.0, &lo, &hi) == 0);

  // leaving, timing out or running short of evidence says nothing about the copy
  assert(mini_tela_sem_consenso(1, 1, 1, 1, 0) == MINI_TELA_INCONCLUSIVA);
  assert(mini_tela_sem_consenso(1, 5, 0, 0, 9) == MINI_TELA_INCONCLUSIVA);
  assert(mini_tela_sem_consenso(0, 0, 0, 0, 0) == MINI_TELA_SEM_QUADROS);
  assert(mini_tela_sem_consenso(0, 3, 1, 1, 0) == MINI_TELA_INCONCLUSIVA);
  assert(mini_tela_sem_consenso(0, 9, 0, 0, 3) == MINI_TELA_INCONCLUSIVA);   // Se7en's correct copy got this far
  assert(mini_tela_sem_consenso(0, 9, 0, 0, 5) == MINI_TELA_INCONCLUSIVA);
  assert(mini_tela_sem_consenso(0, 9, 0, 0, 6) == MINI_TELA_REJEITADA);
  assert(mini_tela_sem_consenso(0, 9, 3, 3, 6) == MINI_TELA_INCONCLUSIVA);
  assert(mini_tela_sem_consenso(0, 9, 4, 2, 0) == MINI_TELA_REJEITADA);
  assert(mini_tela_sem_consenso(0, 9, 4, 3, 0) == MINI_TELA_INCONCLUSIVA);
  assert(mini_tela_anota(MINI_TELA_ACEITA) && mini_tela_anota(MINI_TELA_REJEITADA));
  assert(!mini_tela_anota(MINI_TELA_INCONCLUSIVA) && !mini_tela_anota(MINI_TELA_SEM_QUADROS));
  return 0;
}
