// #392: switching A -> B -> A must give A back the exact Home row order it had.
// In the window after the switch the discovery pass still holds the PREVIOUS
// profile's addon list (the account answers a few seconds later). Registering
// those catalogs into the new profile's full file evicted A's own rows, and
// they came back at the end ("collections, catalogs and even Continue Watching
// rearranged"). The discovery pass asks fil_lista_e_deste_perfil() first.
#include "fileiras.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *addons_base_por_id(const char *id) { (void)id; return ""; }
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return "account-A"; }
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

static char antes[FIL_MAX][96];
static int nAntes;
static void guardar(void) {
  nAntes = fil_n();
  for (int i = 0; i < nAntes; i++) snprintf(antes[i], sizeof antes[i], "%s", fil_chave(i));
}
static int tagLista;   // what addons_marcar_da_conta would have recorded
int addons_perfil_da_lista(void) { return tagLista; }
// Writers of the real path: the discovery pass, the home drawer and the
// beyond-quota listing all end in these two functions.
static void passada(const char *prefixo) {
  for (int i = 0; i < 40; i++) {
    char k[96]; snprintf(k, sizeof k, "%s_movie_new%d", prefixo, i);
    if (i % 2) fil_registrar(k, k, "Addon", "movie", -1);
    else fil_registrar_se_couber(k, k, "Addon", "movie");
  }
  fil_gravar_registro();
}
int main(void) {
  fil_definir_perfil(1);
  fil_registrar("continue_watching", "Continue", "", "", 2);
  fil_registrar("social_activity", "Friends", "", "", 1);
  for (int i = 0; i < FIL_MAX - 2; i++) {
    char k[96]; snprintf(k, sizeof k, "addonA_movie_c%03d", i);
    fil_registrar(k, k, "AddonA", "movie", 5);
  }
  fil_gravar_registro();
  assert(fil_n() == FIL_MAX);
  guardar();

  tagLista = 1;
  fil_definir_perfil(2);                 // A -> B
  tagLista = 2; passada("addonB");       // B's own list
  fil_definir_perfil(1);                 // B -> A, account of A not answered yet
  fil_n();                               // loads A's file from disk
  passada("addonB");                     // stale writers: tag still says B
  { // the Home mirror (home.c) draws the rows built from that stale list
    static char ks[40][96]; const char *ch[40];
    for (int i = 0; i < 40; i++) { snprintf(ks[i], sizeof ks[i], "addonB_movie_new%d", i); ch[i] = ks[i]; }
    fil_espelhar_ordem(ch, ch, 40);
  }
  fil_gravar_registro();
  fil_teste_recarregar();

  assert(fil_n() == nAntes);
  for (int i = 0; i < nAntes; i++) assert(!strcmp(fil_chave(i), antes[i]));
  // App and collection rows do not depend on the list and still register.
  fil_registrar("collection_x", "X", "", "", 1);
  assert(fil_n() == nAntes || fil_n() == FIL_MAX);

  // Same list arrives (addons_definir_lista == 0): the tag flips to A and the
  // repeated pass registers normally (the repeat is what sync.c now forces).
  tagLista = 1; passada("addonA2");
  fil_teste_recarregar();
  int achou = 0;
  for (int i = 0; i < fil_n(); i++) if (strstr(fil_chave(i), "addonA2_")) achou = 1;
  assert(achou);
  // P2: a pass validated under profile A is refused atomically after the
  // switch to B even though the tag now equals the profile (reasoned, not run
  // concurrently: this is the sequential form of the race).
  FilPassada velha = fil_passada_ler();
  assert(fil_passada_valida(&velha));
  fil_definir_perfil(2); tagLista = 2;
  assert(!fil_passada_valida(&velha));
  fil_registrar_de(&velha, "addonA_movie_stale", "s", "Addon", "movie", -1);
  fil_registrar_se_couber_de(&velha, "addonA_movie_stale2", "s", "Addon", "movie");
  for (int i = 0; i < fil_n(); i++) assert(!strstr(fil_chave(i), "_stale"));
  FilPassada nova = fil_passada_ler();
  fil_registrar_de(&nova, "addonB_movie_fresh", "f", "Addon", "movie", -1);
  { int ok = 0; for (int i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), "addonB_movie_fresh")) ok = 1; assert(ok); }

  // P2 empty answer: profile B just entered, tag still A, the account answered
  // empty and sync marks the kept list "in use" for B: catalogs register.
  fil_definir_perfil(3); tagLista = 2;   // list tagged for another profile
  for (int i = 0; i < 40; i++) {
    char k[96]; snprintf(k, sizeof k, "addonK_movie_m%d", i);
    fil_registrar_se_couber(k, k, "Addon", "movie");
  }
  assert(fil_n() == 0);
  tagLista = 3;                          // addons_marcar_em_uso(3)
  for (int i = 0; i < 40; i++) {
    char k[96]; snprintf(k, sizeof k, "addonK_movie_m%d", i);
    fil_registrar_se_couber(k, k, "Addon", "movie");
  }
  assert(fil_n() == 40);
  // Round 4 / P1: B at the cap, memory holds A's list (tag A), the account
  // answered empty: nothing new may be registered, and B's real list later
  // leaves every key and position as it was.
  fil_definir_perfil(7); tagLista = 7;
  static char b7[FIL_MAX][96]; 
  for (int i = 0; i < FIL_MAX; i++) {
    snprintf(b7[i], sizeof b7[i], "addonB7_movie_o%03d", i);
    fil_registrar(b7[i], b7[i], "B", "movie", 3);
  }
  fil_gravar_registro(); fil_teste_recarregar();
  tagLista = 1;                          // memory still holds A's list
  passada("addonA7");
  { static char ks[40][96]; const char *ch[40];
    for (int i = 0; i < 40; i++) { snprintf(ks[i], sizeof ks[i], "addonA7_movie_new%d", i); ch[i] = ks[i]; }
    fil_espelhar_ordem(ch, ch, 40); }
  fil_gravar_registro(); fil_teste_recarregar();
  tagLista = 7;                          // B's real list arrives
  for (int i = 0; i < FIL_MAX; i++) fil_registrar(b7[i], b7[i], "B", "movie", 3);
  fil_gravar_registro(); fil_teste_recarregar();
  assert(fil_n() == FIL_MAX);
  for (int i = 0; i < FIL_MAX; i++) assert(!strcmp(fil_chave(i), b7[i]));

  // Round 4 / P2: a stale snapshot changes nothing, not even existing keys.
  fil_definir_perfil(5); tagLista = 5;
  FilPassada p5 = fil_passada_ler();
  fil_registrar_se_couber_de(&p5, "addonS_movie_sug", "s", "S", "movie");
  fil_gravar_registro();
  int e0 = fil_estado_chave("addonS_movie_sug");   // a suggestion
  assert(e0 >= 0);
  fil_definir_perfil(4); tagLista = 4;
  FilPassada p4 = fil_passada_ler();
  fil_definir_perfil(5); tagLista = 5;
  fil_registrar_de(&p4, "addonS_movie_sug", "s", "S", "movie", -1);
  assert(fil_estado_chave("addonS_movie_sug") == e0);   // not promoted by the old pass
  fil_registrar_de(&p5, "addonS_movie_sug", "s", "S", "movie", -1);
  assert(fil_estado_chave("addonS_movie_sug") == e0);   // p5 is stale too (switched away)
  FilPassada p5b = fil_passada_ler();
  fil_registrar_de(&p5b, "addonS_movie_sug", "s", "S", "movie", -1);
  assert(fil_estado_chave("addonS_movie_sug") != e0);   // current pass promotes it

  // Round 4 / P2: logout, then selecting the SAME profile again, invalidates
  // snapshots; tag 0 after logout must not validate an old one.
  fil_definir_perfil(6); tagLista = 6;
  FilPassada p6 = fil_passada_ler();
  fil_esquecer(); tagLista = 0;
  fil_definir_perfil(6);
  fil_registrar_de(&p6, "addonL_movie_old", "o", "L", "movie", -1);
  assert(fil_n() == 0 && !fil_passada_valida(&p6));
  fil_teste_recarregar();
  assert(fil_n() == 0);
  printf("ordemperfil ok\n");
  return 0;
}
