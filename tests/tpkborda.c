// Provas de src/tpkborda.c (premult do GLView so enquanto o libass desenha)
// e das contas de Subtitle contrast / gamma (assperf.c). Ver tests/tpkborda.sh.
#include "../src/tpkborda.h"
#include "../src/assperf.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int quer, aberto, chamadas, ultimo = -1;
int assrender_borda_quer(void) { return quer; }
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

  // Subtitle contrast: indice 0 = 30 (1,00) .. 30 = 0 (0,70), ganho (c + 70) / 100.
  for (int v = 0; v <= 30; v++) {
    assperf_teste_definir(AP_CONTRASTE, v);
    assert(fabs(assperf_contraste() - ((30 - v) + 70) / 100.0) < 1e-9);
  }
  // Subtitle gamma: os mesmos numeros da TV, 1 - 0,0426 x n.
  { static const int n[11] = { 0, -1, -2, -3, -4, -5, 5, 4, 3, 2, 1 };
    for (int v = 0; v < 11; v++) {
      assperf_teste_definir(AP_GAMA, v);
      assert(fabs(assperf_gama() - (1.0 - 0.0426 * n[v])) < 1e-9);
    } }
  assperf_teste_definir(AP_CONTRASTE, 0); assperf_teste_definir(AP_GAMA, 0);
  assert(assperf_contraste() == 1.0 && assperf_gama() == 1.0);           // padrao: legenda intacta
  printf("tpkborda: ok\n");
  return 0;
}
