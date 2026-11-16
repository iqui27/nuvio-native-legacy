// Provas de src/tpkborda.c (premult do GLView so enquanto o libass desenha)
// e das contas de Subtitle contrast / gamma (assperf.c). Ver tests/tpkborda.sh.
#include "../src/tpkborda.h"
#include "../src/assperf.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int quer, aberto, chamadas, ultimo = -1, app;
int assrender_borda_quer(void) { return quer; }
int player_legenda_app_quer(void) { return app; }   // app-drawn (SRT) text on screen
int player_aberto(void) { return aberto; }
void nv_tpk_premult_registrar(void (*f)(int));
static void host(int v) { chamadas++; ultimo = v; }

int main(void) {
  // Sem host registrado (6.0/6.5 ou host velho): nada quebra, nada e chamado.
  aberto = 1; quer = 1; tpkborda_quadro(); quer = 0; tpkborda_quadro();
  assert(chamadas == 0);
  nv_tpk_premult_registrar(host);
  tpkborda_quadro(); assert(chamadas == 1 && ultimo == 0);              // registrou: recebe o estado atual
  chamadas = 0;
  quer = 1; tpkborda_quadro(); assert(chamadas == 1 && ultimo == 1);
  tpkborda_quadro(); tpkborda_quadro(); assert(chamadas == 1);          // so na troca
  quer = 0; tpkborda_quadro(); assert(chamadas == 2 && ultimo == 0);
  quer = 1; aberto = 0; tpkborda_quadro(); assert(chamadas == 2);       // player fechado: fica off
  aberto = 1; tpkborda_quadro(); assert(chamadas == 3 && ultimo == 1);
  aberto = 0; tpkborda_quadro(); assert(chamadas == 4 && ultimo == 0);  // fechou: off na hora
  // App-drawn subtitle text asks for it too (faint colours at low opacity).
  quer = 0; aberto = 1; app = 1; tpkborda_quadro(); assert(chamadas == 5 && ultimo == 1);
  app = 0; tpkborda_quadro(); assert(chamadas == 6 && ultimo == 0);
  aberto = 0;

  // Subtitle contrast: the TV's number 0..50 kept as number - 30 (0 = neutral
  // also off the .tpk, where assperf_get is 0); gain (c + 70) / 100.
  for (int c = 0; c <= 50; c++) {
    assperf_teste_definir(AP_CONTRASTE, c - 30);
    assert(fabs(assperf_contraste() - (c + 70) / 100.0) < 1e-9);
  }
  assperf_teste_definir(AP_CONTRASTE, 21); assert(assperf_contraste() == 1.0);   // fora: o padrao
  // Subtitle gamma: signed -20..+20, os mesmos numeros da TV, 1 - 0,0426 x n.
  for (int n = -20; n <= 20; n++) {
    assperf_teste_definir(AP_GAMA, n);
    assert(fabs(assperf_gama() - (1.0 - 0.0426 * n)) < 1e-9);
    assert(assperf_gama() > 0.0);
  }
  assperf_teste_definir(AP_GAMA, 21); assert(assperf_gama() == 1.0);
  assperf_teste_definir(AP_CONTRASTE, 0); assperf_teste_definir(AP_GAMA, 0);
  assert(assperf_contraste() == 1.0 && assperf_gama() == 1.0);           // padrao: legenda intacta
  printf("tpkborda: ok\n");
  return 0;
}
