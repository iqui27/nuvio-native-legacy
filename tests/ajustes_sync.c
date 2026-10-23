// Regression: missing portable keys and a push before first cloud apply lose settings.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

#define main conta_ajustes_main
#include "conta_ajustes.c"
#undef main

static char *cloud;
static int pushes, failPull, editDuringPull;
int nuvem_configurar(const char *d) { (void)d; return 1; }
int nuvem_pronta(void) { return 1; }
const char *nuvem_url(void) { return "https://account.invalid"; }
const char *nuvem_anon(void) { return "test"; }
const char *nuvem_base_login(void) { return ""; }
const char *nuvem_trakt_cliente(void) { return ""; }
const char *nuvem_trakt_segredo(void) { return ""; }
const char *nuvem_simkl_cliente(void) { return ""; }
const char *nuvem_simkl_app(void) { return ""; }
const char *nuvem_ultimo_erro(void) { return ""; }
int nuvem_freio_ativo(void) { return 0; }
void nuvem_falhou(void) {}
void nuvem_ok(void) {}
int nuvem_erro_ausente(const char *s) { (void)s; return 0; }
void nuvem_url_escapar(const char *v, char *d, unsigned n) { snprintf(d, n, "%s", v); }
char *nuvem_post(const char *p, const char *c, const char *b, int *st) {
  (void)p; (void)c; (void)b; *st = 200; return strdup("{}");
}
char *nuvem_tabela(const char *t, const char *q, const char *b, int *st) {
  (void)t; (void)q; (void)b; *st = 200; return strdup("[]");
}
char *nuvem_addon_alternar(const char *q, int a, const char *b, unsigned p, int *st) {
  (void)q; (void)a; (void)b; (void)p; *st = 200; return strdup("[]");
}
char *nuvem_rpc_com(const char *f, const char *c, const char *b, int *st) {
  (void)b; *st = 200;
  if (!strcmp(f, "sync_pull_profiles"))
    return strdup("[{\"profile_index\":1,\"name\":\"Test\"}]");
  if (!strcmp(f, "sync_pull_profile_settings_blob")) {
    if (failPull) { *st = 503; return strdup("unavailable"); }
    if (editDuringPull) sync_proteger_ajustes_locais();
    if (!cloud) return strdup("[]");
    char *r = malloc(strlen(cloud) + 32); assert(r);
    sprintf(r, "[{\"settings_json\":%s}]", cloud); return r;
  }
  if (!strcmp(f, "sync_push_profile_settings_blob")) {
    const char *p = strstr(c, "\"p_settings_json\":"); assert(p);
    p = strchr(p, ':') + 1;
    const char *e = js_fim(p); assert(e);
    free(cloud); cloud = strndup(p, (size_t)(e - p)); pushes++;
    return strdup("null");
  }
  return strdup("[]");
}
static void login(void) {
  // Payload: {"sub":"settings-test","exp":4102444800}; no network/auth service.
  dados_gravar("sessao.txt", "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzZXR0aW5ncy10ZXN0IiwiZXhwIjo0MTAyNDQ0ODAwfQ.test\nrefresh\n0\n");
  sessao_iniciar(); assert(sessao_logada());
  sync_reaplicar_ajustes();
}
static void cycle(void) {
  sync_iniciar();
  for (int i = 0; i < 5000 && sync_estado() == SYNC_RODANDO; i++) usleep(1000);
  assert(sync_estado() == SYNC_PRONTO);
  sync_passo(SDL_GetTicks());
}
static void check(int ok, const char *name) {
  if (!ok) { fprintf(stderr, "FAIL: %s\n", name); exit(1); }
}
#include "ajustes_nuvem.c"
#ifndef SYNCJANK_TEST
int main(int argc, char **argv) {
  assert(argc == 3);
  if (!strcmp(argv[2], "device")) return recovery(argv[0], argv[1]);
  if (!strncmp(argv[2], "device-", 7)) return recoveryStage(argv[1], argv[2]);
  setenv("NUVIO_DADOS", argv[1], 1);
  dados_iniciar(NULL); player_dir(argv[1]);
  if (!strcmp(argv[2], "legacy")) {
    dados_gravar("aparencia-20.txt", "1\n");
    return conta_ajustes_main();
  }
  ajustes_dir(argv[1]); fil_definir_perfil(1);
  if (!strcmp(argv[2], "quota-restart")) {
    check(fil_limite_gravado() == FIL_LIMITE_PADRAO, "cloud device row quota survives process restart");
    return 0;
  }
  login();
  int webEdit = !strcmp(argv[2], "webedit"), quota = !strcmp(argv[2], "quota");
  if (!strcmp(argv[2], "roundtrip") || !strcmp(argv[2], "empty") || webEdit || quota) {
    if (strcmp(argv[2], "empty")) cloud = strdup("{\"version\":1,\"selected_theme\":\"WHITE\",\"features\":{\"layout_settings\":{\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":true}},\"future\":{\"untouched\":42}}}");
    cycle();
    valor[AJ_TEMA] = webEdit ? 1 : AJ_TEMA_DINAMICA;
    valor[AJ_HOME_LAYOUT] = 2; valor[AJ_IDIOMA] = 4;
    valor[AJ_HERO] = 1; valor[AJ_PULAR_ABERTURA] = 0;
    valor[AJ_LEG_LINGUA] = 3; valor[AJ_LEG2_COR] = 2;
    valor[AJ_FONTE_UI] = 1; valor[AJ_RESOLUCAO] = RES_4K;
    dados_gravar("debrid.txt", "realdebrid=SECRET-DEBRID\n");
    dados_gravar("musicas-p1.txt", "tt1160419:0\tDune OST\tHans Zimmer\n");
    dados_gravar("fanart.txt", "SECRET-FANART");
    dados_gravar("posteres.txt", "SECRET-POSTER");
    fontefalha_definir_provedores(1);
    fontefalha_provedor_definir_limite(0, 7);
    fonteregra_definir_regex("1080p|4K");
    const char *order[] = {"Addon B", "Addon A"}; fonteregra_ordem_definir(order, 2);
    fil_registrar("org.test_movie_top", "Top", "Test", "movie", 10);
    fil_registrar("org.test_series_new", "New", "Test", "series", 10);
    fil_mover(1, -1);
    fil_registrar("https://addon.invalid/SECRET-ADDON_movie_top", "Private", "Test", "movie", 10);
    fil_gravar_registro();
    player_leg_estilo_carregar();
    VideoLegendaEstilo style = *player_leg_estilo(); style.tamanho = 160;
    assert(player_leg_estilo_definir(&style, -1));
    int expected[AJ_N]; memcpy(expected, valor, sizeof expected);
    sync_proteger_ajustes_locais(); cycle();
    check(pushes == 1, "settings saved, including first blob for empty account");
    check(strstr(cloud, "untouched\":42") || !strcmp(argv[2], "empty"), "unknown cloud feature preserved");
    const char *shared, *sharedEnd;
    check(ajJsonGet(cloud, cloud + strlen(cloud), "c_settings_v1", &shared, &sharedEnd) &&
          !js_tem(shared, sharedEnd, "c_resolucaoUi"), "display override excluded from shared preferences");
    if (webEdit) {
      // Simulate web edits after the TV uploaded its C snapshot.
      char *updated = malloc(strlen(cloud) + 256); assert(updated);
      sprintf(updated, "{\"selected_theme\":\"JADE\",\"hero_section_enabled\":true,%s", cloud + 1);
      free(cloud); cloud = updated;
    }
    char *saved = strdup(cloud);
    dados_gravar("musicas-p1.txt", "");
    sessao_sair(); sync_esquecer_usuario();
    memcpy(valor, valorPadrao, sizeof valor);
    valor[AJ_TEMA] = 0; // Restore onto a fixed theme, without a local accent.
    assert(player_leg_estilo_definir(player_leg_estilo_padrao(), -1));
    fonteregra_perfil_restaurar(1);
    fontefalha_definir_provedores(0); fontefalha_provedor_definir_limite(0, 3);
    check(!strcmp(saved, cloud), "logout leaves cloud intact");
    if (quota) {
      fil_definir_limite(3);
      ajustes_aplicar_blob(cloud);
      pid_t child = fork(); assert(child >= 0);
      if (!child) { execl(argv[0], argv[0], argv[1], "quota-restart", (char *)NULL); _exit(127); }
      int status; assert(waitpid(child, &status, 0) == child);
      check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "row quota persisted before any row save");
    }
    if (webEdit) {
      valor[AJ_TEMA] = 0; // A fixed local theme accepts account theme edits.
      expected[AJ_TEMA] = 8; expected[AJ_HERO] = 0;
    }
    login(); cycle();
    char *music = dados_ler("musicas-p1.txt");
    check(music && !strcmp(music, "tt1160419:0\tDune OST\tHans Zimmer\n"), "saved music restored from profile settings");
    free(music);
    for (int i = 0; i < AJ_N; i++) if (ajustePortavel(i) && !ajLegendaPrincipal(i))
      check(valor[i] == expected[i], CHAVE[i]);
    check(valor[AJ_TEMA] == expected[AJ_TEMA] && valor[AJ_HOME_LAYOUT] == expected[AJ_HOME_LAYOUT] && valor[AJ_IDIOMA] == 4, "appearance/layout/language restored");
    check(valor[AJ_HERO] == expected[AJ_HERO] && valor[AJ_PULAR_ABERTURA] == 0, "player/home settings restored");
    check(valor[AJ_LEG_LINGUA] == 3 && valor[AJ_LEG2_COR] == 2 && player_leg_estilo()->tamanho == 160, "subtitle languages and both styles restored");
    check(valor[AJ_FONTE_UI] == 1, "interface font restored");
    check(valor[AJ_RESOLUCAO] == RES_4K, "same-device display setting restored");
    check(fontefalha_provedores() && fontefalha_provedor_limite(0) == 7, "source fallback preferences restored");
    char first[FR_NOME_MAX]; fonteregra_ordem_nome(0, first, sizeof first);
    check(fonteregra_ordem_n() == 2 && !strcmp(first, "Addon B"), "addon order restored");
    check(!strcmp(fil_chave(0), "org.test_series_new"), "home row order restored");
    check(fil_limite_gravado() == FIL_LIMITE_PADRAO, "home row quota restored from device snapshot");
    check(pushes == 1 && !strcmp(saved, cloud), "login restores without default push");
    if (webEdit) {
      sync_proteger_ajustes_locais(); cycle();
      check(strstr(cloud, "\"selected_theme\":\"JADE\"") &&
            strstr(cloud, "\"hero_section_enabled\":true"), "next TV upload preserves later web edits");
    }
    free(saved);
  } else {
    cloud = strdup("{\"features\":{\"layout_settings\":{\"hero_section_enabled\":{\"type\":\"boolean\",\"value\":false}}}}");
    char *saved = strdup(cloud);
    editDuringPull = 1; failPull = !strcmp(argv[2], "offline");
    cycle();
    check(pushes == 0 && !strcmp(saved, cloud), "fresh/default login cannot overwrite populated cloud");
    if (failPull) { failPull = 0; cycle(); }
    check(valor[AJ_HERO] == 1 && pushes == 0, "cloud applies before any push, including retry");
    free(saved);
  }
  if (!strcmp(argv[2], "roundtrip")) {
    for (int i = 0; i < AJ_N; i++)
      if (CHAVE[i] && CHAVE[i][0] != '-' &&
          (OPCOES[i].tipo == OP_ESCOLHA || OPCOES[i].tipo == OP_NUMERO))
        printf("%s: %s\n", ajustePortavel(i) ? "SYNC_KEY" : "DEVICE_KEY", CHAVE[i]);
  }
  printf("PASS: %s\n", argv[2]); free(cloud); return 0;
}
#endif
