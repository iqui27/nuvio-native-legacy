// #468: Home follows the reorder sheet and new catalog sources without restart.
// Same focused Home fixture as colfileiras_home; no renderer, no network.
#define main cwordem_home_fixture_main
#include "cwordem_home.c"
#undef main

static Uint32 ticks = 1000;
Uint32 SDL_GetTicks(void) { return ticks; }
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
int addons_travar_lista(unsigned v, int p) { (void)v; (void)p; return 0; }
void addons_destravar_lista(void) {}
const char *sessao_usuario(void) { return "refresh-home"; }

static CatItem itens[8];
static CatFileira fils[8];
static const char *chaves[8] = {"addon_movie_a", "addon_movie_b", "addon_movie_c", "addon_movie_d",
                                "addon_movie_e", "addon_movie_f", "addon_movie_g", "addon_movie_h"};

static void publicar(int n) {
  for (int i = 0; i < n; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt468%d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "movie");
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Item %d", i);
    snprintf(fils[i].chave, sizeof fils[i].chave, "%s", chaves[i]);
    snprintf(fils[i].titulo, sizeof fils[i].titulo, "Row %d", i);
    snprintf(fils[i].base, sizeof fils[i].base, "https://addon.invalid");
    snprintf(fils[i].catId, sizeof fils[i].catId, "cat%d", i);
    snprintf(fils[i].tipo, sizeof fils[i].tipo, "movie");
    fils[i].ini = i; fils[i].n = 1;
  }
  cat_definir_tudo(itens, n, fils, n);
  sincronizarFileiras();
}
static int indiceFil(const char *chave) {
  for (int i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), chave)) return i;
  return -1;
}

int main(void) {
  fil_definir_limite(10);
  publicar(4);
  assert(nFileiras >= 4);
  // (a) Reorder in the sheet: the Home rebuilds on the next sync, no restart,
  // and the focus stays on the same row (now at its new position).
  int c0 = posicao(chaves[2]), unsigned_rev = (int)fil_revisao();
  assert(c0 >= 0);
  foco.fileira = c0; foco.coluna = 0;
  assert(fil_mover(indiceFil(chaves[2]), -1) >= 0);
  assert((int)fil_revisao() != unsigned_rev);
  sincronizarFileiras();
  assert(posicao(chaves[2]) == c0 - 1);
  assert(foco.fileira == posicao(chaves[2]));
  // Disable a row: it leaves the Home, focus still on the same row.
  foco.fileira = posicao(chaves[3]);
  fil_remover(indiceFil(chaves[0]));
  sincronizarFileiras();
  assert(!naHome(chaves[0]) && foco.fileira == posicao(chaves[3]));
  // Nothing changed: sync is a no-op (no rebuild flash).
  unsigned rev = fil_revisao();
  for (int i = 0; i < 5; i++) sincronizarFileiras();
  assert(fil_revisao() == rev);
  // (b) A catalog source registered at runtime shows in the reorder list and
  // on the Home without restart.
  int antes = fil_n();
  assert(indiceFil(chaves[4]) < 0);
  publicar(5);
  assert(fil_n() == antes + 1 && indiceFil(chaves[4]) >= 0);
  assert(naHome(chaves[4]));
  puts("ok #468: reorder/disable refresh the Home keeping focus; new source listed at runtime");
  return 0;
}
