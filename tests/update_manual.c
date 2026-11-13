// Update notifications OFF: automatic consults never open the update card,
// while the Settings "Procurar atualização" path still finds and opens it.
// Offline: the GitHub fetch is replaced by a fixture release; real ajustes.c
// supplies the default and the saved choice from a throwaway data folder.
//   bash tests/update_manual.sh
#define rede_baixar fixture_rede_baixar
#include "../src/atualizacao.c"
#undef rede_baixar
#include "dados.h"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static int fetches;
char *fixture_rede_baixar(const char *url, int segundos) {
  (void)url; (void)segundos;
  fetches++;
  return strdup("{\"tag_name\":\"v99.0.0\",\"body\":\"Fixture release\"}");
}

static void wait_consult(void) {
  for (int i = 0; i < 500; i++) {
    int busy;
    SDL_LockMutex(mtx); busy = emCurso; SDL_UnlockMutex(mtx);
    if (!busy) return;
    SDL_Delay(10);
  }
  assert(!"consult thread did not finish");
}

static void load_settings(const char *dir, const char *content) {
  char path[1200];
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  if (content) { FILE *f = fopen(path, "w"); assert(f); fputs(content, f); fclose(f); }
  else remove(path);
  ajustes_dir(dir); ajustes_iniciar();
}

// Same calls the app makes: app.c home frame (automatic) and ajustes.c
// AJ_ATUALIZAR OK + per-frame busca_achou (manual).
static void automatic_frame(void) { atualizacao_mostrar_se_houver(); }
static void settings_manual_check(void) {
  if (atualizacao_nova()[0]) atualizacao_abrir();
  else if (atualizacao_busca() != ATUALIZACAO_BUSCA_PROCURANDO) atualizacao_procurar_agora();
  wait_consult();
  if (atualizacao_busca_achou() && !atualizacao_aberta()) atualizacao_abrir();
}
static void reset_update_state(void) {
  aberto = 0; tagNova[0] = 0; pronto = 0; geracaoVista = geracao;
  busca = ATUALIZACAO_BUSCA_NADA; manualAchou = manualPendente = manualIlha = 0;
}

int main(void) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/nuvio-update-manual-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  mtx = SDL_CreateMutex(); assert(mtx);

  // Default: ON. An automatic consult that finds a version opens the card.
  load_settings(dir, NULL);
  assert(ajustes_update_notifications());
  disparar(); wait_consult();
  assert(fetches == 1 && !strcmp(atualizacao_nova(), "99.0.0"));
  automatic_frame();
  assert(atualizacao_aberta());
  assert(!atualizacao_busca_achou());          // automatic is not a manual result

  // Saved OFF (device-local key): automatic consults still run quietly, but
  // the card stays closed, across repeated frames and a newer consult.
  reset_update_state();
  load_settings(dir, "updateNotificationsLocal 1\n");
  assert(!ajustes_update_notifications());
  disparar(); wait_consult();
  assert(fetches == 2 && atualizacao_nova()[0]);
  for (int i = 0; i < 3; i++) automatic_frame();
  assert(!atualizacao_aberta());

  // OFF, version not yet known: the Settings check consults and opens it.
  reset_update_state();
  settings_manual_check();
  assert(fetches == 3 && atualizacao_busca() == ATUALIZACAO_BUSCA_NOVA);
  assert(atualizacao_aberta());

  // OFF, version already known from an automatic consult: OK opens directly.
  reset_update_state();
  disparar(); wait_consult();
  automatic_frame(); assert(!atualizacao_aberta());
  settings_manual_check();
  assert(atualizacao_aberta() && fetches == 4);

  puts("PASS: update notifications default ON; OFF suppresses the automatic card and keeps manual checks working");
  return 0;
}
