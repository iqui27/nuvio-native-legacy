#include "../src/biblioteca.c"
#include <assert.h>

float gfx_poster_k(void) { return 1.0f; }
float gfx_escala_tela_txt(void) { return 1.0f; }
float gfx_escala_txt(void) { return 1.0f; }
void ajustes_area_conteudo(float left, float right, float *x, float *w) {
  if (x) *x = left;
  if (w) *w = 1920 - left - right;
}
int lst_n(void) { return 0; }
void check_library_grid(int expected) {
  modo = MODO_LISTAS; temAberta = 1;
  assert(contarCelulas() == expected);
  nCelulas = contarCelulas();
  exibicao = VIS_LISTA;
  assert(nLinhas() == expected);
  exibicao = VIS_CARTAZ;
  assert(nLinhas() * colunas() >= expected);
}
