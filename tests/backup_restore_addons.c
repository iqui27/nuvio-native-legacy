// Exercise the real app restore route and native account/add-on owners.
// Only the confirmed Drive snapshot and discovery's network-start boundary
// are controlled. Account backoff prevents any live account requests.
#define drivebackup_restauro fixture_drive_restauro
#ifndef BACKUP_RESTORE_APP
#define BACKUP_RESTORE_APP "../src/app.c"
#endif
#include BACKUP_RESTORE_APP
#undef drivebackup_restauro
#include "dados.h"
#include "contacache.h"
#include "nuvem.h"
#include <assert.h>

static BackupLocal *confirmado;
static BackupContexto contextoConfirmado;
static int prontas, prontasReabertas;
int fixture_drive_restauro(BackupContexto *c, BackupLocal **out) {
  if (!confirmado) return 0;
  *c = contextoConfirmado; *out = confirmado; confirmado = NULL; return 1;
}
void desc_sessao_preparada(void) {
  // The router must reload the saved list before discovery can use it.
  assert(addons_n() == 1 && addons_perfil_da_lista() == 1);
  prontas++;
  if (perfis_sessao_geracao() != contextoConfirmado.sessao) prontasReabertas++;
}
int main(void) {
  const char *sessao = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ1c2VyLTEiLCJleHAiOjQxMDI0NDQ4MDB9.test-signature\nfixture-refresh\n0\n";
  const char *lista = "[{\"url\":\"https://own.invalid/manifest.json\",\"name\":\"Own source\",\"enabled\":true}]";
  const char *outra = "[{\"url\":\"https://other.invalid/manifest.json\",\"name\":\"Other profile\",\"enabled\":true}]";
  dados_iniciar(getenv("NUVIO_DADOS"));
  assert(dados_gravar("sessao.txt", sessao)); sessao_iniciar();
  assert(dados_gravar("perfil.txt", "1\n")); perfis_carregar_ativo();
  ajustes_dir(dados_dir()); player_dir(dados_dir());
  perfis_ao_abrir_sessao(perfilEncerrarSessao);
  assert(contacache_gravar(CC_ADDONS_APLICADOS, 1, "user-1", lista));
  assert(contacache_gravar(CC_ADDONS_APLICADOS, 2, "user-1", outra));
  assert(contacache_gravar(CC_ADDONS_APLICADOS, 3, "other-user", outra));
  AddonRemoto addon = {0};
  snprintf(addon.url, sizeof addon.url, "%s", "https://own.invalid/manifest.json");
  snprintf(addon.nome, sizeof addon.nome, "%s", "Own source"); addon.ativo = 1;
  assert(addons_definir_lista(&addon, 1)); addons_marcar_da_conta(1);
  AddonRemoto esperado = {0}; assert(addons_exportar(&esperado, 1) == 1);
  nuvem_falhou(); assert(nuvem_freio_ativo());
  for (int vez = 0; vez < 2; vez++) {
    assert(backupapp_contexto(&contextoConfirmado));
    assert(backupapp_capturar(&contextoConfirmado, 1792330000, &confirmado));
    int antesReabrir = prontasReabertas;
    tela = TELA_AJUSTES; perfilAntes = 1;
    assert(restaurarBackupConfirmado());
    assert(!confirmado && !backupRecuperacaoPendente && tela == TELA_HOME);
    assert(addons_n() == 1 && addons_perfil_da_lista() == 1);
    // Closing Settings also syncs now; require discovery readiness after the
    // new session opens, regardless of extra valid pre-close notifications.
    assert(prontasReabertas > antesReabrir);
    AddonRemoto atual = {0}; assert(addons_exportar(&atual, 1) == 1);
    assert(!strcmp(atual.url, esperado.url) && !strcmp(atual.nome, esperado.nome));
    assert(!strcmp(sessao_usuario(), "user-1") && perfis_ativo() == 1);
    assert(perfis_sessao_geracao() != contextoConfirmado.sessao);
    char *s = dados_ler("sessao.txt"); assert(s && !strcmp(s, sessao)); free(s);
  }
  puts("PASS: repeated app restore reloads only current profile's saved addons before discovery, even during account backoff");
}
