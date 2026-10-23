// A PONTE DE WEBP NO VIDAA st/ (--um-fio), num Chromium de verdade e SEM
// isolamento de origem — o mesmo tests/webp_tizen.c, so que sem -pthread: o
// "pthread" de decode vira uma fibra de src/fio1.c, e quem gira as fibras e o
// laco de quadro, como em main.c. Antes da correcao de 24/09/2026 este teste
// dava "FALHOU: webp_carregar devolveu NULL": navegador_decodificar recusava
// todo pedido porque, com um fio so, tudo parece o fio principal.
//
// Roda por tests/webp-vidaa-st.sh.
#define main webp_tizen_main
#include "webp_tizen.c"
#undef main
#include "../src/fio1.h"

static void quadro(void) { fio1_rodar(8.0); }

int main(void) {
  webp_tizen_main();
  emscripten_set_main_loop(quadro, 0, 0);
  return 0;
}
