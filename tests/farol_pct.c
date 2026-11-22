// Aridade de modelo (issue 39, secao 4.2): fp_modelo_args conta os
// marcadores printf do literal FT_* — "%d"/"%s" contam e "%%" e '%'
// literal, nao conta. Inclui o .c para alcancar a funcao estatica;
// compila e roda dentro de tests/farol_peca.sh, que ja junta
// faroltexto.c e js.c para as dependencias externas.
#include "../src/farolpeca.c"
#include <stdio.h>
#include <stdlib.h>

static int t_falhas;
#define CHECK(c) do { if (!(c)) { \
    printf("FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); \
    if (++t_falhas > 20) abort(); } } while (0)

// Contagem independente de marcadores: "%%" pula, qualquer outro "%" e
// marcador (o literal so usa %d/%s). Serve de segunda opiniao para a
// tabela FT_* inteira, nao so para os casos dirigidos.
static int t_indep(const char *s) {
  int n = 0;
  while (*s) {
    if (*s == '%') {
      s++;
      if (*s == '%') { s++; continue; }
      while (*s && !strchr("diuscxXfg", *s)) s++;
      n++;
      if (*s) s++;
    } else s++;
  }
  return n;
}

int main(void) {
  static const char *const CASOS[] = {
    "100%% certo", "%d%% de %s", "%%%s", "%%", "%s%%", "%%%%",
    "%d e %s", "sem nada", "%% %% %d", "fim %", NULL
  };
  for (int i = 0; CASOS[i]; i++) {
    int a = fp_modelo_args(CASOS[i]), b = t_indep(CASOS[i]);
    printf("%-14s fp_modelo_args=%d independente=%d%s\n",
           CASOS[i], a, b, a == b ? "" : "  <-- DIFERE");
    CHECK(a == b);
  }
  for (size_t i = 0; i < FP_MODELOS_N; i++) {
    CHECK(fp_modelo_args(FP_MODELOS[i].lit) == t_indep(FP_MODELOS[i].lit));
    CHECK(fp_modelo_args(FP_MODELOS[i].lit) <= FAROL_ARGS);
  }
  printf("farol_pct: %zu frases conferidas\n", (size_t)FP_MODELOS_N);
  if (t_falhas) { printf("farol_pct: %d FALHAS\n", t_falhas); return 1; }
  printf("farol_pct: tudo ok\n");
  return 0;
}
