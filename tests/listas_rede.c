/* Lost branch intent, on 2.1 code: a non-2xx Trakt answer must not erase the
 * Lists table, and each Lists source keeps its own rows instead of sharing one
 * table. The network symbol is renamed before inclusion so the test can return
 * a 403 body where production used to read it as "zero lists". */
#define rede_baixar_st_retry teste_rede_baixar_st_retry
#include "../src/listas.c"
#undef rede_baixar_st_retry
#include <assert.h>

static int status, dentro, maxDentro, nChamadas;
static char chamadas[8][220];
static const char *corpo;
char *teste_rede_baixar_st_retry(const char *url, int segundos,
                                 const char *const *cab, int *st, int *ra) {
  struct timespec z = { 0, 10000000L };
  (void)segundos; (void)cab;
  dentro++; if (dentro > maxDentro) maxDentro = dentro;
  if (nChamadas < 8) snprintf(chamadas[nChamadas++], sizeof chamadas[0], "%s", url);
  nanosleep(&z, NULL);
  if (st) *st = status;
  if (ra) *ra = 0;
  dentro--;
  return corpo ? strdup(corpo) : NULL;
}

static void pedir(int tab, int qual, int st, const char *json) {
  ListaTarefa *t;
  status = st; corpo = json; fonteAtual = tab;
  t = calloc(1, sizeof *t); assert(t);
  t->qual = qual; t->tab = tab; t->sessao = perfis_sessao_geracao();
  pthread_mutex_lock(&trava);
  t->geracao = ++tabs[tab].geracao;
  pthread_mutex_unlock(&trava);
  if (qual == PED_TRAKT_MINHAS)
    snprintf(t->aut, sizeof t->aut, "Bearer tok-teste");
  else snprintf(t->chave, sizeof t->chave, "trakt-api-key: chave-de-teste");
  fioListas(t);
}

static void enfileira(int tab, int qual) {
  ListaTarefa *t = calloc(1, sizeof *t);
  assert(t);
  t->qual = qual; t->tab = tab; t->sessao = perfis_sessao_geracao();
  t->geracao = ++tabs[tab].geracao;
  if (qual == PED_TRAKT_MINHAS) snprintf(t->aut, sizeof t->aut, "Bearer tok-teste");
  else snprintf(t->chave, sizeof t->chave, "trakt-api-key: chave-de-teste");
  pthread_mutex_lock(&trava);
  tabs[tab].fioVivo++;
  enfileirar(t);
  pthread_mutex_unlock(&trava);
}

int main(void) {
  const char *lista = "[{\"list\":{\"name\":\"De boa\",\"ids\":{\"trakt\":5001},\"user\":{\"username\":\"dono\"}}}]";
  pedir(LST_TAB_PUBLICAS, PED_TRAKT_BUSCA, 200, lista);
  assert(tabs[LST_TAB_PUBLICAS].n == 1 && tabs[LST_TAB_PUBLICAS].aviso[0] == 0);

  pedir(LST_TAB_PUBLICAS, PED_TRAKT_BUSCA, 403, "[{\"error\":\"forbidden\"}]");
  assert(tabs[LST_TAB_PUBLICAS].n == 1 && tabs[LST_TAB_PUBLICAS].aviso[0] == 0);

  pedir(LST_TAB_PUBLICAS, PED_TRAKT_BUSCA, 200, "[]");
  assert(tabs[LST_TAB_PUBLICAS].n == 0 &&
         !strcmp(tabs[LST_TAB_PUBLICAS].aviso, "Nenhuma lista pública para este termo"));

  /* A late or failed request in one source cannot erase another source. */
  pedir(LST_TAB_PUBLICAS, PED_TRAKT_BUSCA, 200, lista);
  fonteAtual = LST_TAB_TRAKT;
  pedir(LST_TAB_TRAKT, PED_TRAKT_MINHAS, 403, "[{\"error\":\"forbidden\"}]");
  assert(tabs[LST_TAB_PUBLICAS].n == 1);
  assert(lst_n() == 0);
  fonteAtual = LST_TAB_PUBLICAS;
  assert(lst_n() == 1 && !strcmp(lst_lista(0)->titulo, "De boa"));

  /* The one paced worker drains sources in FIFO order, never in parallel. */
  nChamadas = maxDentro = 0; status = 200; corpo = lista;
  enfileira(LST_TAB_TRAKT, PED_TRAKT_MINHAS);
  enfileira(LST_TAB_PUBLICAS, PED_TRAKT_BUSCA);
  for (int i = 0; i < 300; i++) {
    int pendentes;
    pthread_mutex_lock(&trava);
    pendentes = tabs[LST_TAB_TRAKT].fioVivo + tabs[LST_TAB_PUBLICAS].fioVivo;
    pthread_mutex_unlock(&trava);
    if (!pendentes) break;
    { struct timespec z = { 0, 10000000L }; nanosleep(&z, NULL); }
  }
  assert(!tabs[LST_TAB_TRAKT].fioVivo && !tabs[LST_TAB_PUBLICAS].fioVivo);
  assert(maxDentro == 1 && nChamadas == 2);
  assert(strstr(chamadas[0], "/users/me/lists") && strstr(chamadas[1], "/lists/trending"));

  puts("lists keep per-source rows over one paced queue: PASS");
  return 0;
}
