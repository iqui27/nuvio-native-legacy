// Offline reporter regression for the automatic effects ladder (gpunivel.h).
#define main gpunivel_legacy_main
#include "gpunivel.c"
#undef main

static void auto_relato(double fps, double worst, double period, int scene, int full) {
  rodar(3, 1000.0 / fps, 20, 4, scene, full);
  if (worst > 1000) gpun_medir(worst, 0, worst, scene, full);
  gpun_relatar(fps, worst, period, scene, full);
}
static void auto_relatos(int n, double fps, double period, int scene) {
  while (n--) auto_relato(fps, 50, period, scene, 1);
}

int main(int argc, char **argv) {
  if (argc > 1) return gpunivel_legacy_main(argc - 1, argv + 1);

  gpun_teste_reiniciar(); gravado[0] = 0;
  gpun_iniciar(1920, 1080);
  auto_relato(60, 50, 1000.0 / 60.0, 1, 1);
  auto_relatos(2, 39, 1000.0 / 60.0, 1);
  assert(gpun_degrau() == 0);
  auto_relatos(1, 39, 1000.0 / 60.0, 1);
  assert(gpun_degrau() == 1);
  auto_relatos(3, 50.4, 1000.0 / 60.0, 1);
  assert(gpun_efeito_ativo(1));
  auto_relatos(3, 39, 1000.0 / 60.0, 1);
  assert(gpun_degrau() >= 2);
  auto_relatos(30, 59.5, 1000.0 / 60.0, 1);
  assert(gpun_degrau() == 0);
  puts("ok  TCL slow FPS: one rung per three reports, trials need gain, recovery");

  gpun_teste_reiniciar(); gravado[0] = 0;
  gpun_iniciar(1920, 1080);
  auto_relato(60, 50, 1000.0 / 60.0, 1, 1);
  auto_relatos(2, 39, 1000.0 / 60.0, 1);
  auto_relato(18, 3700, 1000.0 / 60.0, 1, 1);
  auto_relatos(3, 39, 1000.0 / 60.0, 1);
  assert(gpun_degrau() == 1);
  puts("ok  lone 3.7 s update stall ignored");

  gpun_teste_reiniciar(); gravado[0] = 0;
  gpun_iniciar(1920, 1080);
  auto_relato(60, 50, 20.0, 1, 1);
  auto_relatos(12, 49.9, 20.0, 1);
  assert(gpun_degrau() == 0);
  testRenderer = "Mali-G52";
  gpun_iniciar(1920, 1080);
  for (int pref = 1; pref <= 3; pref++) {
    gpun_preferencia(pref);
    auto_relatos(12, 28.8, 1000.0 / 60.0, 1);
    assert(gpun_nivel() == pref - 1);
  }
  puts("ok  50 Hz and manual effects preserved");

  gravado[0] = 0;
  gpun_iniciar(3840, 2160);
  auto_relato(60, 50, 1000.0 / 60.0, 1, 1);
  auto_relatos(3, 39, 1000.0 / 60.0, 1);
  assert(gpun_degrau() >= 1 && !gpun_alvo_1080_ativo());
  gpun_alvo_1080();
  assert(gpun_alvo_1080_ativo() && gpun_degrau() == 0);
  auto_relato(60, 50, 1000.0 / 60.0, 1, 1);
  auto_relatos(3, 39, 1000.0 / 60.0, 1);
  assert(gpun_degrau() >= 1 && gpun_alvo_1080_ativo());
  gpun_preferencia(3);
  gpun_alvo_1080();
  assert(gpun_nivel() == 2 && gpun_alvo_1080_ativo());
  puts("ok  4K fallback preserves resolution logic and restarts effects validation");
  puts("gpuauto: all checks passed");
  return 0;
}
