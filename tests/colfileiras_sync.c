// #233: actual parser, account projection and durable local row registry.
#include "colfileiras.h"
#include "colecoes.h"
#include "catordem.h"
#include "fileiras.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *addons_base_por_id(const char *id) { (void)id; return "https://fixture.example"; }
int addons_n(void) { return 1; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return "https://fixture.example"; }
const char *addons_id_manifesto(int i) { (void)i; return "addon.demo"; }
static const char *usuario = "account-A";
const char *sessao_usuario(void) { return usuario; }
char *dados_caminho(char *dst, unsigned n, const char *nome) {
  snprintf(dst, n, "%s/%s", getenv("NV_T_DIR"), nome); return dst;
}
int dados_gravar(const char *nome, const char *texto) {
  char path[1024]; dados_caminho(path, sizeof path, nome);
  FILE *f = fopen(path, "w"); if (!f) return 0;
  int ok = fputs(texto, f) >= 0;
  return fclose(f) == 0 && ok;
}
void fil_teste_recarregar(void);

#define GRUPO(id,nome,pasta,cat) "{\"id\":\"" id "\",\"title\":\"" nome "\",\"folders\":[{\"id\":\"" pasta "\",\"title\":\"Folder\",\"sources\":[{\"addonId\":\"addon.demo\",\"addonBaseUrl\":\"https://fixture.example\",\"type\":\"movie\",\"catalogId\":\"" cat "\"}]}]}"
static const char *duas = "{\"collections\":[" GRUPO("c1","Same","f1","a") "," GRUPO("c2","Same","f2","b") "]}";
static const char *uma = "{\"collections\":[" GRUPO("c1","Renamed","f1","new_a") "]}";
static int idx(const char *chave) {
  for (int i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), chave)) return i;
  return -1;
}
static atomic_int terminar;
static void *leitor(void *arg) {
  (void)arg;
  while (!atomic_load(&terminar)) {
    const ColFolder *f = col_por_catalogo("https://fixture.example", "movie", "shared");
    assert(f && f->nSources == 1 && !strcmp(f->sources[0].catId, "shared"));
    assert(!strcmp(f->groupId, "r1") || !strcmp(f->groupId, "r2"));
    assert(!strcmp(f->id, "safe"));
  }
  return NULL;
}
int main(void) {
  fil_definir_perfil(1); fil_definir_limite(3);
  fil_registrar("continue_watching", "Continue", "", "", 2);
  fil_registrar("social_activity", "Friends", "", "", 1);
  for (int i = 1; i <= 3; i++) {
    char k[80]; snprintf(k, sizeof k, "addon.demo_movie_s%d", i);
    fil_registrar(k, k, "Addon", "movie", 5);
  }
  assert(colfileiras_receber(duas) == 2);
  colfileiras_sincronizar(); fil_normalizar();
  assert(fil_n_na_home() == 7 && fil_n_capacidade() == 3 && fil_n_fila() == 0);
  assert(fil_estado_chave("collection_c1") == FIL_NA_HOME);
  assert(fil_estado_chave("collection_c2") == FIL_NA_HOME);
  int pastas[4];
  assert(col_grupo_chave("collection_c1", pastas, 4) == 1 && pastas[0] == 0);
  assert(col_grupo_chave("collection_c2", pastas, 4) == 1 && pastas[0] == 1);
  unsigned c = col_revisao(), f = fil_revisao();
  colfileiras_receber(duas); colfileiras_sincronizar();
  assert(col_revisao() == c && fil_revisao() == f);
  assert(catordem_ler("{\"items\":[{\"is_collection\":true,\"collection_id\":\"c2\",\"order\":0,\"enabled\":false},{\"is_collection\":true,\"collection_id\":\"c1\",\"order\":1},{\"addon_id\":\"addon.demo\",\"type\":\"movie\",\"catalog_id\":\"s2\",\"enabled\":false,\"order\":2}]}"));
  colfileiras_sincronizar();
  assert(idx("collection_c2") < idx("collection_c1"));
  assert(fil_estado_chave("collection_c2") == FIL_FORA && fil_estado_chave("addon.demo_movie_s2") == FIL_FORA);
  assert(fil_n_na_home() == 5 && fil_n_capacidade() == 2);
  catordem_ler("{\"items\":[]}"); colfileiras_sincronizar();
  assert(fil_estado_chave("collection_c2") == FIL_NA_HOME);
  assert(fil_n_capacidade() == 3);

  // Local styling/visibility/order survive a remote rename and deletion.
  fil_ciclar_tipo(idx("collection_c1"));
  int estilo = fil_tipo("collection_c1");
  fil_ciclar_tipo(idx("addon.demo_movie_s1"));
  int estiloCat = fil_tipo("addon.demo_movie_s1");
  fil_remover(idx("addon.demo_movie_s2"));
  assert(colfileiras_receber(uma) == 1); colfileiras_sincronizar();
  assert(idx("collection_c2") == -1 && !strcmp(fil_titulo(idx("collection_c1")), "Renamed"));
  assert(fil_tipo("collection_c1") == estilo && fil_tipo("addon.demo_movie_s1") == estiloCat);
  assert(fil_oculta("addon.demo_movie_s2"));
  assert(fil_estado_chave("addon.demo_movie_a") == FIL_FORA && fil_oculta("addon.demo_movie_a"));
  assert(fil_estado_chave("addon.demo_movie_b") == FIL_FORA && fil_oculta("addon.demo_movie_b"));
  int estado;
  fil_adicionar(idx("addon.demo_movie_b"), &estado);
  assert(estado == FIL_NA_HOME && !fil_oculta("addon.demo_movie_b"));
  c = col_revisao(); f = fil_revisao();
  assert(!col_resposta_valida("{\"collections\":[]"));
  assert(colfileiras_receber("{\"collections\":[{\"id\":\"bad\"}]}") == 0);
  assert(col_revisao() == c && fil_revisao() == f);
  const char *invalidas[] = {
    "{\"collections\":[] garbage}",
    "{\"collections\":[],\"broken\":}",
    "{\"collections\":[],\"broken\":[}",
    "{\"collections_json\":\"{\\\"collections\\\":[],\\\"broken\\\":}\"}"
  };
  for (size_t i = 0; i < sizeof invalidas / sizeof invalidas[0]; i++) {
    assert(!col_resposta_valida(invalidas[i]));
    assert(colfileiras_receber(invalidas[i]) == 0);
    colfileiras_sincronizar();
    assert(col_n() == 1 && idx("collection_c1") >= 0);
    assert(col_revisao() == c && fil_revisao() == f);
  }
  colfileiras_receber("{\"collections\":[]}"); colfileiras_sincronizar();
  assert(col_n() == 0 && idx("collection_c1") == -1);
  assert(fil_estado_chave("addon.demo_movie_new_a") == FIL_FORA);
  assert(!fil_oculta("addon.demo_movie_b") && fil_oculta("addon.demo_movie_s2"));
  assert(fil_n_capacidade() == 3 && fil_n_na_home() == 5);
  fil_teste_recarregar();
  assert(fil_oculta("addon.demo_movie_new_a") && !fil_oculta("addon.demo_movie_b"));
  assert(fil_tipo("addon.demo_movie_s1") == estiloCat);
  colfileiras_receber(uma); colfileiras_sincronizar();
  assert(!fil_oculta("addon.demo_movie_new_a")); // re-add releases only automatic hiding
  assert(fil_oculta("addon.demo_movie_s2"));     // personal hiding remains
  colfileiras_receber("{\"collections\":[]}"); colfileiras_sincronizar();
  assert(fil_oculta("addon.demo_movie_new_a"));
  fil_definir_perfil(2); col_esquecer_perfil();
  assert(fil_n() == 0 && col_n() == 0 && !col_tem_conta());
  colfileiras_receber(duas); colfileiras_sincronizar();
  assert(fil_n() == 2 && !fil_oculta("addon.demo_movie_a"));
  fil_definir_perfil(1); assert(fil_oculta("addon.demo_movie_new_a"));
  usuario = "account-B"; colfileiras_contexto();
  assert(!fil_oculta("addon.demo_movie_new_a") && fil_oculta("addon.demo_movie_s2"));
  assert(!col_tem_conta());
  usuario = "account-A";
  fil_definir_perfil(3); col_esquecer_perfil(); catordem_esquecer();
  fil_registrar("addon.demo_movie_a", "Explicit TV row", "Addon", "movie", 4);
  fil_adicionar(idx("addon.demo_movie_a"), &estado);
  colfileiras_receber(duas); colfileiras_sincronizar();
  catordem_ler("{\"items\":[{\"addon_id\":\"addon.demo\",\"type\":\"movie\",\"catalog_id\":\"b\",\"enabled\":true}]}");
  colfileiras_receber("{\"collections\":[]}"); colfileiras_sincronizar();
  assert(!fil_oculta("addon.demo_movie_a") && !fil_oculta("addon.demo_movie_b"));
  fil_teste_recarregar();
  assert(!fil_oculta("addon.demo_movie_a")); // explicit TV choice survives restart
  puts("ok #233: menu/Home identities, empty snapshot, visibility, quota, local choices, restart and profile isolation");

  // The editor's visible neighbor skips both account-hidden rows and sources
  // represented by a collection; neither invisible row consumes a move.
  for (int tipo = 0; tipo < 2; tipo++) {
    fil_definir_perfil(4 + tipo); col_esquecer_perfil(); catordem_esquecer();
    const char *chaves[] = {"addon.demo_movie_A", "addon.demo_movie_hidden", "addon.demo_movie_B"};
    const int ocultas[] = {0, tipo == 0, 0}, agrupadas[] = {0, tipo == 1, 0};
    for (int i = 0; i < 3; i++) fil_registrar(chaves[i], chaves[i], "Addon", "movie", 4);
    fil_conta_reconciliar(chaves, ocultas, agrupadas, 3);
    assert(fil_estado_chave(chaves[1]) == FIL_FORA);
    assert(idx(chaves[0]) < idx(chaves[2]));
    int movida = fil_mover(idx(chaves[0]), 1);
    assert(movida == idx(chaves[0]));
    assert(idx(chaves[0]) > idx(chaves[2]));
    movida = fil_mover(idx(chaves[0]), -1);
    assert(movida == idx(chaves[0]));
    assert(idx(chaves[0]) < idx(chaves[2]));
    assert(fil_estado_chave(chaves[1]) == FIL_FORA);
  }
  puts("ok #233: editor reorder skips account-hidden and collection-wrapped rows in both directions");

  // Discovery cannot observe a partial builder while sync swaps snapshots.
  const char *r1 = "{\"collections\":[" GRUPO("r1","One","safe","shared") "]}";
  const char *r2 = "{\"collections\":[" GRUPO("r2","Two","safe","shared") "]}";
  col_definir_json(r1);
  pthread_t t; assert(!pthread_create(&t, NULL, leitor, NULL));
  for (int i = 0; i < 300; i++) col_definir_json(i % 2 ? r1 : r2);
  atomic_store(&terminar, 1); pthread_join(t, NULL);
  puts("ok #233: concurrent account replacement exposes complete discovery snapshots");

  // #294: A -> B -> A keeps A's own order, collections included. The Home pass
  // that runs between fil_definir_perfil and col_esquecer_perfil still sees
  // B's collections; it must not rewrite A's rows with them.
  {
    const char *colA = "{\"collections\":[" GRUPO("ga1","A one","pa1","xa1") "," GRUPO("ga2","A two","pa2","xa2") "]}";
    const char *colB = "{\"collections\":[" GRUPO("gb1","B one","pb1","xb1") "]}";
    fil_definir_perfil(10); col_esquecer_perfil(); catordem_esquecer();
    fil_registrar("continue_watching", "Continue", "", "", 2);
    for (int i = 1; i <= 3; i++) {
      char k[80]; snprintf(k, sizeof k, "addon.demo_movie_p%d", i);
      fil_registrar(k, k, "Addon", "movie", 5);
    }
    colfileiras_receber(colA); colfileiras_sincronizar();
    // Personal order: both collections on top, above "Continue".
    while (idx("collection_ga2") > 0) fil_mover(idx("collection_ga2"), -1);
    while (idx("collection_ga1") > 0) fil_mover(idx("collection_ga1"), -1);
    assert(idx("collection_ga1") == 0 && idx("collection_ga2") == 1);
    char antes[16][FIL_CHAVE]; int nAntes = fil_n();
    assert(nAntes <= 16);
    for (int i = 0; i < nAntes; i++) snprintf(antes[i], FIL_CHAVE, "%s", fil_chave(i));

    // Switch to B the way app.c does it, with a Home pass in the window.
    fil_definir_perfil(11); colfileiras_sincronizar(); col_esquecer_perfil();
    colfileiras_sincronizar();
    colfileiras_receber(colB); colfileiras_sincronizar();
    assert(idx("collection_gb1") >= 0 && idx("collection_ga1") < 0);
    fil_registrar("addon.demo_movie_onlyB", "B", "Addon", "movie", 1); // pending write

    // Back to A: Home pass while B's collections are still in memory.
    fil_definir_perfil(10);
    colfileiras_sincronizar();
    fil_gravar_registro();
    assert(fil_n() == nAntes);
    for (int i = 0; i < nAntes; i++) assert(!strcmp(fil_chave(i), antes[i]));
    col_esquecer_perfil(); colfileiras_sincronizar();
    colfileiras_receber(colA); colfileiras_sincronizar();
    assert(fil_n() == nAntes);
    for (int i = 0; i < nAntes; i++) assert(!strcmp(fil_chave(i), antes[i]));
    // And from disk, as the next boot sees it.
    fil_teste_recarregar();
    fil_definir_perfil(11); fil_definir_perfil(10);
    assert(fil_n() == nAntes && fil_tem_ordem());
    for (int i = 0; i < nAntes; i++) assert(!strcmp(fil_chave(i), antes[i]));
    // B kept its pending row: the flush went to B's file, not A's.
    fil_definir_perfil(11);
    assert(idx("addon.demo_movie_onlyB") >= 0 && idx("collection_ga1") < 0);
    puts("ok #294: profile switch A->B->A keeps A's Home order, collections included");
  }
  return 0;
}
