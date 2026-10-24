// Exercise the production settings math without a renderer or GL context.
#include "../src/ajustes.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#if __has_include("../src/tela21.h")
#include "../src/tela21.h"
#define HAVE_T21 1
#endif

static float scale = 1, active = 1;
float gfx_escala_txt(void) { return scale; }
float gfx_escala(void) { return active; }
float gfx_escala_tela_txt(void) { return active == scale ? scale : 1; }
float gfx_escala_txt_entrar(void) { float previous = active; active = scale; return previous; }
void gfx_escala_sair(float previous) { active = previous; }
static int gpuReduced;
int gpun_efeito_ativo(int step) { return step == 3 && gpuReduced; }

float t21_probe_biblioteca(void);
float t21_probe_comecar(void);
float t21_probe_conquistas(void);
float t21_probe_filmo(void);
static int failures;
static void near(const char *name, float actual, float expected) {
  if (fabsf(actual - expected) < .001f) return;
  printf("FAIL %s at %.0f%%: %.3f != %.3f\n", name, scale * 100, actual, expected);
  failures++;
}
int main(void) {
  const float scales[] = { .8f, .85f, 1, 1.15f, 1.3f };
  for (int layout = HOME_LAYOUT_MODERNA; layout < HOME_LAYOUT_N; layout++) {
    valor[AJ_HOME_LAYOUT] = layout;
    for (int rail = 0; rail < 2; rail++) {
      valor[AJ_RAIL_MODERNA] = 1;
      valor[AJ_RAIL] = rail ? 1 : 0;
      valor[AJ_MENU_TOPO] = 0;
      for (unsigned i = 0; i < sizeof scales / sizeof *scales; i++) {
        float x, w;
        scale = active = scales[i];
        float home = ajustes_conteudo_x();
        ajustes_area_conteudo(NV_CONTENT_PAD, NV_LEGACY_CONTENT_RIGHT, &x, &w);
        near("left matches Home", x * scale, home);
        near("right stays safe", (x + w) * scale, 1816);
        near("Library origin", t21_probe_biblioteca() * scale, home);
        near("Onboarding origin", t21_probe_comecar() * scale, home);
        near("Achievements origin", t21_probe_conquistas() * scale, home);
#ifdef HAVE_T21
        near("Filmography origin", t21_probe_filmo() * scale, home);
        t21_inicio(&x, &w);
        near("shared left", x * scale, home);
        near("shared right", (x + w) * scale, 1816);
        active = 1;
        t21_inicio(&x, &w);
        near("shared layout outside draw", x * scale, home);
        near("restored outer scale", active, 1);
#endif
      }
    }
  }
#ifdef HAVE_T21
  near("entry midpoint", t21_entrada(0, .160f, 1), .5f);
  near("exit midpoint", t21_entrada(1, .090f, 0), .5f);
  near("stagger pending", anim_atraso(.01f, 2, 28, 320), 0);
  near("stagger done", anim_atraso(.5f, 7, 28, 320), .95f);
  near("stagger capped", anim_atraso(.5f, 99, 28, 320), .95f);
  anim_politica_reduzida = 1;
  near("reduced stagger", anim_atraso(0, 7, 28, 320), 1);
  anim_politica_reduzida = 0;
#endif
  for (gpuReduced = 0; gpuReduced <= 1; gpuReduced++) {
    for (int choice = 0; choice < 3; choice++) {
      valor[AJ_ANIM] = choice;
      near("only explicit reduced motion", ajustes_animacoes_reduzidas(), choice == 1);
    }
  }
  if (!failures) puts("PASS tela21: safe area 80/85/100/115/130%, entry, stagger, explicit reduced motion");
  return failures ? 1 : 0;
}
