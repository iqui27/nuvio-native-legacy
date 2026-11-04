// Reuse host GL/storage doubles; drive the production ladder.
#define main legacy_main
#include "gpunivel.c"
#undef main
#include "resolucao.h"

extern int gpun_teste_automaticos;
static void report(double fps, int scene, int clean) {
  gpun_relatar(fps, 47.4, 1000.0 / 60, scene, clean);
}
static void reports(int n, double fps) { while (n--) report(fps, 1, 1); }
static void fresh(void) { gravado[0] = 0; gpun_iniciar(1920, 1080); }
static void reset_auto(ResAuto *a, int hz) { memset(a, 0, sizeof *a); a->hz = hz; }

// Refresh-relative 4K thresholds; 60 Hz keeps its historic values.
static void resolution_refresh(void) {
  ResAuto a; ResVigia v;
  int i;
  // Healthy 50 Hz panel, no GPU clock: must pass (55 FPS was unreachable).
  reset_auto(&a, 50);
  assert(res_auto_amostra(&a, 49.9, 0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 49.9, 0, 1) == RES_AUTO_APROVOU);
  reset_auto(&a, 50); // Below refresh - 5 fails at the verdict.
  assert(res_auto_amostra(&a, 44, 0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 44, 0, 1) == RES_AUTO_REBAIXOU);
  reset_auto(&a, 50); // 2/3 of refresh fails at once; 40 is not that at 50 Hz.
  assert(res_auto_amostra(&a, 40, 0, 1) == RES_AUTO_NADA);
  reset_auto(&a, 50);
  assert(res_auto_amostra(&a, 33, 0, 1) == RES_AUTO_REBAIXOU);
  // GPU budgets scale with the 20 ms frame: 14.4 and 30 ms.
  reset_auto(&a, 50);
  assert(res_auto_amostra(&a, 49.9, 14, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 49.9, 14, 1) == RES_AUTO_APROVOU);
  reset_auto(&a, 50);
  assert(res_auto_amostra(&a, 49.9, 29, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 49.9, 14, 1) == RES_AUTO_REBAIXOU);
  // Unknown, 60+ and sub-48 Hz modes keep the 60 Hz rule (4K30 stays rejected).
  int legacy[] = {0, 60, 120, 30};
  for (i = 0; i < 4; i++) {
    reset_auto(&a, legacy[i]);
    assert(res_auto_amostra(&a, 49.9, 0, 1) == RES_AUTO_NADA);
    assert(res_auto_amostra(&a, 49.9, 0, 1) == RES_AUTO_REBAIXOU);
    reset_auto(&a, legacy[i]);
    assert(res_auto_amostra(&a, 39, 0, 1) == RES_AUTO_REBAIXOU);
  }
  // Approved 4K at 50 Hz: 35 FPS is not bad, 33 is.
  reset_auto(&a, 50); a.estado = RES_AUTO_4K;
  for (i = 0; i < 9; i++) assert(res_auto_amostra(&a, 35, 0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 33, 0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 33, 0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 33, 0, 1) == RES_AUTO_REBAIXOU);
  // Manual 4K watch: same relation, still four reports.
  memset(&v, 0, sizeof v); v.hz = 50;
  for (i = 0; i < 9; i++) assert(!res_vigia_amostra(&v, 35, 0, 1));
  for (i = 0; i < 9; i++) assert(!res_vigia_amostra(&v, 35, 35, 1));
  for (i = 0; i < 3; i++) assert(!res_vigia_amostra(&v, 30, 37, 1));
  assert(res_vigia_amostra(&v, 30, 37, 1));
  memset(&v, 0, sizeof v); // hz 0 = the historic 40 FPS / 30 ms.
  for (i = 0; i < 3; i++) assert(!res_vigia_amostra(&v, 39, 0, 1));
  assert(res_vigia_amostra(&v, 39, 0, 1));
  // Probe that never sees a clean window ends, unsaved, instead of holding
  // the 4K surface on a TV whose frames always look CPU-bound.
  reset_auto(&a, 60);
  for (i = 0; i < RES_AUTO_SUJAS - 1; i++) assert(res_auto_amostra(&a, 15, 0, 0) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 15, 0, 0) == RES_AUTO_INCONCLUSIVA);
  assert(a.estado == RES_AUTO_1080 && res_auto_amostra(&a, 15, 0, 0) == RES_AUTO_NADA);
  // Clean-window bound does not apply once 4K is approved.
  reset_auto(&a, 60); a.estado = RES_AUTO_4K;
  for (i = 0; i < 2 * RES_AUTO_SUJAS; i++) assert(res_auto_amostra(&a, 15, 0, 0) == RES_AUTO_NADA);
  assert(a.estado == RES_AUTO_4K);
}

// The resolution probe and the ladder judge the same window.
static void shared_eligibility(void) {
  double fps;
  fresh();
  fps = 30; assert(!gpun_janela_elegivel(&fps, 40, 0, 1)); // Player/settings.
  fps = 30; assert(!gpun_janela_elegivel(&fps, 40, 1, 0)); // Empty Home/settling.
  fps = 30; assert(!gpun_janela_elegivel(&fps, 1001, 1, 1)); // Stall.
  fps = 30; assert(gpun_janela_elegivel(&fps, 40, 1, 1) && fps == 30);
  // Dirty frame invalidates the window; asking does not consume the measure.
  gpun_medir(25, 20, 2, 1, 0);
  for (int f = 0; f < 120; f++) gpun_medir(25, 20, 2, 1, 1);
  fps = 30; assert(!gpun_janela_elegivel(&fps, 40, 1, 1));
  fps = 30; assert(!gpun_janela_elegivel(&fps, 40, 1, 1));
  report(30, 1, 1); // Reporting consumes it.
  // Clean frames replace the raw FPS, same as the ladder.
  for (int f = 0; f < 120; f++) gpun_medir(20, 15, 2, 1, 1);
  gpun_medir(200, 180, 2, 1, 1);
  fps = 30; assert(gpun_janela_elegivel(&fps, 200, 1, 1) && fps == 50);
  // CPU-bound windows are not GPU evidence.
  report(30, 1, 1);
  for (int f = 0; f < 60; f++) gpun_medir(50, 1, 35, 1, 1);
  fps = 20; assert(!gpun_janela_elegivel(&fps, 50, 1, 1));
  report(20, 1, 1);
}

// One trial at a time across resolution and effects.
static void serialized_trials(void) {
  fresh(); report(60, 1, 1);
  gpun_resolucao_em_prova(1);
  reports(20, 30); // A verdict is pending: effects keep their quality.
  assert(gpun_degrau() == 0 && !gpun_em_teste());
  gpun_resolucao_em_prova(0);
  reports(2, 30); assert(gpun_degrau() == 0); // Held windows left no streak.
  reports(1, 30); assert(gpun_degrau() == 1 && gpun_em_teste());
  reports(3, 34); assert(gpun_efeito_ativo(1) && !gpun_em_teste());
}

// Veto, recovery and manual choices stay bounded and explicit.
static void bounded_persistence(void) {
  fresh(); report(60, 1, 1);
  reports(3, 40); reports(3, 50.4); // Rung 1 kept; no stale failed-recovery mark.
  reports(10, 59.5); assert(gpun_degrau() == 0);
  reports(10, 59.5);
  reports(3, 40); reports(3, 50.4); assert(gpun_degrau() == 1);
  reports(10, 59.5); assert(gpun_degrau() == 0);

  fresh(); report(60, 1, 1);
  reports(3, 40); reports(3, 50.4); reports(3, 40); reports(3, 50.4);
  assert(strstr(gravado, "foscoVetado=1"));
  // Manual effects do not inherit the automatic veto; Automatic restores it.
  gpun_preferencia(1); assert(!gpun_fosco_vetado());
  gpun_preferencia(0); assert(gpun_fosco_vetado());
  // Explicit Frost Automatic clears the disk even under manual effects.
  gpun_preferencia(2); gpun_fosco_reiniciar();
  assert(strstr(gravado, "foscoVetado=0"));
  gpun_iniciar(1920, 1080); assert(!gpun_fosco_vetado());

  // The veto expires: frost is proven again every GPUN_VETO_ARRANQUES launches.
  fresh(); report(60, 1, 1);
  reports(3, 40); reports(3, 50.4); reports(3, 40); reports(3, 50.4);
  for (int boot = 1; boot < GPUN_VETO_ARRANQUES; boot++) {
    gpun_iniciar(1920, 1080); assert(gpun_fosco_vetado());
  }
  gpun_iniciar(1920, 1080); assert(!gpun_fosco_vetado());
  gpun_iniciar(1920, 1080); assert(!gpun_fosco_vetado());
}
int main(void) {
  resolution_refresh();
  shared_eligibility();
  serialized_trials();
  bounded_persistence();
  fresh(); reports(12, 59.5);
  report(60, 0, 0); // Player.
  // Teardown/upload burst: even one dirty frame invalidates its window.
  const double fps[] = {46.6, 39.7, 39.8};
  for (int i = 0; i < 3; i++) {
    gpun_medir(25, 20, 2, 1, 0);
    for (int f = 0; f < 120; f++) gpun_medir(25, 20, 2, 1, 1);
    report(fps[i], 1, 1);
  }
  assert(gpun_degrau() == 0);
  reports(12, 59.5); assert(gpun_degrau() == 0);
  fresh(); report(60, 1, 1); // Scene settling window.
  for (int step = 1; step <= 3; step++) {
    reports(2, 40); assert(gpun_degrau() == step - 1);
    reports(1, 40); assert(gpun_degrau() == step);
    reports(1, 50.4); // One gain window cannot persist frost.
    if (step == 2) assert(!strstr(gravado, "foscoVetado=1"));
    reports(2, 50.4); assert(gpun_efeito_ativo(step));
  }
  reports(30, 59.5); assert(gpun_degrau() == 0);
  gpun_iniciar(1920, 1080); assert(gpun_degrau() == 0);
  // Forced effects still consume separate rungs; never 0 -> 3.
  gpun_teste_automaticos = 4; fresh(); report(60, 1, 1);
  for (int step = 1; step <= 3; step++) {
    reports(3, 40); assert(gpun_degrau() == step);
  }
  reports(3, 50.4); assert(gpun_efeito_ativo(3));
  reports(10, 59.5); assert(!gpun_efeito_ativo(3));
  // A single favorable post-trial sample cannot save a frost veto.
  gpun_teste_automaticos = 15; fresh(); report(60, 1, 1);
  reports(3, 40); reports(3, 50.4);
  reports(3, 40); reports(1, 50.4);
  gpun_iniciar(1920, 1080); assert(!gpun_fosco_vetado());
  puts("gpunivel_transients: ok");
  return 0;
}
