#define main home21_main
#include "home21_shot.c"
#undef main
#include "addonguia.h"
#include "addons.h"
#include "perfis.h"
#include "sessao.h"
#include <sys/wait.h>
#include <unistd.h>

static void installed(void) {
  AddonRemoto a[3] = {{"Direct", "https://direct.invalid/manifest.json", 1},
    {"Torrent", "https://torrent.invalid/manifest.json", 1},
    {"Catalog", "https://catalog.invalid/manifest.json", 0}};
  assert(addons_definir_lista(a, 3));
  for (int i = 0; i < 3; i++)
    addons_manifesto_lido(i, i == 2 ? "{\"id\":\"catalog\",\"resources\":[\"catalog\"]}"
      : "{\"id\":\"stream\",\"resources\":[\"stream\"]}");
}
static void load(void) {
  addonmodo_carregar(perfis_dono()[0] ? perfis_dono() : sessao_usuario(), perfis_ativo_addons());
}
static void scenario(int which) {
  char dir[] = "/tmp/nuvio-c4-test.XXXXXX";
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  if (which != 1) installed();
  load();
  if (which == 1) installed();
  if (which < 2) {
    int openings = 0;
    for (int i = 0; i < 3; i++) {
      addonguia_atualizar(1);
      openings += addonguia_aberto();
      addonguia_fechar();
    }
    assert(openings <= 1);
    for (int i = 0; i < 3; i++) assert(addonmodo_visto(addonmodo_id(addons_base(i))));
    load();
    for (int i = 0; i < 3; i++) assert(addonmodo_visto(addonmodo_id(addons_base(i))));
  } else {
    /* An existing modes file must preserve unseen addons. */
    assert(addonmodo_set(addonmodo_id(addons_base(0)), AM_AUTO));
    load();
    /* Use new identities after migration, as for newly installed addons. */
    AddonRemoto a[3] = {{"New direct", "https://new-direct.invalid/manifest.json", 1},
      {"New torrent", "https://new-torrent.invalid/manifest.json", 1},
      {"New catalog", "https://new-catalog.invalid/manifest.json", 1}};
    assert(addons_definir_lista(a, 3));
    for (int i = 0; i < 3; i++) addons_manifesto_lido(i, "{\"id\":\"stream\",\"resources\":[\"stream\"]}");
    uint64_t id = addonmodo_id(addons_base(0));
    if (which == 2) {
      addonmodo_resposta(id, -1, 0, 0, 0);
      addonguia_atualizar(1);
      assert(!addonguia_aberto() && !addonmodo_visto(id));
      addonmodo_resposta(id, 200, 1, 2, 0);
      addonguia_atualizar(1);
      assert(addonguia_aberto() && addonguia_shot_tela() == AG_NOVO_DIRETO);
    } else {
      for (int i = 0; i < 3; i++) addonmodo_resposta(addonmodo_id(addons_base(i)), 200, 1, 0, 2);
      int openings = 0;
      for (int i = 0; i < 3; i++) {
        addonguia_atualizar(1);
        openings += addonguia_aberto();
        if (addonguia_aberto()) assert(addonguia_shot_tela() == AG_NOVO_TORRENT);
        addonguia_fechar();
      }
      assert(openings == 1);
    }
  }
}
int main(int argc, char **argv) {
  int failed = 0;
  for (int i = 0; i < 4; i++) {
    pid_t p = fork(); assert(p >= 0);
    if (!p) { scenario(i); _exit(0); }
    int status; assert(waitpid(p, &status, 0) == p);
    printf("C4 scenario %d: %s\n", i, status ? "FAIL" : "PASS");
    failed |= status != 0;
  }
  if (failed) return 1;
  /* Same upgrade state, rendered through the existing real Home fixture. */
  dados_iniciar(getenv("NUVIO_DADOS"));
  installed(); load();
  for (int i = 0; i < 3; i++) { addonguia_atualizar(1); assert(!addonguia_aberto()); }
  return home21_main(argc, argv);
}
