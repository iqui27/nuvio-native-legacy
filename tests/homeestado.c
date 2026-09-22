#define _POSIX_C_SOURCE 200809L
#include "homeestado.h"
#include "ajustes.h"
#include "sessao.h"
#include "perfis.h"
#include "fileiras.h"
#include "colecoes.h"
#include "catordem.h"
#include "dados.h"
#include "addons.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

static const char *owner = "owner-A";
static int profile = 1, config = 1;
const char *sessao_usuario(void) { return owner; }
int perfis_ativo(void) { return profile; }
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_cw_ligado(void) { return 1; }
int ajustes_cw_estilo(void) { return 0; }
int ajustes_posteres_deitados(void) { return 0; }
int ajustes_rotulos_poster(void) { return 1; }
int ajustes_hero_fonte(void) { return config; }
const char *fil_hero_fonte(void) { return "auto"; }
int fil_limite(void) { return 20; }
int fil_n(void) { return 0; }
const char *fil_chave(int i) { (void)i; return ""; }
int fil_linha_oculta(int i) { (void)i; return 0; }
int fil_linha_tipo(int i) { (void)i; return 0; }
int fil_linha_tam(int i) { (void)i; return 0; }
int catordem_oculta(const char *k, const char *t) { (void)k; (void)t; return 0; }
int catordem_tem_ordem(void) { return 0; }
int catordem_n(void) { return 0; }
const char *catordem_chave(int i) { (void)i; return ""; }
int catordem_tem_ocultar_nao_lancados(void) { return 0; }
int catordem_ocultar_nao_lancados(void) { return 0; }
int catordem_tem_ocultar_sublinhado(void) { return 0; }
int catordem_ocultar_sublinhado(void) { return 0; }
int col_n(void) { return 0; }
const ColFolder *col_folder(int i) { (void)i; return NULL; }
int addons_n(void) { return 0; }
const char *addons_base(int i) { (void)i; return ""; }
int addons_tem_catalogo(int i) { (void)i; return 0; }

const char *dados_dir(void) { return getenv("HOMEESTADO_TEST_DIR"); }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", dados_dir(), nome); return dst;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char path[PATH_MAX]; FILE *f;
  dados_caminho(path, sizeof path, nome); f = fopen(path, "wb");
  if (!f) return 0;
  size_t n = strlen(conteudo); int ok = fwrite(conteudo, 1, n, f) == n;
  return fclose(f) == 0 && ok;
}
char *dados_ler(const char *nome) {
  char path[PATH_MAX]; FILE *f; long n; char *p;
  dados_caminho(path, sizeof path, nome); f = fopen(path, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f); p = malloc((size_t)n + 1);
  if (!p) { fclose(f); return NULL; }
  if (fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); fclose(f); return NULL; }
  p[n] = 0; fclose(f); return p;
}
int dados_apagar(const char *nome) { char path[PATH_MAX]; dados_caminho(path, sizeof path, nome); return unlink(path) == 0; }

static void expect(int yes, const char *what) { if (!yes) { fprintf(stderr, "FAIL: %s\n", what); exit(1); } }
static void saveRows(const char *a, const char *b) {
  CatFileira rows[2] = {0};
  snprintf(rows[0].chave, sizeof rows[0].chave, "%s", a);
  snprintf(rows[1].chave, sizeof rows[1].chave, "%s", b);
  homeestado_salvar(rows, 2);
}
static int child(const char *mode) {
  homeestado_iniciar();
  if (!strcmp(mode, "read-A")) {
    expect(homeestado_contexto_valido(), "restart loads valid persisted snapshot");
    expect(homeestado_tem_fileira("row-a") && homeestado_tem_fileira("row-b"), "restart restores exact rows");
    expect(!homeestado_tem_fileira("row-owner-b"), "owner A snapshot stays private");
  } else if (!strcmp(mode, "read-B")) {
    expect(homeestado_contexto_valido(), "owner B snapshot is independently valid");
    expect(homeestado_tem_fileira("row-owner-b"), "owner B gets its own snapshot");
    expect(!homeestado_tem_fileira("row-a"), "owner B never sees owner A rows");
  } else if (!strcmp(mode, "config-invalid")) {
    expect(!homeestado_contexto_valido(), "changed explicit config invalidates stale structure");
  } else if (!strcmp(mode, "read-profile-2")) {
    expect(homeestado_contexto_valido() && homeestado_tem_fileira("row-profile-2"),
           "profile 2 restores its own snapshot");
    expect(!homeestado_tem_fileira("row-a"), "profile 2 does not inherit profile 1 rows");
  } else return 2;
  return 0;
}
static void restart(const char *self, const char *mode, const char *newOwner,
                    const char *newConfig, const char *newProfile) {
  pid_t pid = fork(); int status;
  if (pid == 0) {
    if (newOwner) setenv("HOMEESTADO_TEST_OWNER", newOwner, 1);
    if (newConfig) setenv("HOMEESTADO_TEST_CONFIG", newConfig, 1);
    if (newProfile) setenv("HOMEESTADO_TEST_PROFILE", newProfile, 1);
    execl(self, self, mode, (char *)NULL); _exit(127);
  }
  expect(pid > 0 && waitpid(pid, &status, 0) == pid, "child process restarted");
  expect(WIFEXITED(status) && WEXITSTATUS(status) == 0, mode);
}
int main(int argc, char **argv) {
  char dir[128];
  if (argc > 1) {
    owner = getenv("HOMEESTADO_TEST_OWNER") ? getenv("HOMEESTADO_TEST_OWNER") : "owner-A";
    profile = getenv("HOMEESTADO_TEST_PROFILE") ? atoi(getenv("HOMEESTADO_TEST_PROFILE")) : 1;
    config = getenv("HOMEESTADO_TEST_CONFIG") ? atoi(getenv("HOMEESTADO_TEST_CONFIG")) : 1;
    return child(argv[1]);
  }
  snprintf(dir, sizeof dir, "/tmp/nuvio-homeestado-%ld", (long)getpid());
  expect(mkdir(dir, 0700) == 0, "create isolated storage");
  setenv("HOMEESTADO_TEST_DIR", dir, 1); owner = "owner-A"; config = 1;
  homeestado_iniciar(); expect(!homeestado_contexto_valido(), "first start has no stale rows");
  saveRows("row-a", "row-b");
  expect(homeestado_contexto_valido() && homeestado_quantidade_fileiras() == 2, "publish complete snapshot");
  restart(argv[0], "read-A", NULL, NULL, NULL);

  unsigned staleGeneration = homeestado_geracao();
  owner = "owner-B"; homeestado_geracao();
  expect(!homeestado_contexto_valido(), "owner switch clears in-memory snapshot");
  CatFileira stale[1] = {0}; snprintf(stale[0].chave, sizeof stale[0].chave, "stale-A");
  expect(!homeestado_salvar_se_geracao(stale, 1, staleGeneration),
         "late publisher cannot persist after an owner switch");
  saveRows("row-owner-b", "row-owner-b");
  restart(argv[0], "read-B", "owner-B", NULL, NULL);
  restart(argv[0], "read-A", "owner-A", NULL, NULL);

  owner = "owner-A"; homeestado_geracao();
  profile = 2; homeestado_geracao();
  expect(!homeestado_contexto_valido(), "profile switch clears in-memory snapshot");
  saveRows("row-profile-2", "row-profile-2");
  restart(argv[0], "read-profile-2", "owner-A", NULL, "2");
  profile = 1; homeestado_geracao();
  expect(!homeestado_contexto_valido(),
         "runtime switch does not treat saved profile 1 structure as live catalog identity");
  profile = 2; homeestado_geracao();
  expect(!homeestado_contexto_valido(),
         "existing destination snapshot cannot authorize previous profile live catalog");
  restart(argv[0], "read-profile-2", "owner-A", NULL, "2");
  profile = 1; homeestado_geracao();
  restart(argv[0], "read-A", "owner-A", NULL, "1");

  config = 2; homeestado_geracao();
  expect(!homeestado_contexto_valido(), "explicit configuration change invalidates old snapshot");
  restart(argv[0], "config-invalid", "owner-A", "2", NULL);
  printf("homeestado: process restart, explicit config invalidation, and owner isolation passed\n");
  return 0;
}
