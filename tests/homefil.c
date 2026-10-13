// homefil: "vistos ocultos" (#366), colecao fixada e ocultar/mover por chave.
// Sem disco: dados_* em memoria.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/homefil.h"
#include "../src/fileiras.h"
#include "../src/perfis.h"
#include "../src/catalogo.h"

static char mem[16][64], conteudo[16][4096]; static int nMem;
static int esperaVazio;
void fil_definir_perfil(int p) { (void)p; }
void fontepref_definir_perfil(int p) { (void)p; }
void legmem_definir_perfil(int p) { (void)p; }
void arteesc_definir_perfil(int p) { (void)p; }
void plugins_perfil_mudou(void) {}
void servidores_perfil_trocou(void) {}
int ajustes_addons_do_principal(void) { return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
const char *sessao_usuario(void) { return "teste"; }
const char *nuvem_url(void) { return "https://example.invalid"; }
char *sessao_rpc(const char *f, const char *b, int *s) {
  (void)f; (void)b; if (s) *s = 0; return NULL;
}
char *nuvem_rpc_com(const char *f, const char *b, const char *t, int *s) {
  (void)t; return sessao_rpc(f, b, s);
}
int cat_n_fileiras(void) { return 0; }
const CatFileira *cat_fileira(int i) { (void)i; return NULL; }
int cat_n(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
char *dados_ler(const char *n) {
  for (int i = 0; i < nMem; i++) if (!strcmp(mem[i], n)) return strdup(conteudo[i]);
  return NULL;
}
int dados_gravar(const char *n, const char *c) {
  if (esperaVazio) assert(c[0] == '\0');
  int i; for (i = 0; i < nMem && strcmp(mem[i], n); i++) {}
  if (i == nMem) { snprintf(mem[i], 64, "%s", n); nMem++; }
  snprintf(conteudo[i], 4096, "%s", c); return 1;
}
// fileiras.c nao entra: so o que homefil usa.
int fil_n(void) { return 0; }
const char *fil_chave(int i) { (void)i; return ""; }
void fil_remover(int i) { (void)i; }
int fil_mover(int i, int d) { (void)d; return i; }
int fil_oculta(const char *chave) { (void)chave; return 0; }
void fil_alternar(int i) { (void)i; }
// colecoes.c idem
static ColFolder pastas[2];
int col_n(void) { return 2; }
const ColFolder *col_folder(int i) { return &pastas[i]; }

int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "empty")) {
    ColFolder p = {0}; snprintf(p.id, sizeof p.id, "ultima");
    assert(ov_alternar("ultima"));
    esperaVazio = 1; assert(!ov_alternar("ultima")); esperaVazio = 0;
    assert(!strcmp(conteudo[0], ""));
    assert(cf_alternar(&p));
    esperaVazio = 1; assert(!cf_alternar(&p));
    assert(!strcmp(conteudo[1], ""));
    ov_esquecer(); cf_esquecer();
    puts("homefil empty: PASS"); return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "profile")) {
    ColFolder a = {0};
    snprintf(a.id, sizeof a.id, "perfil1");
    dados_gravar("colfixada-p2.txt", "perfil2\tDois\n");
    dados_gravar("ocultavistos-p2.txt", "cat2\n");
    perfis_definir_ativo(1);
    assert(cf_alternar(&a) && ov_alternar("cat1"));
    perfis_definir_ativo(2);
    assert(!cf_fixada("perfil1") && cf_fixada("perfil2"));
    assert(!ov_ativa("cat1") && ov_ativa("cat2"));
    assert(cf_alternar(&a) && ov_alternar("cat3"));
    perfis_definir_ativo(1);
    assert(cf_fixada("perfil1") && !cf_fixada("perfil2"));
    assert(ov_ativa("cat1") && !ov_ativa("cat3"));
    perfis_definir_ativo(2);
    assert(cf_fixada("perfil1") && cf_fixada("perfil2"));
    assert(ov_ativa("cat2") && ov_ativa("cat3") && !ov_ativa("cat1"));
    puts("homefil profile: PASS"); return 0;
  }
  unsigned r0 = ov_revisao();
  assert(!ov_ativa("addon_a"));
  assert(ov_alternar("addon_a") == 1 && ov_ativa("addon_a"));
  assert(ov_revisao() != r0);
  assert(ov_alternar("addon_b") == 1);
  assert(ov_alternar("addon_a") == 0 && !ov_ativa("addon_a") && ov_ativa("addon_b"));
  // persistiu e recarrega por perfil
  assert(strstr(conteudo[0], "addon_b"));
  perfis_definir_ativo(2);
  assert(!ov_ativa("addon_b"));
  perfis_definir_ativo(1);
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
