// Production settings and save path; only remote writes are intercepted.
#define desc_repetir bibfonte_repeat
#include "../src/ajustes.c"
#undef desc_repetir
void bibfonte_repeat(void) {}
#include "../src/contalib.h"
#include "../src/agenda.h"
int test_simkl_write(const char *, const char *, int);
#define trakt_watchlist_tipo test_trakt_write
#define simkl_lista_tipo test_simkl_write
#include "../src/ilhasalvar.c"
#undef trakt_watchlist_tipo
#undef simkl_lista_tipo
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static int traktWrites, simklWrites, failures;
int test_trakt_write(const char *id, const char *type, int add) {
  assert(!strcmp(id, "tt1234567") && !strcmp(type, "series"));
  traktWrites += add ? 1 : -1; return 1;
}
int test_simkl_write(const char *id, const char *type, int add) {
  assert(!strcmp(id, "tt1234567") && !strcmp(type, "series"));
  simklWrites += add ? 1 : -1; return 1;
}
int bibfonte_library_count(void);
int bibfonte_library_frame(int count, const char *id, const char *expectedBadge);
int bibfonte_discovery_watchlist(void);
const char *bibfonte_menu_label(int index);
void bibfonte_menu_toggle(int index);
void bibfonte_app_toggle(int index);
#define CHECK(name, condition) do { int ok = !!(condition); \
  printf("%s: %s\n", ok ? "PASS" : "FAIL", name); failures += !ok; } while (0)

void bibfonte_library_shot(void);
#include "bibfonte_shots.inc"
int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  char dir[1024], path[1100];
  CatItem ci = {0};
  snprintf(dir, sizeof dir, "%s/nuvio-bibfonte-XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  assert(!setenv("NUVIO_DADOS", dir, 1));
  assert(!SDL_Init(SDL_INIT_TIMER));
  dados_iniciar(dir); assert(!strcmp(dados_dir(), dir));
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  snprintf(path, sizeof path, "%s/simkl.txt", dir);
  FILE *f = fopen(path, "w"); assert(f); fputs("test-token\n", f); fclose(f);
  simklauth_carregar(); assert(simkl_ativo());
  strcpy(ci.imdb, "tt1234567"); strcpy(ci.tipo, "series"); strcpy(ci.titulo, "Saved series");
  cat_definir_tudo(&ci, 1, NULL, 0); assert(cat_n() == 1);
  salvos_iniciar();
  if (argc == 2) { screenshots(argv[1]); return 0; }
  dados_gravar("salvar-perguntado.txt", "1\n");
  // Restore the factory source through the actual confirmation event.
  assert(definirValorDireto(AJ_SALVOS_DEST, AJ_SALVOS_LOCAL));
  uxAbrirEditor(AJ_SALVOS_DEST);
  uxRestaurar = uxConfirmar = 1;
  uxEvento(SDLK_RETURN);
  CHECK("restore default without Trakt closes editor", !uxEditor);
#ifndef BIBFONTE_BASELINE
  CHECK("restore default without Trakt selects Simkl", ajustes_fonte_biblioteca() == AJ_SALVOS_SIMKL);
#endif
  CHECK("restored source matches effective default", !uxDiferente(AJ_SALVOS_DEST));
  uxCancelar();
  // Factory Trakt choice with only Simkl connected must save into its feed.
  valor[AJ_SALVOS_DEST] = AJ_SALVOS_TRAKT;
  CHECK("detail/context save getters follow fallback", ajustes_salvos_no_simkl() && !ajustes_salvos_no_trakt());
  ilhasalvar_executar(&ci, 1);
  CHECK("Simkl-only + reaches Simkl", simklWrites == 1 && traktWrites == 0);
  CHECK("toast names effective destination", !strcmp(ilhasalvar_destino(), i18n("Plan to Watch do Simkl")));
  CHECK("+ appears in Library", bibfonte_library_count() == 1);
  // Discovery republishes a remote snapshot, losing transient catalog marks.
  CatItem remote = ci; remote.naLista = simklWrites == 1;
#ifndef BIBFONTE_BASELINE
  remote.listaFontes = remote.naLista ? 1u << AJ_SALVOS_SIMKL : 0;
#endif
  cat_definir_tudo(&remote, 1, NULL, 0);
  CHECK("Simkl save survives discovery rebuild", bibfonte_library_count() == 1);
  CHECK("menu offers remove on active source", !strcmp(bibfonte_menu_label(0), "Remover dos Salvos"));
  ilhasalvar_executar(&ci, 0);
  CHECK("Simkl-only remove reaches Simkl", simklWrites == 0);
  trakt_definir("test-token", "test-client");
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  trakt_esquecer();
  ilhasalvar_executar(&ci, 1);
  CHECK("disconnected Trakt + falls back to Simkl", simklWrites == 1);
  ilhasalvar_executar(&ci, 0);
  // A legacy salvosDestino was a write preference, not an account-library opt-out.
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  f = fopen(path, "w"); assert(f); fputs("salvosDestino 1\n", f); fclose(f);
  ajustes_dir(dir);
  trakt_definir("test-token", "test-client");
  assert(contalib_ler_biblioteca("[{\"content_id\":\"tt7654321\",\"content_type\":\"series\",\"name\":\"Account series\"}]") == 1);
  CHECK("legacy account keeps Library source", !ajustes_salvos_no_trakt() && !ajustes_salvos_no_simkl());
  CHECK("account save getters agree with Library", !ajustes_salvos_no_trakt() && !ajustes_salvos_no_simkl());
  CHECK("Settings displays effective source", uxValorAtual(AJ_SALVOS_DEST) == AJ_SALVOS_LOCAL);
  CHECK("Quick Settings displays effective source", !strcmp(ajustes_rapido_valor(AJ_SALVOS_DEST), "Lista do Nuvio"));
#ifndef BIBFONTE_BASELINE
  CHECK("Quick Settings advances from Nuvio to Trakt", ajustes_rapido_passo(AJ_SALVOS_DEST, 1) && ajustes_fonte_biblioteca() == AJ_SALVOS_TRAKT);
  dados_gravar("ajustes.txt", "salvosDestino 1\n");
  ajustes_dir(dir);
  CHECK("Quick Settings reverses from Nuvio to Simkl", ajustes_rapido_passo(AJ_SALVOS_DEST, -1) && ajustes_fonte_biblioteca() == AJ_SALVOS_SIMKL);
  dados_gravar("ajustes.txt", "salvosDestino 1\n");
  ajustes_dir(dir);
#endif
  CHECK("legacy account reaches Library", contalib_aplicar_catalogo() == 1 && bibfonte_library_count() == 1);
  assert(definirValorDireto(AJ_SALVOS_DEST, AJ_SALVOS_TRAKT));
  CHECK("explicit Trakt choice wins over account", ajustes_salvos_no_trakt());
  ajustes_dir(dir);
  CHECK("explicit choice survives reload", ajustes_salvos_no_trakt());
  ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
  CHECK("Trakt watchlist still fetched for Agenda", bibfonte_discovery_watchlist() == 1);
  CHECK("Trakt-only title stays outside Nuvio Library", bibfonte_library_count() == 1);
  int followed = agenda_montar(), found = 0;
  for (int i = 0; i < followed; i++) if (!strcmp(agenda_lista(i)->imdb, "tt9999999")) found = 1;
  CHECK("Trakt series remains in Agenda", found);
#ifndef BIBFONTE_BASELINE
  // Merge must preserve membership even when the shared mark was already set.
  CatItem shared = *cat_item(1);
  shared.listaFontes = (1u << AJ_SALVOS_TRAKT) | (1u << AJ_SALVOS_SIMKL);
  cat_mesclar_listas(&shared, 1);
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  CHECK("merge retains overlapping sources", bibfonte_library_count() == 2);
  cat_definir_na_lista(1, 0);
  CHECK("remove preserves unrelated remote source and Agenda mark", cat_item(1)->naLista && cat_item(1)->listaFontes == (1u << AJ_SALVOS_SIMKL));
  ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
  CHECK("Nuvio item disappears after remote removal", bibfonte_library_count() == 0);
#endif
  // Both services connected; union membership must not decide the action.
  remote.naLista = 1;
#ifndef BIBFONTE_BASELINE
  remote.listaFontes = 1u << AJ_SALVOS_TRAKT;
#endif
  cat_definir_tudo(&remote, 1, NULL, 0);
  ajustes_definir_salvos_destino(AJ_SALVOS_SIMKL);
  CHECK("Trakt-only title excluded under Simkl", bibfonte_library_count() == 0);
  CHECK("Trakt-only title offers save under Simkl", !strcmp(bibfonte_menu_label(0), "Salvar"));
  int entrar = !biblioteca_na_lista(0);
  CHECK("Detail chooses add under Simkl", entrar);
  ilhasalvar_executar(cat_item(0), entrar);
  CHECK("Detail add reaches Simkl", simklWrites == 1);
  CHECK("menu offers remove after Simkl add", !strcmp(bibfonte_menu_label(0), "Remover dos Salvos"));
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  CHECK("Trakt membership survives Simkl add", biblioteca_na_lista(0));
  ajustes_definir_salvos_destino(AJ_SALVOS_SIMKL);
  ilhasalvar_executar(cat_item(0), !biblioteca_na_lista(0));
  CHECK("Detail remove reaches Simkl", simklWrites == 0);
  CHECK("Simkl remove preserves Trakt membership", cat_item(0)->naLista && !biblioteca_na_lista(0));
  CHECK("menu offers save after Simkl remove", !strcmp(bibfonte_menu_label(0), "Salvar"));
#ifndef BIBFONTE_BASELINE
  // Discovery marks only the first catalog copy with Simkl membership.
  CatItem duplicates[2] = {remote, remote};
  duplicates[0].listaFontes |= 1u << AJ_SALVOS_SIMKL;
  cat_definir_tudo(duplicates, 2, NULL, 0);
  CHECK("duplicate title appears once in Simkl Library", bibfonte_library_count() == 1);
  CHECK("Detail sees Simkl membership on every copy", biblioteca_na_lista(0) && biblioteca_na_lista(1));
  int duplicateWrites = simklWrites;
  bibfonte_app_toggle(1);
  CHECK("Detail removes duplicate from Simkl instead of adding", simklWrites == duplicateWrites - 1);
  CHECK("duplicate removal clears Simkl on every copy", !biblioteca_na_lista(0) && !biblioteca_na_lista(1) && bibfonte_library_count() == 0);
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  CHECK("duplicate removal preserves Trakt on every copy", biblioteca_na_lista(0) && biblioteca_na_lista(1));
  // Account reconciliation can precede another Home row for the same title.
  ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
  cat_definir_tudo(&ci, 1, NULL, 0);
  salvos_definir(&ci, 0);
  assert(contalib_ler_biblioteca("[{\"content_id\":\"tt1234567\",\"content_type\":\"series\",\"name\":\"Saved series\"}]") == 1);
  assert(contalib_aplicar_catalogo() == 1);
  assert(cat_acrescentar_lote(&ci, 1, NULL) == 1);
  CHECK("account-only duplicate absent from local file", !salvos_tem(ci.imdb));
  CHECK("account-only duplicate Detail reports saved", biblioteca_na_lista(1));
  CHECK("account-only duplicate menu offers remove", !strcmp(bibfonte_menu_label(1), "Remover dos Salvos"));
  bibfonte_menu_toggle(1);
  CHECK("account-only duplicate menu removes instead of adding", !salvos_tem(ci.imdb) && !biblioteca_na_lista(0) && !biblioteca_na_lista(1) && bibfonte_library_count() == 0);
  salvos_definir(&ci, 0);
  assert(contalib_ler_biblioteca("[{\"content_id\":\"tt7654321\",\"content_type\":\"series\",\"name\":\"Account series\"}]") == 1);
#endif
  // Saved sidebar must offer only titles removable from its selected source.
  SalvosEntrada sidebar[8];
  cat_definir_tudo(&ci, 1, NULL, 0);
  ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
  salvos_definir(&ci, 1);
  CHECK("sidebar includes local save under Nuvio", salvos_uniao(sidebar, 8) == 1);
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  CHECK("sidebar excludes local-only save under Trakt", salvos_uniao(sidebar, 8) == 0);
  ajustes_definir_salvos_destino(AJ_SALVOS_SIMKL);
  CHECK("sidebar excludes local-only save under Simkl", salvos_uniao(sidebar, 8) == 0);
  cat_definir_tudo(&remote, 1, NULL, 0);
  CHECK("sidebar excludes Trakt-only title under Simkl", salvos_uniao(sidebar, 8) == 0);
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  CHECK("sidebar includes selected remote title", salvos_uniao(sidebar, 8) == 1 && sidebar[0].local == -1);
  CHECK("sidebar remote title offers remove", !strcmp(bibfonte_menu_label(0), "Remover dos Salvos"));
  ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
  CHECK("sidebar preserves local save after source switches", salvos_uniao(sidebar, 8) == 1 && salvos_tem(ci.imdb));
  salvos_definir(&ci, 0);
  cat_definir_tudo(&remote, 1, NULL, 0);
#ifndef BIBFONTE_BASELINE
  // Upgraded profiles share a legacy write preference, not an explicit source.
  dados_gravar("ajustes.txt", "salvosDestino 1\n");
  dados_gravar("ajustes-p1.txt", "salvosDestino 1\n");
  dados_gravar("ajustes-p2.txt", "salvosDestino 1\n");
  ajustes_dir(dir);
  for (int profile = 1; profile <= 2; profile++) {
    ajustes_perfil_padrao();
    assert(ajustes_perfil_restaurar(profile));
    CHECK("upgraded profile initially uses Nuvio", ajustes_fonte_biblioteca() == AJ_SALVOS_LOCAL);
  }
  assert(ajustes_perfil_restaurar(1));
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  ajustes_perfil_guardar(1);
  ajustes_perfil_padrao();
  CHECK("profile reset clears explicit source", ajustes_fonte_biblioteca() == AJ_SALVOS_LOCAL);
  assert(ajustes_perfil_restaurar(2));
  CHECK("legacy B does not inherit A's explicit Trakt choice", ajustes_fonte_biblioteca() == AJ_SALVOS_LOCAL);
  CHECK("legacy B Settings displays Nuvio", uxValorAtual(AJ_SALVOS_DEST) == AJ_SALVOS_LOCAL);
  CHECK("legacy B excludes Trakt-only Library item", bibfonte_library_count() == 0);
  int beforeTrakt = traktWrites, beforeSimkl = simklWrites;
  ilhasalvar_executar(&ci, 1);
  CHECK("legacy B + stays in Nuvio", traktWrites == beforeTrakt && simklWrites == beforeSimkl && bibfonte_library_count() == 1);
  ilhasalvar_executar(&ci, 0);
  ajustes_perfil_guardar(2);
  // Reload defaults before restoring A: only its profile file can recover intent.
  dados_gravar("ajustes.txt", "salvosDestino 1\n");
  ajustes_dir(dir);
  ajustes_perfil_padrao();
  assert(ajustes_perfil_restaurar(1));
  CHECK("A restores explicit Trakt after reload", ajustes_fonte_biblioteca() == AJ_SALVOS_TRAKT);
  ajustes_definir_salvos_destino(AJ_SALVOS_SIMKL);
  ajustes_perfil_guardar(1);
  ajustes_perfil_padrao();
  assert(ajustes_perfil_restaurar(1));
  CHECK("A restores explicit Simkl with source value", ajustes_fonte_biblioteca() == AJ_SALVOS_SIMKL);
  // Direct restores also clear intent when a legacy/new profile has no marker.
  assert(ajustes_perfil_restaurar(2));
  CHECK("saved untouched B retains Nuvio precedence", ajustes_fonte_biblioteca() == AJ_SALVOS_LOCAL);
  ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
  assert(!ajustes_perfil_restaurar(3));
  CHECK("missing profile does not inherit explicit source", ajustes_fonte_biblioteca() == AJ_SALVOS_LOCAL);
  // Remote removal also deletes the local save; switching back must not revive it.
  contalib_esquecer();
  for (int source = AJ_SALVOS_TRAKT; source <= AJ_SALVOS_SIMKL; source++) {
    CatItem copies[2] = {ci, ci};
    copies[0].naLista = copies[1].naLista = 1;
    copies[0].listaFontes = copies[1].listaFontes = 7u;
    cat_definir_tudo(copies, 2, NULL, 0);
    ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
    salvos_definir(&ci, 1);
    CHECK("Nuvio title exists before remote removal", biblioteca_na_lista(0) && salvos_tem(ci.imdb));
    ajustes_definir_salvos_destino(source);
    bibfonte_app_toggle(0);
    CHECK("remote removal deletes local save", !salvos_tem(ci.imdb));
    CHECK("remote removal clears selected source", !biblioteca_na_lista(0) && !biblioteca_na_lista(1));
    ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
    salvos_reconciliar(); contalib_reconciliar();
    CHECK("remote removal leaves Nuvio Library empty", bibfonte_library_count() == 0);
    CHECK("remote removal leaves Nuvio sidebar empty", salvos_uniao(sidebar, 8) == 0);
    CHECK("remote removal clears Nuvio detail checkmark", !biblioteca_na_lista(0) && !biblioteca_na_lista(1) && !cat_imdb_na_lista_fonte(ci.imdb));
    CHECK("remote removal offers save under Nuvio", !strcmp(bibfonte_menu_label(0), "Salvar"));
    ajustes_definir_salvos_destino(source == AJ_SALVOS_TRAKT ? AJ_SALVOS_SIMKL : AJ_SALVOS_TRAKT);
    CHECK("remote removal preserves unrelated source on every copy", biblioteca_na_lista(0) && biblioteca_na_lista(1) && cat_imdb_na_lista(ci.imdb));
  }
  // #639: a complete account replacement removes old membership immediately.
  ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
  CatItem accountCopies[2] = {ci, ci};
  accountCopies[0].naLista = accountCopies[1].naLista = 1;
  accountCopies[0].listaFontes = accountCopies[1].listaFontes = 3u;
  cat_definir_tudo(accountCopies, 2, NULL, 0);
  assert(contalib_ler_biblioteca("[{\"content_id\":\"tt1234567\"}]") == 1);
  contalib_aplicar_catalogo();
  salvos_definir(&ci, 1);
  assert(contalib_ler_biblioteca("[{\"content_id\":\"tt7654321\"}]") == 1);
  contalib_aplicar_catalogo();
  salvos_reconciliar();
  CHECK("remote account removal deletes stale local mirror", !salvos_tem(ci.imdb));
  CHECK("remote account removal clears every duplicate", !cat_imdb_na_lista_na_fonte(ci.imdb, AJ_SALVOS_LOCAL));
  CHECK("remote account removal preserves Trakt membership", cat_imdb_na_lista_na_fonte(ci.imdb, AJ_SALVOS_TRAKT));
  CHECK("remote account replacement stays removed after reconciliation", (contalib_reconciliar(), !cat_imdb_na_lista_na_fonte(ci.imdb, AJ_SALVOS_LOCAL)));
  // Source transitions must update the open grid without an explicit rebuild.
  valor[AJ_IDIOMA] = IDIOMA_EN + 1;
  CatItem sources[3] = {ci, ci, ci};
  strcpy(sources[1].imdb, "tt7654321");
  for (int i = 0; i < 3; i++) {
    sources[i].naLista = 1;
    sources[i].listaFontes = 1u << (i == 1 ? AJ_SALVOS_LOCAL : AJ_SALVOS_TRAKT);
  }
  cat_definir_tudo(sources, 3, NULL, 0);
  contalib_esquecer();
  dados_gravar("ajustes.txt", "salvosDestino 1\n");
  ajustes_dir(dir);
  assert(biblioteca_iniciar());
  CHECK("open Library initially shows legacy Trakt source", bibfonte_library_frame(1, "tt1234567", "TRAKT"));
  assert(contalib_ler_biblioteca("[{\"content_id\":\"tt7654321\",\"content_type\":\"series\",\"name\":\"Account series\"}]") == 1);
  CHECK("account arrival replaces equal-count open grid", bibfonte_library_frame(1, "tt7654321", "ACCOUNT"));
  CHECK("account arrival updates duplicate membership", !biblioteca_na_lista(2) && biblioteca_na_lista(1));
  contalib_esquecer();
  CHECK("account disconnect restores legacy Trakt grid", bibfonte_library_frame(1, "tt1234567", "TRAKT"));
  trakt_esquecer();
  CHECK("Trakt disconnect fallback clears open grid and count", bibfonte_library_frame(0, NULL, "SIMKL"));
  CHECK("fallback excludes Trakt duplicate membership", !biblioteca_na_lista(2));
  trakt_definir("test-token", "test-client");
  CHECK("membership refresh also synchronizes Library source", biblioteca_na_lista(2));
  CHECK("reconnect restores open grid", bibfonte_library_frame(1, "tt1234567", "TRAKT"));
#endif
  SDL_Quit();
  printf("bibfonte: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
