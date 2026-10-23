// Exercise the production account parser, pull merge and applied cache.
// Existing sync fixture supplies disk and addon-list doubles; no network.
#define ADDON_PULL_FIXTURE
#define main syncaddons_fixture_main
#define perfis_ativo fixture_perfis_ativo
#define perfis_ativo_addons fixture_perfis_ativo_addons
#define addons_marcar_da_conta fixture_addons_marcar_da_conta
#define addons_perfil_da_lista fixture_addons_perfil_da_lista
#include "syncaddons.c"
#undef main
#undef perfis_ativo
#undef perfis_ativo_addons
#undef addons_marcar_da_conta
#undef addons_perfil_da_lista
#include <assert.h>

static int activeProfile = 1, addonProfile = 1, listProfile = 1;
int perfis_ativo(void) { return activeProfile; }
int perfis_ativo_addons(void) { return addonProfile; }
void addons_marcar_da_conta(int perfil) { listProfile = perfil; }
int addons_perfil_da_lista(void) { return listProfile; }

#include "../src/sync.c"


static int patches, patchStatus = 200, checkEarlyPublication, stallPatch;

static int pullAndWrite(void) {
  int r = puxarAddons();
  if (r == 1) addonsEnviarToggles(addonsTogglesRem, nAddonsTogglesRem);
  return r;
}
char *nuvem_addon_alternar(const char *q, int ativo, const char *jwt, unsigned prazo_ms, int *st) {
  (void)jwt;
  assert(prazo_ms >= 1000 && prazo_ms <= 2000);
  if (stallPatch) {
    confere("priority pulls published before pending write", PRONTO_LER(estruturaCedo));
    usleep(prazo_ms * 1000);
  }
  assert(strstr(q, "user_id=eq.dono-a&profile_id=eq.1&url=eq."));
  patches++;
  if (checkEarlyPublication) {
    confere("membership published before pending PATCH", PRONTO_LER(addonsCedo));
    if (PRONTO_LER(addonsCedo)) {
      addonsAplicar();
      temAddonsRem = 0;
      PRONTO_PUBLICAR(addonsCedo, 0);
      checkEarlyPublication = 0;
    }
  }
  *st = patchStatus;
  if (*st != 200) return strdup("error");
  char l = letra(strstr(q, "url=eq.") + 8);
  for (int i = 0; conta[i]; i++)
    if ((conta[i] | 0x20) == l) conta[i] = ativo ? l & ~0x20 : l;
  return strdup("[{\"url\":\"updated\"}]");
}

static void pull(const char *json) {
  nAddonsRem = lerAddons(json);
  addonsAplicar();
}
static void expect(const char *label, const char *letters) {
  char actual[40];
  tvLetras(actual, sizeof actual);
  if (strcmp(actual, letters)) {
    fprintf(stderr, "FAIL %s: got %s, expected %s\n", label, actual, letters);
    exit(1);
  }
}
int main(int argc, char **argv) {
  // Synthetic account-table rows, with only placeholder identities/endpoints.
  // enabled:false reproduces the suspected web-default failure; other shapes exercise
  // compatibility. This is not a captured response from the reporter's account.
  static const char web[] =
    "[{\"user_id\":\"fixture\",\"profile_id\":1,\"sort_order\":0,"
      "\"name\":\"B\",\"url\":\"https://b.EXEMPLO/manifest.json\",\"enabled\":false},"
     "{\"sort_order\":1,\"name\":\"A\",\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":false},"
     "{\"sort_order\":2,\"name\":\"C\",\"url\":\"https://c.exemplo/manifest.json\"},"
     "{\"sort_order\":3,\"name\":\"D\",\"url\":\"https://d.exemplo/manifest.json\",\"enabled\":{\"stream\":false}},"
     "{\"sort_order\":4,\"name\":\"E\",\"url\":\"https://e.exemplo/manifest.json\",\"enabled\":false}]";
  snprintf(usuarioDoCiclo, sizeof usuarioDoCiclo, "conta-a");
  addonsPerfilCiclo = 1;
  copiaGeracaoDoCiclo = contacache_geracao();
  perfilDoCiclo = 1;
  if (argc > 1) {
    if (!strcmp(argv[1], "first-enable")) {
      listProfile = 0; // Production starts with package/local ownership.
      tvAcrescentar('A');
      tv[0].ativo = 0;
      tv[0].ativo = 1;
      sync_sujar_addons(tv[0].url);
      char *pending = contacache_ler(CC_ADDONS_TOGGLES, 1, usuarioDoCiclo, NULL);
      confere("first enable queued without baseline", pending && strstr(pending, "true"));
      free(pending);
      pull("[{\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":false}]");
      expect("first pull preserves local enable with owner zero", "A");
    } else if (!strcmp(argv[1], "switch-toggle")) {
      tvAcrescentar('A'); tvAcrescentar('B');
      activeProfile = addonProfile = addonsPerfilCiclo = perfilDoCiclo = 2;
      addonsRestaurar();
      tv[0].ativo = 0;
      sync_sujar_addons(tv[0].url);
      char *pending = contacache_ler(CC_ADDONS_TOGGLES, 2, usuarioDoCiclo, NULL);
      char *applied = contacache_ler(CC_ADDONS_APLICADOS, 2, usuarioDoCiclo, NULL);
      confere("foreign list cannot queue flags", !pending);
      confere("foreign list cannot seed applied cache", !applied);
      free(pending); free(applied);
    } else if (!strcmp(argv[1], "early-publish")) {
      tvAcrescentar('A');
      tv[0].ativo = 0;
      sync_sujar_addons(tv[0].url);
      nTv = 0;
      addonsRestaurar();
      snprintf(conta, sizeof conta, "ABC");
      checkEarlyPublication = 1;
      confere("pending pull completes", puxarAddons() == 1);
      confere("pull never waits for pending writes", patches == 0);
      addonsEnviarToggles(addonsTogglesRem, nAddonsTogglesRem);
      confere("pending PATCH exercised", patches == 1);
      expect("membership available during PATCH", "aBC");
      confere("pending disable reaches account", !strcmp(conta, "aBC"));
    } else if (!strcmp(argv[1], "package-restart")) {
      listProfile = 0; // Package list, no account snapshot yet.
      tvAcrescentar('A'); tvAcrescentar('B');
      tv[0].ativo = 0;
      sync_sujar_addons(tv[0].url);
      char *applied = contacache_ler(CC_ADDONS_APLICADOS, 1, usuarioDoCiclo, NULL);
      confere("package list never seeds the applied snapshot", !applied);
      free(applied);
      nTv = 0; // Restart: package list reloads with its default flags.
      tvAcrescentar('A'); tvAcrescentar('B');
      addonsRestaurar();
      pull("[{\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":true},"
            "{\"url\":\"https://b.exemplo/manifest.json\",\"enabled\":false}]");
      expect("toggle before first snapshot survives restart", "aB");
      snprintf(conta, sizeof conta, "Ab");
      confere("pull completes", puxarAddons() == 1);
      addonsEnviarToggles(addonsTogglesRem, nAddonsTogglesRem);
      confere("only the toggled row is written", patches == 1 && !strcmp(conta, "ab"));
    } else if (!strcmp(argv[1], "write-budget")) {
      tvAcrescentar('A'); tvAcrescentar('B'); tvAcrescentar('C');
      tv[0].ativo = tv[1].ativo = tv[2].ativo = 0;
      for (int i = 0; i < 3; i++) sync_sujar_addons(tv[i].url);
      snprintf(conta, sizeof conta, "ABC");
      stallPatch = 1;
      rodar(NULL);
      confere("stalled writes share one budget", patches == 1);
      char *pending = contacache_ler(CC_ADDONS_TOGGLES, 1, usuarioDoCiclo, NULL);
      confere("unfinished flags retained", pending && strstr(pending, "b.exemplo") &&
              strstr(pending, "c.exemplo") && !strstr(pending, "a.exemplo"));
      free(pending);
      stallPatch = 0;
      rodar(NULL);
      confere("next cycle drains remaining flags", patches == 3 && !strcmp(conta, "abc"));
      pending = contacache_ler(CC_ADDONS_TOGGLES, 1, usuarioDoCiclo, NULL);
      confere("successful flags acknowledged", pending && !strcmp(pending, "[]"));
      free(pending);
    } else return 2;
    return falhas ? 1 : 0;
  }
  tvAcrescentar('A'); tvAcrescentar('B'); tvAcrescentar('C');
  tvAcrescentar('D'); tvAcrescentar('X');
  // Production export returns normalized bases, not manifest endpoints.
  for (int i = 0; i < nTv; i++) {
    char base[NV_ADDON_URL_MAX];
    nv_addon_base(tv[i].url, base, sizeof base);
    snprintf(tv[i].url, sizeof tv[i].url, "%s", base);
  }
  pull(web);
  expect("web false preserves local ON; remote add/remove/order", "BACDe");
  int applied = aplicacoes;
  pull(web);
  confere("identical merged pull does not reinstall", aplicacoes == applied);
  // Explicit local OFF survives the next sync, even if remote defaults ON.
  tv[1].ativo = 0;
  pull("[{\"name\":\"A\",\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":true},"
        "{\"name\":\"B\",\"url\":\"https://b.exemplo/manifest.json\"}]");
  expect("local OFF and remote removal", "aB");
  // Missing, null, string, numeric and nested flags must not change local OFF.
  const char *flags[] = {"", ",\"enabled\":null", ",\"enabled\":\"false\"",
                         ",\"enabled\":0", ",\"enabled\":{\"stream\":true}"};
  for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
    char json[512];
    snprintf(json, sizeof json,
             "[{\"url\":\"https://a.exemplo/manifest.json\"%s},"
              "{\"url\":\"https://b.exemplo/manifest.json\",\"enabled\":false}]", flags[i]);
    pull(json);
    expect("unknown flags preserve local toggles", "aB");
  }
  char *saved = contacache_ler(CC_ADDONS_APLICADOS, 1, usuarioDoCiclo, NULL);
  assert(saved);
  nTv = 0;
  nAddonsRem = lerAddons(saved);
  addons_definir_lista(addonsRem, nAddonsRem);
  free(saved);
  expect("restart restores merged ON and explicit local OFF", "aB");
  // Removing an addon also removes its local preference; reinstall is remote.
  pull("[{\"url\":\"https://b.exemplo/manifest.json\",\"enabled\":true}]");
  pull("[{\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":true},"
        "{\"url\":\"https://b.exemplo/manifest.json\",\"enabled\":false}]");
  expect("reinstall uses remote state; remaining ON stays ON", "AB");
  // A toggle must reach the account without replacing its membership/order.
  snprintf(conta, sizeof conta, "ABZ");
  tv[0].ativo = 0;
  sync_sujar_addons(tv[0].url);
  perfilDoCiclo = 1;
  confere("toggle pull completes", pullAndWrite() == 1);
  confere("explicit disable reaches account; unrelated rows stay", !strcmp(conta, "aBZ"));
  confere("no whole-list upload", pushes == 0);
  // Seed the OFF snapshot, then enable and restart BEFORE another pull.
  addonsAplicar();
  tv[0].ativo = 1;
  sync_sujar_addons(tv[0].url);
  nTv = 0;
  addonsRestauradosPerfil = 0;
  addonsRestaurar();
  expect("restart immediately after enable", "ABZ");
  pull("[{\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":false},"
        "{\"url\":\"https://b.exemplo/manifest.json\"},"
        "{\"url\":\"https://z.exemplo/manifest.json\"}]");
  expect("web false after immediate restart preserves enable", "ABZ");
  confere("enable sync survives restart", pullAndWrite() == 1 && !strcmp(conta, "ABZ"));
  confere("only explicit flags patched", patches == 2);
  // A failed request must remain durable until a later successful sync.
  tv[0].ativo = 0;
  sync_sujar_addons(tv[0].url);
  patchStatus = 503;
  confere("failed patch does not claim success", pullAndWrite() == 1 && !strcmp(conta, "ABZ"));
  patchStatus = 200;
  confere("failed flag retried", pullAndWrite() == 1 && !strcmp(conta, "aBZ"));
  // No account snapshot exists when the first local disable is made.
  dados_apagar("conta-" CC_ADDONS_APLICADOS "-p1.json");
  contacache_gravar_geracao(CC_ADDONS_TOGGLES, 1, usuarioDoCiclo,
                            "[]", contacache_geracao());
  nTv = 0;
  tvAcrescentar('A'); tvAcrescentar('B');
  tv[0].ativo = 0;
  snprintf(conta, sizeof conta, "ABZ");
  int priorPatches = patches;
  sync_sujar_addons(tv[0].url);
  confere("disable before first applied snapshot reaches account",
          pullAndWrite() == 1 && !strcmp(conta, "aBZ") && patches == priorPatches + 1);
  addonsAplicar();
  expect("first pull preserves disabled state and remote membership", "aBZ");
  // Switch to an independent profile without an applied snapshot. Restoration
  // leaves profile 1's disabled A in memory; profile 2 must use its own flags.
  activeProfile = addonProfile = addonsPerfilCiclo = 2;
  addonsRestaurar();
  expect("uncached switch leaves previous list until pull", "aBZ");
  assert(addons_perfil_da_lista() == 1);
  pull("[{\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":true},"
        "{\"url\":\"https://b.exemplo/manifest.json\",\"enabled\":false}]");
  expect("independent profile does not inherit ON or OFF", "Ab");
  saved = contacache_ler(CC_ADDONS_APLICADOS, 2, usuarioDoCiclo, NULL);
  assert(saved);
  nTv = 0;
  nAddonsRem = lerAddons(saved);
  addons_definir_lista(addonsRem, nAddonsRem);
  free(saved);
  expect("independent profile persists its own flags", "Ab");
  // Ownership is the active profile even when addons come from primary.
  addonProfile = addonsPerfilCiclo = 1;
  addons_marcar_da_conta(activeProfile);
  pull("[{\"url\":\"https://a.exemplo/manifest.json\",\"enabled\":false},"
        "{\"url\":\"https://b.exemplo/manifest.json\",\"enabled\":true}]");
  expect("shared primary preserves current profile local flags", "Ab");
  assert(!falhas);
  puts("PASS: production addon pull preserves profile-local toggles and remote membership/order");
  return 0;
}
