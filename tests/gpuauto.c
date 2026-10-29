// Reuse the existing GPU/disk doubles and legacy adaptive regression cases.
#define main gpunivel_legacy_main
#include "gpunivel.c"
#undef main

// Allow the regression to run on pre-fix, frame-only builds: it must fail at 0.
__attribute__((weak))
void gpun_relatar(double fps, double pior, double periodo, int cena, int cheia) {
  (void)fps; (void)pior; (void)periodo; (void)cena; (void)cheia;
}

static void report(double fps, double worst, double period, int scene, int full) {
  double dt = 1000.0 / fps;
  // CPU submission can exceed clr+swap while GPU work remains asynchronous.
  rodar(3, dt, 7, 12, scene, full);
  if (worst > 1000) gpun_medir(worst, 0, worst, scene, full);
  gpun_relatar(fps, worst, period, scene, full);
}

static void reports(int count, double fps) {
  while (count--) report(fps, 78.6, 16.70, 13, 1); // Home, Dynamic, glass/color on.
}

static void saved(int level) {
  gpun_teste_reiniciar();
  snprintf(gravado, sizeof gravado,
           "versao=1\nchave=0\nnivel=%d\navaliacao=1\nfps0=60\n", level);
  gpun_preferencia(0);
  assert(gpun_nivel() == level);
}

int main(int argc, char **argv) {
  if (argc > 1) return gpunivel_legacy_main(argc - 1, argv + 1);
  saved(0);
  report(39.6, 78.6, 16.70, 13, 1);
  report(39.3, 91.6, 16.70, 13, 1);
  assert(gpun_nivel() == 0);
  report(39.0, 78.6, 16.70, 13, 1);
  assert(gpun_nivel() == 1); // Fails before: legacy detector stays at 0.
  reports(3, 39.0);
  assert(gpun_nivel() == 2);
  reports(12, 39.0);
  assert(gpun_nivel() == 2);
  reports(12, 56.0); // Hysteresis band does not restore effects.
  assert(gpun_nivel() == 2);
  reports(9, 60.0);
  assert(gpun_nivel() == 2);
  reports(1, 60.0);
  assert(gpun_nivel() == 1);
  reports(3, 39.0); // Failed upgrade must not repeat for this workload.
  assert(gpun_nivel() == 2);
  reports(30, 60.0);
  assert(gpun_nivel() == 2);
  puts("ok  TCL 39.6/39.3/39.0: one tier per three reports, hysteresis, no oscillation");

  saved(0);
  reports(2, 39.0);
  report(18.0, 3700.0, 16.70, 13, 1);
  reports(1, 39.0);
  assert(gpun_nivel() == 0); // Stall breaks consecutive evidence.
  reports(2, 39.0);
  assert(gpun_nivel() == 1);
  saved(0);
  reports(2, 60.0);
  report(18.0, 3700.0, 16.70, 13, 1);
  reports(12, 60.0);
  assert(gpun_nivel() == 0);
  puts("ok  lone 3.7 s update stall ignored");

  saved(1);
  reports(3, 39.0);
  assert(gpun_nivel() == 2); // Saved lower tiers are revalidated too.
  saved(2);
  reports(10, 60.0);
  assert(gpun_nivel() == 1);
  reports(10, 60.0);
  assert(gpun_nivel() == 0);
  saved(0);
  reports(2, 39.0);
  report(39, 78.6, 16.70, 0, 1);
  reports(1, 39.0);
  assert(gpun_nivel() == 0);
  report(39, 78.6, 16.70, 13, 0);
  reports(1, 39.0);
  assert(gpun_nivel() == 0);
  report(39, 78.6, 16.70, 14, 1); // Settings/new workload starts fresh.
  assert(gpun_nivel() == 0);
  report(39, 78.6, 16.70, 14, 1);
  report(39, 78.6, 16.70, 14, 1);
  assert(gpun_nivel() == 1);
  puts("ok  saved tiers and scene/artwork eligibility revalidated");

  saved(0);
  for (int i = 0; i < 12; i++) report(49.9, 78.6, 20.0, 13, 1);
  assert(gpun_nivel() == 0); // Healthy 50 Hz panel.
  for (int pref = 1; pref <= 3; pref++) {
    gpun_preferencia(pref);
    reports(12, 28.8);
    assert(gpun_nivel() == pref - 1);
  }
  puts("ok  50 Hz and manual effects preserved");

  gravado[0] = 0;
  gpun_iniciar(3840, 2160);
  reports(3, 39.0);
  assert(gpun_nivel() == 1 && !gpun_alvo_1080_ativo());
  gpun_alvo_1080();
  assert(gpun_alvo_1080_ativo() && gpun_nivel() == 0);
  reports(3, 39.0);
  assert(gpun_nivel() == 1 && gpun_alvo_1080_ativo());
  report(18, 3700, 16.70, 13, 1);
  assert(gpun_nivel() == 1 && gpun_alvo_1080_ativo());
  gpun_preferencia(3);
  gpun_alvo_1080();
  assert(gpun_nivel() == 2 && gpun_alvo_1080_ativo());
  puts("ok  4K fallback preserves resolution logic and restarts effects validation");
  puts("gpuauto: all checks passed");
  return 0;
}
