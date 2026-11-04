// Persisted embedded subtitle numero -1 (pre-83aa66f2b PGS) vs new -2, -3 identities.
#include <stdlib.h>
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
const char *i18n(const char *s) { return s; }

#include "../src/legmemoria.c"
#include "../src/linguas.c"
#include <assert.h>
#include <string.h>

static int exata(LegMem *m, const int *nums, const char *const *idi, const char *const *nom, int n) {
  return legmem_exata(m, nums, idi, nom, n, NULL, 0);
}

int main(void) {
  LegMem m;
  int nums[8];
  const char *idi[8], *nom[8];

  memset(&m, 0, sizeof m);
  m.tipo = 'e';
  m.numero = -1;
  snprintf(m.idioma, sizeof m.idioma, "por");
  nums[0] = -2; idi[0] = "eng"; nom[0] = "eng PGS";
  nums[1] = -3; idi[1] = "por"; nom[1] = "por PGS";
  nums[2] = -4; idi[2] = "spa"; nom[2] = "spa PGS";
  assert(exata(&m, nums, idi, nom, 3) == 1);

  m.numero = -3;
  assert(exata(&m, nums, idi, nom, 3) == 1);

  m.numero = -1;
  snprintf(m.idioma, sizeof m.idioma, "eng");
  nums[0] = -2; idi[0] = "eng"; nom[0] = "eng A";
  nums[1] = -3; idi[1] = "eng"; nom[1] = "eng B";
  assert(exata(&m, nums, idi, nom, 2) == -1);

  nums[0] = 4; idi[0] = "eng"; nom[0] = "native eng";
  nums[1] = -3; idi[1] = "por"; nom[1] = "por PGS";
  nums[2] = -4; idi[2] = "spa"; nom[2] = "spa PGS";
  m.numero = -1;
  snprintf(m.idioma, sizeof m.idioma, "por");
  assert(exata(&m, nums, idi, nom, 3) == 1);

  m.numero = 5;
  snprintf(m.idioma, sizeof m.idioma, "eng");
  nums[0] = 3; idi[0] = "por"; nom[0] = "por";
  nums[1] = 5; idi[1] = "eng"; nom[1] = "eng native";
  assert(exata(&m, nums, idi, nom, 2) == 1);

  puts("legmem_pgs_legacy: saved -1, new ids, ambiguous PGS, native tracks ok");
  return 0;
}
