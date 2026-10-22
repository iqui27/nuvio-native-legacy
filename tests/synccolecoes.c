#include <string.h>
// Exercise sync.c's actual network/copy publication with the actual collection
// parser and cache. Reuse the account/profile/app doubles from syncordem.
#define main syncordem_main
#define sessao_rpc syncordem_rpc
#define sessao_tabela syncordem_tabela
#define col_definir_json syncordem_col_stub
#define col_revisao syncordem_col_rev_stub
#define col_resposta_valida syncordem_col_valida_stub
#define colfileiras_sincronizar syncordem_colfileiras_stub
#define colfileiras_receber syncordem_colfileiras_receber_stub
#define colfileiras_contexto syncordem_colfileiras_contexto_stub
#include "syncordem.c"
#undef main
#undef sessao_rpc
#undef sessao_tabela
#undef col_definir_json
#undef col_revisao
#undef col_resposta_valida
#undef colfileiras_sincronizar
#undef colfileiras_receber
#undef colfileiras_contexto
#include "colecoes.h"
#include "fileiras.h"
#include "contacache.h"
#include <assert.h>

static int collectionsStatus = 200, outageBeforeCollections;
static const char *payload;
static const char *object = "{\"collections_json\":{\"collections\":[{\"id\":\"account-group\",\"title\":\"Account\",\"folders\":[{\"id\":\"remote-folder\",\"title\":\"Remote\",\"sources\":[{\"addonBaseUrl\":\"https://fixture.example\",\"type\":\"movie\",\"catalogId\":\"movies\"}]}]}]}}";
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
char *dados_caminho(char *dst, unsigned n, const char *nome) {
  snprintf(dst, n, "%s/%s", dados_dir(), nome); return dst;
}
char *sessao_rpc(const char *funcao, const char *corpo, int *status) {
  if (!strcmp(funcao, "sync_pull_collections")) {
    *status = collectionsStatus;
    return payload ? strdup(payload) : NULL;
  }
  return syncordem_rpc(funcao, corpo, status);
}
char *sessao_tabela(const char *table, const char *query, int *status) {
  if (outageBeforeCollections && !strcmp(table, "addons")) {
    *status = 503; return strdup("{\"error\":\"unavailable\"}");
  }
  return syncordem_tabela(table, query, status);
}
static void folder(const char *id) {
  assert(col_n() == 1 && col_folder(0));
  assert(!strcmp(col_folder(0)->id, id));
}
int main(int argc, char **argv) {
  assert(argc == 2);
  escolher(2);
  fil_definir_perfil(2);
  payload = object;
  if (!strcmp(argv[1], "network")) {
    ciclo();
    folder("remote-folder");
    char *copy = contacache_ler(CC_COLECOES, 2, sessao_usuario(), NULL);
    assert(copy && !strcmp(copy, object)); free(copy);
    assert(!contacache_ler(CC_COLECOES, 1, sessao_usuario(), NULL));
    int before = remontagens;
    unsigned rev = col_revisao();
    ciclo(); folder("remote-folder");
    assert(remontagens == before && col_revisao() == rev);
    assert(fil_estado_chave("collection_account-group") == FIL_NA_HOME);
    payload = "[]"; ciclo(); folder("remote-folder");
    assert(remontagens == before);
    copy = contacache_ler(CC_COLECOES, 2, sessao_usuario(), NULL);
    assert(copy && !strcmp(copy, object)); free(copy);
    payload = "{\"collections_json\":{\"collections\":[]";
    ciclo(); folder("remote-folder"); assert(remontagens == before);
    copy = contacache_ler(CC_COLECOES, 2, sessao_usuario(), NULL);
    assert(copy && !strcmp(copy, object)); free(copy);
    payload = "{\"collections_json\":{\"collections\":[]}}";
    ciclo(); assert(col_n() == 0);
    assert(fil_estado_chave("collection_account-group") == -1);
    assert(remontagens == before + 1);
    before = remontagens;
    payload = "{\"collections_json\":null}";
    ciclo(); assert(col_n() == 0 && remontagens == before);
  } else if (!strcmp(argv[1], "copy") || !strcmp(argv[1], "fallback")) {
    assert(contacache_gravar(CC_COLECOES, 2, sessao_usuario(), object));
    collectionsStatus = 503; payload = NULL;
    outageBeforeCollections = !strcmp(argv[1], "copy");
    ciclo(); folder("remote-folder");
    assert(sync_servidor_fora() == 503);
  } else if (!strcmp(argv[1], "isolated")) {
    assert(contacache_gravar(CC_COLECOES, 1, sessao_usuario(), object));
    assert(col_definir_json(object) == 1);
    col_esquecer_perfil(); assert(col_n() == 0);
    collectionsStatus = 503; payload = NULL; outageBeforeCollections = 1;
    ciclo(); assert(col_n() == 0);
  } else if (!strcmp(argv[1], "no-copy")) {
    assert(col_definir_json(object) == 1);
    collectionsStatus = 503; payload = NULL;
    ciclo(); folder("remote-folder");
  } else assert(0);
  puts("synccolecoes: ok");
  return 0;
}
