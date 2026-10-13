// Real logout + Home preferences, with the existing offline sync/disk fixture.
#define main contaoffline_main
#include "contaoffline.c"
#undef main
#include "homefil.h"
#include "catalogo.h"

int col_n(void) { return 0; }
const ColFolder *col_folder(int i) { (void)i; return NULL; }
int cat_n_fileiras(void) { return 0; }
const CatFileira *cat_fileira(int i) { (void)i; return NULL; }
int cat_n(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
int fil_n(void) { return 0; }
const char *fil_chave(int i) { (void)i; return ""; }
void fil_remover(int i) { (void)i; }
int fil_mover(int i, int d) { (void)d; return i; }
int fil_oculta(const char *key) { (void)key; return 0; }
void fil_alternar(int i) { (void)i; }

int main(void) {
  ColFolder p = {0};
  snprintf(p.id, sizeof p.id, "account-a");
  for (int i = 2; i <= 16; i++) {
    char name[64];
    snprintf(name, sizeof name, "colfixada-p%d.txt", i);
    dados_gravar(name, "account-a\tPinned\n");
    snprintf(name, sizeof name, "ocultavistos-p%d.txt", i);
    dados_gravar(name, "cat0\n");
  }
  if (!cf_alternar(&p) || !ov_alternar("cat0")) return 2;
  sync_esquecer_usuario();
  usuario = "conta-b";  // Same profile 1; only the account changes.
  confere("account B inherits no in-memory Home preferences",
          !cf_fixada(p.id) && !ov_ativa("cat0"));
  confere("profile 1 Home preference files removed",
          !existe("colfixada.txt") && !existe("ocultavistos.txt"));
  for (int i = 2; i <= 16; i++) {
    char name[64];
    snprintf(name, sizeof name, "colfixada-p%d.txt", i);
    int absent = !existe(name);
    snprintf(name, sizeof name, "ocultavistos-p%d.txt", i);
    absent &= !existe(name);
    cf_definir_perfil(i); ov_definir_perfil(i);
    confere("other profiles retain no account A files or preferences",
            absent && !cf_fixada(p.id) && !ov_ativa("cat0"));
  }
  cf_definir_perfil(1); ov_definir_perfil(1);
  confere("reloading profile 1 does not restore account A",
          !cf_fixada(p.id) && !ov_ativa("cat0"));
  return falhas ? 1 : 0;
}
