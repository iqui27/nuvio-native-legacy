// #233: assemble the real Home after an account pull, without a renderer.
// Reuse the focused Home fixture's service doubles; no network is involved.
#define main cwordem_home_fixture_main
#include "cwordem_home.c"
#undef main
#include "../src/colfileiras.h"

static Uint32 ticks = 1000;
Uint32 SDL_GetTicks(void) { return ticks; }

int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return "account-home"; }

#define GRUPO(id,pasta) "{\"id\":\"" id "\",\"title\":\"Same title\",\"folders\":[{\"id\":\"" pasta "\",\"title\":\"Folder\",\"sources\":[{\"addonBaseUrl\":\"https://collection.invalid\",\"type\":\"movie\",\"catalogId\":\"" pasta "\"}]}]}"
static const char *duas = "{\"collections\":[" GRUPO("c1","f1") "," GRUPO("c2","f2") "]}";

int main(void) {
  fil_definir_limite(3);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");
  assert(colfileiras_receber(duas) == 2);
  colfileiras_sincronizar();
  catordem_ler("{\"items\":[{\"is_collection\":true,\"collection_id\":\"c2\",\"order\":0},{\"is_collection\":true,\"collection_id\":\"c1\",\"order\":1}]}");
  sincronizarFileiras();
  assert(nFileiras == 3 && posicao("collection_c2") < posicao("collection_c1"));
  assert(naHome("social_activity")); // synthetic app row is free
  assert(naHome("collection_c2")->n == 1 && naHome("collection_c1")->n == 1);
  assert(naHome("collection_c2")->folders[0] != naHome("collection_c1")->folders[0]);
  assert(fil_n_na_home() == nFileiras && fil_n_capacidade() == 0);

  // Changing just account visibility/order must invalidate the Home fast path.
  catordem_ler("{\"items\":[{\"is_collection\":true,\"collection_id\":\"c1\",\"enabled\":false}]}");
  sincronizarFileiras();
  assert(nFileiras == 2 && !naHome("collection_c1") && naHome("collection_c2"));
  assert(fil_estado_chave("collection_c1") == FIL_FORA && fil_n_na_home() == nFileiras);

  // A last deletion clears every collection; the synthetic app row remains.
  colfileiras_receber("{\"collections\":[]}"); colfileiras_sincronizar();
  sincronizarFileiras();
  assert(nFileiras == 1 && fil_n_na_home() == 1 && naHome("social_activity"));
  for (int i = 0; i < fil_n(); i++) assert(strncmp(fil_chave(i), "collection_", 11));
  assert(!naHome("collection_c1") && !naHome("collection_c2"));
  catordem_esquecer();

  static CatItem items[5]; static CatFileira rows[5];
  const char *keys[] = { "continue_watching", "social_activity", "addon_movie_a", "addon_movie_b", "addon_movie_c" };
  for (int i = 0; i < 5; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%d", i);
    snprintf(items[i].titulo, sizeof items[i].titulo, "Title %d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(rows[i].chave, sizeof rows[i].chave, "%s", keys[i]);
    snprintf(rows[i].titulo, sizeof rows[i].titulo, "Row %d", i);
    if (i >= 2) {
      snprintf(rows[i].base, sizeof rows[i].base, "https://addon.invalid");
      snprintf(rows[i].catId, sizeof rows[i].catId, "%c", 'a' + i - 2);
      snprintf(rows[i].tipo, sizeof rows[i].tipo, "movie");
    }
    rows[i].ini = i; rows[i].n = 1;
  }
  cat_definir_tudo(items, 5, rows, 5);
  colfileiras_receber(duas); colfileiras_sincronizar(); sincronizarFileiras();
  assert(nFileiras == 7 && fil_n_capacidade() == 3 && fil_n_na_home() == 7);
  for (int i = 0; i < 5; i++) assert(naHome(keys[i]));
  assert(naHome("collection_c1") && naHome("collection_c2"));
  assert(fil_estado_chave("https://collection.invalid_movie_f2") == FIL_FORA);
  sincronizarFileiras(); sincronizarFileiras();
  unsigned filaRev = fil_revisao(), colecaoRev = col_revisao();
  for (int i = 0; i < 10; i++) sincronizarFileiras();
  assert(fil_revisao() == filaRev && col_revisao() == colecaoRev && nFileiras == 7);
  // #392 / LOG-392-67219: saved local order, Home with only social first,
  // then CW + nine collections, then 17 catalogues published one at a time.
  colfileiras_receber("{\"collections\":[]}"); colfileiras_sincronizar();
  col_esquecer_perfil();
  fil_esquecer(); catordem_esquecer();
  fil_definir_limite(40);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true,\"homeLayout\":\"classic\"}");
  static CatItem incItems[19]; static CatFileira incRows[19];
  static char incKeys[17][64];
  for (int i = 0; i < 17; i++)
    snprintf(incKeys[i], sizeof incKeys[i], "addon_movie_late%d", i);
  for (int i = 0; i < 3; i++) fil_registrar(incKeys[i], incKeys[i], "Addon", "movie", -1);
  for (int i = 1; i <= 9; i++) {
    char key[64]; snprintf(key, sizeof key, "collection_g%d", i);
    fil_registrar(key, key, "", "", -1);
  }
  fil_registrar("continue_watching", "Continue", "", "", -1);
  for (int i = 3; i < 17; i++) fil_registrar(incKeys[i], incKeys[i], "Addon", "movie", -1);
  fil_registrar("social_activity", "Friends", "", "", -1);
  for (int i = 28; i < 766; i++) {
    char key[64]; snprintf(key, sizeof key, "addon_movie_off%d", i);
    fil_registrar_se_couber(key, key, "Addon", "movie");
  }
  assert(fil_n() == 766); // saved registry size at log:1524
  // Mark precisely this order as the person's choice, without changing it.
  assert(fil_mover(12, -1) == 11); assert(fil_mover(11, 1) == 12);
  for (int i = 0; i < 19; i++) {
    snprintf(incItems[i].imdb, sizeof incItems[i].imdb, "tt392%d", i);
    snprintf(incItems[i].tipo, sizeof incItems[i].tipo, "movie");
    snprintf(incItems[i].titulo, sizeof incItems[i].titulo, "Item %d", i);
    snprintf(incRows[i].chave, sizeof incRows[i].chave, "%s",
             i == 0 ? "continue_watching" : i == 1 ? "social_activity" : incKeys[i-2]);
    snprintf(incRows[i].titulo, sizeof incRows[i].titulo, "Row %d", i);
    incRows[i].ini = i; incRows[i].n = 1;
    if (i >= 2) {
      snprintf(incRows[i].base, sizeof incRows[i].base, "https://addon.invalid");
      snprintf(incRows[i].catId, sizeof incRows[i].catId, "late%d", i-2);
      snprintf(incRows[i].tipo, sizeof incRows[i].tipo, "movie");
    }
  }
  cat_definir_tudo(incItems, 2, incRows + 1, 1); sincronizarFileiras();
  assert(!naHome("continue_watching") && nFileiras == 1); // log: Home opens with one row
  char groups[4096] = "{\"collections\":[";
  for (int i = 1; i <= 9; i++) {
    char group[384];
    snprintf(group, sizeof group, "%s{\"id\":\"g%d\",\"title\":\"Group %d\",\"folders\":[{\"id\":\"f%d\",\"title\":\"Folder\",\"sources\":[{\"addonBaseUrl\":\"https://collection.invalid\",\"type\":\"movie\",\"catalogId\":\"f%d\"}]}]}",
             i == 1 ? "" : ",", i, i, i, i);
    strcat(groups, group);
  }
  strcat(groups, "]}"); colfileiras_receber(groups); colfileiras_sincronizar();
  for (int arrived = 0; arrived <= 17; arrived++) {
    cat_definir_tudo(incItems, arrived + 2, incRows, arrived + 2);
    sincronizarFileiras();
    assert(posicao("continue_watching") == 12);
    assert(contar("continue_watching") == 1);
    for (int i = 0; i < 3; i++) {
      const Fileira *f = naHome(incKeys[i]); assert(f);
      assert(f->n == (i < arrived ? 1 : 0));
      if (i >= arrived) assert(foco.nColunas[posicao(incKeys[i])] == 0);
    }
    if (!arrived) {
      Foco nav = foco;
      nav.fileira = posicao("collection_g1"); nav.coluna = 0;
      assert(!focus_mover(&nav, 0, -1));
      assert(nav.fileira == posicao("collection_g1"));
    }
  }
  // A nonresponding addon releases its slot with all revisions frozen.
  cat_definir_tudo(incItems, 2, incRows, 2); sincronizarFileiras();
  foco.fileira = posicao("continue_watching"); foco.coluna = 0;
  ticks += 29999; sincronizarFileiras();
  assert(posicao("continue_watching") == 12 && naHome(incKeys[0]));
  ticks++; sincronizarFileiras();
  assert(posicao("continue_watching") == 9 && !naHome(incKeys[0]));
  assert(!strcmp(fileiras[foco.fileira].chave, "continue_watching"));
  sincronizarFileiras(); assert(!naHome(incKeys[0]));
  cat_definir_tudo(incItems, 3, incRows, 3); sincronizarFileiras();
  assert(naHome(incKeys[0])->n == 1 && posicao("continue_watching") == 10);
  // Logout clears the reservation window together with the local order.
  fil_esquecer(); fil_definir_limite(40);
  for (int i = 0; i < 3; i++) fil_registrar(incKeys[i], incKeys[i], "Addon", "movie", -1);
  for (int i = 1; i <= 9; i++) {
    char key[64]; snprintf(key, sizeof key, "collection_g%d", i);
    fil_registrar(key, key, "", "", -1);
  }
  fil_registrar("continue_watching", "Continue", "", "", -1);
  assert(fil_mover(12, -1) == 11); assert(fil_mover(11, 1) == 12);
  cat_definir_tudo(incItems, 2, incRows, 2); sincronizarFileiras();
  assert(posicao("continue_watching") == 12);
  // Hidden predecessors do not reserve space; disabled CW does not leave
  // reservation rows behind.
  fil_definir_limite(3);
  fil_remover(0);
  cat_definir_tudo(incItems, 2, incRows, 2); sincronizarFileiras();
  assert(!naHome(incKeys[0]) && posicao("continue_watching") == 11);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":false}"); sincronizarFileiras();
  assert(!naHome("continue_watching") && !naHome(incKeys[1]));
  puts("ok #392: CW stays in its saved slot from its first frame through 17 arrivals");
  puts("ok #233: real Home/editor order, hidden collection, empty snapshot, equal titles and catalogue-only quota");
  return 0;
}
