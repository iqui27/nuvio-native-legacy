#define NV_CEL_TEST_LOOPBACK
#include "../src/celular.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *i18n(const char *s) { return s; }
int ajustes_idioma(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = .9f; *g = .5f; *b = .2f; }
int ajustes_tinta_foco(void) { return 18; }

int main(void) {
  char u1[96], u2[96], t1[16], t2[16];
  int p1, p2;
  celular_relatorio_fixo("abcdhjkm", CEL_DIGITAR_PORTA);
  assert(celular_abrir_digitar("Busca"));
  snprintf(u1, sizeof u1, "%s", celular_url());
  p1 = celular_porta();
  sscanf(strrchr(u1, '/') + 1, "%8s", t1);
  celular_fechar();
  assert(celular_abrir_digitar("Busca"));
  snprintf(u2, sizeof u2, "%s", celular_url());
  p2 = celular_porta();
  sscanf(strrchr(u2, '/') + 1, "%8s", t2);
  assert(p1 == p2 && !strcmp(t1, t2) && !strcmp(t1, "abcdhjkm"));
  celular_fechar();
  puts("PASS: celular LAN fixo digitar");
  return 0;
}
