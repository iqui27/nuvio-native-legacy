// FPS regression scenes, headless (no GL): percentiles, the p95/upload gate and
// the TV script runner. Ver src/fpscena.h. A regression must turn the gate red.
#include "../src/fpscena.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok   " : "FALHA", m); if (!c) falhas++; }

// Model of the real Home row handler: RIGHT saturates at the last card, LEFT on
// the first card opens the side menu (home.c).
static int col, menu, cartoes = 5;
static int estadoMenu(const char *c) { (void)c; return !menu; }
static void tecla(int k) {
  if (k == SDLK_RIGHT && col < cartoes - 1) col++;
  else if (k == SDLK_LEFT) { if (col == 0) menu = 1; else col--; }
}
static int estadoFalso(const char *c) { (void)c; return 0; }

static void enche(FpsAmostra *a, double base, double lento, int nlentos, int upl) {
  int i;
  fps_zerar(a);
  for (i = 0; i < 100; i++) fps_add(a, i < nlentos ? lento : base, i == 0 ? upl : 0);
}

int main(int argc, char **argv) {
  if (argc == 3 && !strcmp(argv[1], "--log")) {
    int q;
    fpscena_iniciar(NULL, NULL);
    for (q = 0; q < 5000 && fpscena_ativo(); q++) fpscena_quadro(atof(argv[2]), 0);
    fpscena_iniciar("player-osd", NULL);
    for (q = 0; q < 5000 && fpscena_ativo(); q++) fpscena_quadro(atof(argv[2]), 0);
    return 0;
  }
  FpsAmostra a;
  char l[400];
  double p; long u;
  const char *base = "# cena p95 upl\nhome-scroll 1.00 4\ntitle-page 0.50 2\n";
  int i, teclas = 0, q;

  enche(&a, 1.0, 9.0, 0, 3);
  ok(fps_percentil(&a, 95) == 1.0 && fps_percentil(&a, 50) == 1.0, "percentis de quadros iguais");
  enche(&a, 1.0, 9.0, 3, 3);
  ok(fps_percentil(&a, 95) == 1.0 && fps_percentil(&a, 99) == 9.0 && fps_pior(&a) == 9.0, "3% lentos: p95 limpo, p99 e pior pegam");
  enche(&a, 1.0, 9.0, 10, 3);
  ok(fps_percentil(&a, 95) == 9.0, "10% lentos: p95 sobe");
  fps_linha(&a, "x", l, sizeof l);
  ok(strstr(l, "[fps-cena] x n=100 p50=1.00 p95=9.00") && strstr(l, "upl=3"), "linha de relatorio");

  ok(fps_base_achar(base, "title-page", &p, &u) && p == 0.5 && u == 2, "base: acha cena");
  ok(!fps_base_achar(base, "title", &p, &u) && !fps_base_achar(base, "nada", &p, &u), "base: nome exato");
  ok(fps_base_achar("FPSBASE home-scroll 16.667 0\n", "home-scroll", &p, &u) && p == 16.667 && u == 0 &&
     fps_base_achar("# c\nFPSBASE\ttitle-page 2.5 3\n", "title-page", &p, &u) && p == 2.5 && u == 3 &&
     !fps_base_achar("FPSBASEx home-scroll 1 0\n", "home-scroll", &p, &u), "base: aceita prefixo FPSBASE");
  ok(!fps_base_achar("x nan 0\n", "x", &p, &u) &&
     !fps_base_achar("x 1 \n", "x", &p, &u) &&
     !fps_base_achar("x 1 -1\n", "x", &p, &u) &&
     !fps_base_achar("x \n1 0\n", "x", &p, &u) &&
     !fps_base_achar("x 1 \n0\n", "x", &p, &u), "base: rejeita numero invalido ou incompleto");
  fps_zerar(&a);
  for (i = 1; i <= 100; i++) fps_add(&a, i, 0);
  ok(fps_percentil(&a, 50) == 50 && fps_percentil(&a, 95) == 95 && fps_percentil(&a, 99) == 99,
     "percentis nearest-rank");
  fps_zerar(&a);
  ok(!fps_gate(&a, "x", 1, 0, 10, 0.5, l, sizeof l), "gate: nenhuma amostra reprova");

  enche(&a, 1.0, 1.0, 0, 4);
  ok(fps_gate(&a, "home-scroll", 1.0, 4, 50, 0.5, l, sizeof l) && strstr(l, " OK "), "gate: igual a base passa");
  enche(&a, 1.4, 1.4, 0, 4);
  ok(fps_gate(&a, "home-scroll", 1.0, 4, 50, 0.5, l, sizeof l), "gate: ruido dentro da folga passa");
  enche(&a, 3.0, 3.0, 0, 4);
  ok(!fps_gate(&a, "home-scroll", 1.0, 4, 50, 0.5, l, sizeof l) && strstr(l, "FALHA"), "gate: p95 3x a base falha");
  enche(&a, 1.0, 1.0, 0, 40);
  ok(!fps_gate(&a, "home-scroll", 1.0, 4, 50, 0.5, l, sizeof l), "gate: uploads 10x a base falham");

  // Runner: fast frames pass the gate, slow ones turn it red.
  for (q = 0; q < 2; q++) {
    teclas = 0;
    fpscena_iniciar(NULL, base);
    for (i = 0; i < 5000 && fpscena_ativo(); i++) if (fpscena_quadro(q ? 5.0 : 0.8, 0)) teclas++;
    if (!q) ok(!fpscena_ativo() && teclas > 20 && i > 600, "runner: percorre as duas cenas e injeta teclas");
    ok(q ? fpscena_falhas() == 2 : fpscena_falhas() == 0, q ? "runner: quadros lentos reprovam as 2 cenas" : "runner: quadros rapidos passam");
  }
  fpscena_iniciar("title-page", base);
  for (i = 0; i < 5000 && fpscena_ativo(); i++) fpscena_quadro(5.0, 0);
  ok(fpscena_falhas() == 1, "runner: escolha de cena roda so uma");
  fpscena_iniciar("", base);   // an empty flag file on the TV = every scene
  for (i = 0; i < 5000 && fpscena_ativo(); i++) fpscena_quadro(5.0, 0);
  ok(fpscena_falhas() == 2, "runner: arquivo vazio roda todas as cenas");
  fpscena_iniciar("player-osd", "player-osd 16.667 0\n");
  teclas = 0;
  for (i = 0; i < 5000 && fpscena_ativo(); i++) {
    int k = fpscena_quadro(33.334, 0);
    if (k) teclas++;
    if (k && k != SDLK_DOWN) falhas++;
  }
  ok(i == 420 && teclas > 20 && fpscena_falhas() == 1, "OSD: cena propria, p95 16.667 -> 33.334 ms reprova");
  fpscena_iniciar("player-osd", "player-osd 16.667 0\n");
  for (i = 0; i < 5000 && fpscena_ativo(); i++) fpscena_quadro(16.667, 0);
  ok(fpscena_falhas() == 0, "OSD: p95 16.667 -> 16.667 ms passa");
  // home-scroll must never push LEFT past the first card, even on a 4-card row.
  for (q = 4; q <= 5; q++) {
    col = menu = 0; cartoes = q;
    fpscena_estado(estadoMenu);
    fpscena_iniciar("home-scroll", NULL);
    for (i = 0; i < 5000 && fpscena_ativo(); i++) { int k = fpscena_quadro(1.0, 0); if (k && i > 130) tecla(k); }
    ok(!menu && fpscena_falhas() == 0, "home-scroll: nunca abre o menu lateral");
  }
  // wrong screen: scene fails closed, not measured
  fpscena_estado(estadoFalso);
  fpscena_iniciar("home-scroll", "home-scroll 1.0 0\n");
  for (i = 0; i < 5000 && fpscena_ativo(); i++) fpscena_quadro(0.8, 0);
  ok(fpscena_falhas() == 1, "estado errado reprova a cena");
  fpscena_estado(NULL);
  fpscena_iniciar("home-scroll", "title-page 1.0 0\n");
  for (i = 0; i < 5000 && fpscena_ativo(); i++) fpscena_quadro(0.8, 0);
  ok(fpscena_falhas() == 1, "baseline incompleta nao pode passar sem medir");
  return falhas ? 1 : (puts("ok"), 0);
}
