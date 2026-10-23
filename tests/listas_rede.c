/* Lost branch intent, on 2.1 code: a non-2xx Trakt answer must not erase the
 * Lists table. The network symbol is renamed before inclusion so the test can
 * return a 403 body where production used to read it as "zero lists". */
#define rede_baixar_st teste_rede_baixar_st
#include "../src/listas.c"
#undef rede_baixar_st
#include <assert.h>

static int status;
static const char *corpo;
char *teste_rede_baixar_st(const char *url, int segundos,
                           const char *const *cab, int *st) {
  (void)url; (void)segundos; (void)cab;
  if (st) *st = status;
  return corpo ? strdup(corpo) : NULL;
}

static void pedir(int qual, int st, const char *json) {
  ListaTarefa *t;
  status = st; corpo = json;
  t = calloc(1, sizeof *t); assert(t);
  t->qual = qual; t->sessao = perfis_sessao_geracao();
  pthread_mutex_lock(&trava); t->geracao = ++geracao; pthread_mutex_unlock(&trava);
  if (qual == PED_TRAKT_MINHAS)
    snprintf(t->aut, sizeof t->aut, "Bearer tok-teste");
  else snprintf(t->chave, sizeof t->chave, "trakt-api-key: chave-de-teste");
  fioListas(t);
}

int main(void) {
  const char *lista = "[{\"list\":{\"name\":\"De boa\",\"ids\":{\"trakt\":5001},\"user\":{\"username\":\"dono\"}}}]";
  pedir(PED_TRAKT_BUSCA, 200, lista);
  assert(nTabela == 1 && aviso[0] == 0);

  pedir(PED_TRAKT_BUSCA, 403, "[{\"error\":\"forbidden\"}]");
  assert(nTabela == 1 && aviso[0] == 0);

  pedir(PED_TRAKT_BUSCA, 200, "[]");
  assert(nTabela == 0 && !strcmp(aviso, "Nenhuma lista pública para este termo"));

  puts("lists keep rows on HTTP errors: PASS");
  return 0;
}
