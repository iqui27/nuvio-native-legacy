/* Slow optional previews must not hold the list-data queue. */
#define rede_baixar_st_retry mock_download
#include "../src/listas.c"
#undef rede_baixar_st_retry
#include <assert.h>
#include <errno.h>

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int active, started, peak, next_list, release_posters, bad_timeout;

char *mock_download(const char *url, int seconds, const char *const *headers,
                    int *status, int *retry_after) {
  (void)headers;
  if (retry_after) *retry_after = 0;
  *status = 200;
  if (strstr(url, "/items?")) {
    pthread_mutex_lock(&gate);
    if (seconds > 2) bad_timeout = 1;
    started++; active++;
    if (active > peak) peak = active;
    pthread_cond_broadcast(&changed);
    while (!release_posters) pthread_cond_wait(&changed, &gate);
    active--;
    pthread_mutex_unlock(&gate);
    return strdup("[{\"movie\":{\"ids\":{\"imdb\":\"tt0111161\"}}}]");
  }
  if (strstr(url, "/users/me/lists")) {
    pthread_mutex_lock(&gate);
    next_list = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
    return strdup("[]");
  }
  return strdup("[{\"list\":{\"name\":\"First\",\"item_count\":4,\"ids\":{\"trakt\":1}}},"
                "{\"list\":{\"name\":\"Second\",\"item_count\":4,\"ids\":{\"trakt\":2}}}]");
}

static void queue_list(int tab, int kind) {
  ListaTarefa *t = calloc(1, sizeof *t); assert(t);
  t->tab = tab; t->qual = kind; t->sessao = perfis_sessao_geracao();
  strcpy(t->chave, "trakt-api-key: fixture");
  strcpy(t->aut, "Authorization: Bearer fixture");
  pthread_mutex_lock(&trava);
  t->geracao = ++tabs[tab].geracao;
  tabs[tab].fioVivo++;
  enfileirar(t);
  pthread_mutex_unlock(&trava);
}

int main(void) {
  struct timespec deadline;
  queue_list(LST_TAB_PUBLICAS, PED_TRAKT_BUSCA);
  clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec += 2;
  pthread_mutex_lock(&gate);
  while (started < 2)
    assert(pthread_cond_timedwait(&changed, &gate, &deadline) != ETIMEDOUT);
  pthread_mutex_unlock(&gate);
  pthread_mutex_lock(&trava);
  assert(tabs[LST_TAB_PUBLICAS].n == 2 && !tabs[LST_TAB_PUBLICAS].fioVivo);
  tabs[LST_TAB_PUBLICAS].geracao++; /* Invalidate previews already in flight. */
  pthread_mutex_unlock(&trava);
  queue_list(LST_TAB_TRAKT, PED_TRAKT_MINHAS);
  clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec++;
  pthread_mutex_lock(&gate);
  while (!next_list) {
    if (pthread_cond_timedwait(&changed, &gate, &deadline) == ETIMEDOUT) break;
  }
  int progressed = next_list;
  release_posters = 1;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&gate);
  assert(progressed && "list data waited for optional collage requests");
  /* Let detached workers publish or discard their results before inspection. */
  for (int i = 0; i < 300; i++) {
    pthread_mutex_lock(&trava);
    int busy = trabVivo || capasVivas;
    pthread_mutex_unlock(&trava);
    pthread_mutex_lock(&gate);
    busy |= active;
    pthread_mutex_unlock(&gate);
    if (!busy) break;
    struct timespec pause = {0, 10000000L}; nanosleep(&pause, NULL);
  }
  assert(!bad_timeout && peak >= 2 && peak <= 10);
  pthread_mutex_lock(&trava);
  assert(!tabs[LST_TAB_PUBLICAS].v[0].capas[0][0]);
  pthread_mutex_unlock(&trava);
  puts("List data bypasses slow parallel previews; stale previews discarded: PASS");
  return 0;
}
