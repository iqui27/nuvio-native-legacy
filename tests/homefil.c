// homefil: "vistos ocultos" (#366), colecao fixada e ocultar/mover por chave.
// Sem disco: dados_* em memoria.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/homefil.h"
#include "../src/fileiras.h"

static char mem[8][64], conteudo[8][4096]; static int nMem;
static int perfilAtivo = 1;
int perfis_ativo(void) { return perfilAtivo; }
char *dados_ler(const char *n) {
  for (int i = 0; i < nMem; i++) if (!strcmp(mem[i], n)) return strdup(conteudo[i]);
  return NULL;
}
int dados_gravar(const char *n, const char *c) {
  int i; for (i = 0; i < nMem && strcmp(mem[i], n); i++) {}
  if (i == nMem) { snprintf(mem[i], 64, "%s", n); nMem++; }
  snprintf(conteudo[i], 4096, "%s", c); return 1;
}
// fileiras.c nao entra: so o que homefil usa.
int fil_n(void) { return 0; }
const char *fil_chave(int i) { (void)i; return ""; }
void fil_remover(int i) { (void)i; }
int fil_mover(int i, int d) { (void)d; return i; }
// colecoes.c idem
static ColFolder pastas[2];
int col_n(void) { return 2; }
const ColFolder *col_folder(int i) { return &pastas[i]; }

int main(void) {
  unsigned r0 = ov_revisao();
  assert(!ov_ativa("addon_a"));
  assert(ov_alternar("addon_a") == 1 && ov_ativa("addon_a"));
  assert(ov_revisao() != r0);
  assert(ov_alternar("addon_b") == 1);
  assert(ov_alternar("addon_a") == 0 && !ov_ativa("addon_a") && ov_ativa("addon_b"));
  // persistiu e recarrega por perfil
  assert(strstr(conteudo[0], "addon_b"));
  perfilAtivo = 2; ov_definir_perfil(2);
  assert(!ov_ativa("addon_b"));
  perfilAtivo = 1; ov_definir_perfil(1);
  assert(ov_ativa("addon_b"));

  snprintf(pastas[0].id, sizeof pastas[0].id, "pasta1");
  snprintf(pastas[0].title, sizeof pastas[0].title, "Saga");
  assert(!cf_fixada("pasta1") && cf_n() == 0);
  assert(cf_alternar(&pastas[0]) == 1 && cf_fixada("pasta1") && cf_n() == 1);
  assert(cf_pasta(0) == &pastas[0] && !strcmp(cf_titulo(0), "Saga"));
  assert(cf_itens(&pastas[0], NULL, 0) == 0);          // ambar: sem dado
  pastas[0].id[0] = 0;                                  // conta nao a trouxe mais
  assert(cf_pasta(0) == NULL && !strcmp(cf_titulo(0), "Saga"));
  snprintf(pastas[0].id, sizeof pastas[0].id, "pasta1");
  assert(cf_alternar(&pastas[0]) == 0 && cf_n() == 0);
  assert(homefil_ocultar("x") == 0 && homefil_mover("x", 1) == 0);
  {
    const char *p[3];
    assert(homefil_origem("continue_watching", 1, 0, p) == 2 && !strcmp(p[0], "Trakt") &&
           !strcmp(p[1], "Este aparelho"));
    assert(homefil_origem("continue_watching", 0, 0, p) == 1 && !strcmp(p[0], "Este aparelho"));
    assert(homefil_origem("continue_watching", 1, 1, p) == 3);
    assert(homefil_origem("upcoming_section", 1, 1, p) == 2 && !strcmp(p[1], "Simkl"));
    assert(homefil_origem("upcoming_section", 0, 0, p) == 0 && homefil_origem("addon_a", 1, 1, p) == 0);
  }
  puts("homefil: PASS");
  return 0;
}
