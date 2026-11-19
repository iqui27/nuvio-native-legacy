#define AJUSTES_TESTE 1
#include "../src/barraspretas.c"

static int g_aj;

char *dados_ler(const char *name) {
  (void)name;
  return NULL;
}
int dados_gravar(const char *name, const char *text) {
  (void)name; (void)text; return 0;
}
int ajustes_barras_pretas(void) { return g_aj; } /* 1 = Ligado (valor AJ 0) */

#include <stdio.h>

int main(void) {
  g_aj = 1;
  barraspretas_invalidar();
  if (!barraspretas_ativo()) { fputs("expected on\n", stderr); return 1; }
  g_aj = 0;
  barraspretas_invalidar();
  if (barraspretas_ativo()) { fputs("expected off\n", stderr); return 1; }
  barraspretas_definir(1);
  if (!barraspretas_ativo()) { fputs("expected on after definir\n", stderr); return 1; }
  puts("barraspretas: ok");
  return 0;
}
