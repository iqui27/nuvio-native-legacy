// Exercise the production URL/thread path; only disk, HTTP and clock are stubbed.
#include <assert.h>
#include <unistd.h>
#include "../src/curadoria.c"

static pthread_mutex_t network = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int entered, release_fetch;
static char saved[300] = " https://example.test/curation\n";
static char requested[300];
static Uint32 ticks = 100;

char *dados_ler(const char *name) { (void)name; return strdup(saved); }
int dados_gravar(const char *name, const char *value) {
  (void)name;
  snprintf(saved, sizeof saved, "%s", value);
  return 1;
}
int dados_apagar(const char *name) { (void)name; saved[0] = 0; return 1; }
Uint32 SDL_GetTicks(void) { return ticks; }
char *rede_baixar_com(const char *address, int timeout, const char *const *headers) {
  (void)timeout; (void)headers;
  pthread_mutex_lock(&network);
  snprintf(requested, sizeof requested, "%s", address);
  entered = 1;
  pthread_cond_broadcast(&ready);
  while (!release_fetch) pthread_cond_wait(&ready, &network);
  pthread_mutex_unlock(&network);
  return strdup("{\"imdb\":\"tt1375666\",\"texto\":\"Recommendation\"}");
}

static void wait_fetch(void) {
  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += 2;
  pthread_mutex_lock(&network);
  while (!entered) assert(pthread_cond_timedwait(&ready, &network, &deadline) == 0);
  pthread_mutex_unlock(&network);
}
// Between two fetches: wait_fetch watches `entered`, and the release of the
// previous fetch leaves both set — the second wait would pass before the
// second thread even started asking for the address.
static void reset_fetch(void) {
  pthread_mutex_lock(&network);
  entered = 0;
  release_fetch = 0;
  pthread_mutex_unlock(&network);
}
static void finish_fetch(void) {
  pthread_mutex_lock(&network);
  release_fetch = 1;
  pthread_cond_broadcast(&ready);
  pthread_mutex_unlock(&network);
  for (int i = 0; i < 2000; i++) {
    pthread_mutex_lock(&trava);
    int busy = fioVivo;
    pthread_mutex_unlock(&trava);
    if (!busy) return;
    usleep(1000);
  }
  assert(!"fetch did not finish");
}

int main(int argc, char **argv) {
  assert(argc == 2);
  if (!strcmp(argv[1], "startup")) {
    curadoria_passo(); // Home startup, without opening Settings.
    wait_fetch();
    assert(!strcmp(requested, "https://example.test/curation"));
    finish_fetch();
    assert(curadoria_ativa());
  } else if (!strcmp(argv[1], "change")) {
    // Review #205: changing A->B must clear the old pick and fetch B NOW —
    // not after the 30-min cycle of A's read (and not with A's movie still
    // wearing the "Curadoria" badge under B's address).
    assert(curadoria_definir_url("https://example.test/a"));
    curadoria_passo();
    wait_fetch();
    assert(!strcmp(requested, "https://example.test/a"));
    finish_fetch();
    assert(curadoria_ativa());
    assert(curadoria_definir_url("https://example.test/b"));
    assert(!curadoria_ativa());          // A's pick is not B's curation
    assert(!curadoria_texto()[0]);
    reset_fetch();
    curadoria_passo();                   // lidaEm reset: fetch B immediately
    wait_fetch();
    assert(!strcmp(requested, "https://example.test/b"));
    finish_fetch();
    assert(curadoria_ativa());
  } else {
    assert(curadoria_definir_url("https://example.test/curation"));
    curadoria_url();
    assert(curadoria_receber("{\"imdb\":\"tt0088763\",\"texto\":\"Old\"}"));
    if (!strcmp(argv[1], "pending")) {
      curadoria_passo();
      wait_fetch();
    }
    unsigned before = curadoria_revisao();
    assert(curadoria_definir_url(""));
    if (!strcmp(argv[1], "pending")) finish_fetch();
    assert(!curadoria_ativa());
    assert(!curadoria_texto()[0]);
    assert(curadoria_revisao() > before);
    assert(!curadoria_url()[0]);
  }
  puts("curadoria: production lifecycle ok");
  return 0;
}
