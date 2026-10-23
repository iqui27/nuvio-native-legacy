// Regression: fresh/missing setting is automatic; every saved index survives reload.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

static void fixture(const char *dir, const char *value) {
  char path[1200];
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w"); assert(f);
  fputs(value, f); fclose(f);
  ajustes_dir(dir);
}
void ui_settings_test(const char *dir, int baseline) {
  if (baseline) { fixture(dir, "tamanhoUiLocal 0\n"); return; }
  const float old[] = {1.0f, 1.2f, 1.3f, 1.5f};
  assert(ajustes_tamanho_ui() == .85f); // factory default
  const float inches[] = {0, NAN, -1, 19, 121, 43, 32, 50, 55, 65, 74, 75, 85, 98};
  const float want[] = {1, 1, 1, 1, 1, 1.15f, 1.15f, 1, 1, 1, 1, .85f, .85f, .85f};
  for (int i = 0; i < 14; i++) assert(ui_size_inches(inches[i]) == want[i]);
  assert(ui_size_model("OLED83C4") == 83);
  assert(ui_size_model("UN85CU7700GXZD") == 85);
  assert(ui_size_model("50NANO80ASA") == 50);
  assert(ui_size_model("TV-4K-2026") == 0 && ui_size_model(NULL) == 0);
  assert(fabsf(ui_size_diagonal(3840, 2160, 51.83f, 51.83f) - 85) < .1f);
  assert(ui_size_diagonal(1920, 1080, 0, 160) == 0);
  assert(ui_size_diagonal(1920, 1080, 160, 160) == 0);
  assert(ui_size_text(15, .8f) * .8f >= 18);
  assert(ui_size_text(21, .85f) * .85f >= 18);
  assert(ui_size_text(15, 1) == 15);
  fixture(dir, "idioma 1\n");
  assert(ajustes_tamanho_ui() == 0.85f);
  assert(gravar());
  ajustes_dir(dir); assert(ajustes_tamanho_ui() == 0.85f);
  for (int i = 0; i < 4; i++) {
    char line[64]; snprintf(line, sizeof line, "tamanhoUiLocal %d\n", i);
    fixture(dir, line); assert(ajustes_tamanho_ui() == old[i]);
    assert(gravar()); ajustes_dir(dir); assert(ajustes_tamanho_ui() == old[i]);
  }
  // New explicit percentages and automatic are also persisted.
  const float added[] = {0.8f, 0.85f, 0.9f, 1.15f, 0.85f};
  for (int i = 0; i < 5; i++) {
    char line[64]; snprintf(line, sizeof line, "tamanhoUiLocal %d\n", i + 4);
    fixture(dir, line); assert(ajustes_tamanho_ui() == added[i]);
    assert(gravar()); ajustes_dir(dir); assert(ajustes_tamanho_ui() == added[i]);
  }
  fixture(dir, "tamanhoUiLocal 0\n");
}
