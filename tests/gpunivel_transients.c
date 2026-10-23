// Reuse host GL/storage doubles; drive the production ladder.
#define main legacy_main
#include "gpunivel.c"
#undef main

static int automatic = 15;
int ajustes_efeitos_automaticos(void) { return automatic; }
static void report(double fps, int scene, int clean) {
  gpun_relatar(fps, 47.4, 1000.0 / 60, scene, clean);
}
static void reports(int n, double fps) { while (n--) report(fps, 1, 1); }
static void fresh(void) { gravado[0] = 0; gpun_iniciar(1920, 1080); }
int main(void) {
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
  automatic = 4; fresh(); report(60, 1, 1);
  for (int step = 1; step <= 3; step++) {
    reports(3, 40); assert(gpun_degrau() == step);
  }
  reports(3, 50.4); assert(gpun_efeito_ativo(3));
  reports(10, 59.5); assert(!gpun_efeito_ativo(3));
  // A single favorable post-trial sample cannot save a frost veto.
  automatic = 15; fresh(); report(60, 1, 1);
  reports(3, 40); reports(3, 50.4);
  reports(3, 40); reports(1, 50.4);
  gpun_iniciar(1920, 1080); assert(!gpun_fosco_vetado());
  puts("gpunivel_transients: ok");
  return 0;
}
