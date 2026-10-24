// Reuse the GPU/GL/storage doubles; exercise the production FPS reporter.
#define main legacy_main
#include "gpunivel.c"
#undef main

static int automatic = 15;
int ajustes_efeitos_automaticos(void) { return automatic; }
static void reports(int n, double fps) {
  while (n--) gpun_relatar(fps, 40, 1000.0 / 60, 1, 1);
}
static void fresh(void) {
  gravado[0] = 0; gpun_iniciar(1920, 1080);
  reports(1, 60); // Discard the initial scene window.
}
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "manual-effects")) {
    testRenderer = "Mali-G52";
    for (int mode = 1; mode <= 3; mode++) {
      fresh();
      for (int step = 1; step <= 4; step++) {
        reports(3, 30); reports(3, 34);
      }
      assert(gpun_degrau() == 4);
      gpun_preferencia(mode);
      assert(gpun_nivel() == (mode == 1 ? 0 : mode == 2 ? 1 : 2));
      for (int step = 1; step <= 4; step++) assert(!gpun_efeito_ativo(step));
      reports(120, 30);
      for (int step = 1; step <= 4; step++) assert(!gpun_efeito_ativo(step));
    }
    return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "frost-automatic")) {
    fresh();
    gpun_preferencia(3);
    automatic &= ~2; // Frost On does not reset the hardware mark.
    assert(gpun_nivel() == 2);
    automatic |= 2; // Settings calls this when Frost returns to Automatic.
    gpun_fosco_reiniciar();
    assert(!gpun_fosco_vetado());
    assert(gpun_nivel() == 2);
    reports(120, 30);
    assert(gpun_nivel() == 2);
    return 0;
  }
  fresh();
  for (int step = 1; step <= 4; step++) {
    reports(3, 30); assert(gpun_degrau() == step);
    if (step == 2) assert(!strstr(gravado, "foscoVetado=1"));
    reports(3, 34); // Both minimum gains sustained for three windows.
    assert(gpun_efeito_ativo(step));
    assert(gpun_fosco_vetado() == (step >= 2));
  }
  assert(strstr(gravado, "foscoVetado=1"));
  reports(120, 60); assert(gpun_degrau() == 0 && gpun_fosco_vetado());
  gpun_iniciar(1920, 1080); assert(gpun_fosco_vetado());
  gpun_fosco_reiniciar(); assert(!gpun_fosco_vetado());
  gpun_iniciar(1920, 1080); assert(!gpun_fosco_vetado());

  fresh(); reports(3, 30); reports(1, 32);
  assert(gpun_degrau() == 0 && !gpun_efeito_ativo(1));
  reports(3, 30); assert(gpun_degrau() == 1); // No-gain glass consumes its rung.
  reports(3, 30); assert(gpun_degrau() == 2);
  reports(1, 30);
  assert(!gpun_fosco_vetado() && !strstr(gravado, "foscoVetado=1"));
  reports(3, 30); assert(gpun_degrau() == 2); // Failed frost consumes its rung.
  reports(3, 30); assert(gpun_degrau() == 3);
  assert(!gpun_efeito_ativo(1) && !gpun_efeito_ativo(2));

  for (automatic = 0; automatic < 16; automatic++) {
    fresh();
    for (int step = 1; step <= 4; step++) {
      reports(3, 30); assert(gpun_degrau() == step);
      if (automatic & (1 << (step - 1))) reports(3, 34);
    }
  }
  automatic = 15;
  fresh();
  // Decode/upload and CPU/list/network stalls cannot advance the ladder.
  for (int window = 0; window < 6; window++) {
    for (int f = 0; f < 60; f++) gpun_medir(50, 1, 35, 1, 1);
    reports(1, 20);
  }
  assert(gpun_degrau() == 0);
  // An isolated scroll spike is removed from otherwise healthy steady FPS.
  for (int window = 0; window < 6; window++) {
    for (int f = 0; f < 120; f++) gpun_medir(16.667, 12, 2, 1, 1);
    gpun_medir(200, 180, 2, 1, 1); reports(1, 30);
  }
  assert(gpun_degrau() == 0);
  reports(2, 30); gpun_relatar(30, 1001, 16.667, 1, 1); reports(2, 30);
  assert(gpun_degrau() == 0);
  fresh();
  for (int i = 0; i < 20; i++) gpun_relatar(50, 40, 20, 1, 1);
  assert(gpun_degrau() == 0);
  reports(2, 30); gpun_relatar(30, 40, 16.667, 1, 0); reports(2, 30);
  assert(gpun_degrau() == 0);
  reports(1, 30); reports(3, 34); reports(3, 30); reports(3, 34);
  assert(gpun_fosco_vetado());
  testRenderer = "Different GPU"; gpun_iniciar(1920, 1080);
  assert(!gpun_fosco_vetado());
  puts("gpunivel_escada: ok");
  return 0;
}
