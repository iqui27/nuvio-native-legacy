/* Real stop path: old workers stay alive while every slot is reused. */
#define NV_TPK
#include "../src/dts/dts_hls_tpk.c"
#include <assert.h>

static pthread_mutex_t hold = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int started, releaseOld, finished;
static int valid(const char *head) { (void)head; return 1; }
static void *worker(void *u) {
  pthread_mutex_lock(&hold);
  int number = ++started;
  pthread_cond_broadcast(&changed);
  while (number <= 8 && !releaseOld) pthread_cond_wait(&changed, &hold);
  pthread_mutex_unlock(&hold);
  if (number <= 8) conexaoFim(u);
  pthread_mutex_lock(&hold);
  finished++;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&hold);
  return NULL;
}
static void waitFor(int *value, int count) {
  struct timespec deadline;
  prazoEm(&deadline, 2000);
  pthread_mutex_lock(&hold);
  while (*value < count) assert(!pthread_cond_timedwait(&changed, &hold, &deadline));
  pthread_mutex_unlock(&hold);
}
int main(void) {
  int old[8][2], fresh[8][2];
  char head[] = "GET /token/m.m3u8 HTTP/1.1\r\n\r\n";
  Sessao *s = calloc(1, sizeof *s);
  assert(s);
  pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->c, NULL);
  s->refs = 2; atual = s;
  pthread_mutex_init(&porteiroHls.m, NULL);
  porteiroHls.maxVivos = 8; porteiroHls.valido = valid; porteiroHls.fio = worker;
  for (int i = 0; i < 8; i++) {
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, old[i]));
    entregar(&porteiroHls, old[i][0], head, strlen(head));
    waitFor(&started, i + 1);
  }
  dts_tpk_fechar();
  for (int i = 0; i < 8; i++) {
    struct pollfd p = {old[i][1], POLLIN, 0};
    char byte;
    assert(poll(&p, 1, 100) == 1 && recv(p.fd, &byte, 1, 0) == 0);
    close(old[i][1]);
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fresh[i]));
    entregar(&porteiroHls, fresh[i][0], head, strlen(head));
    waitFor(&started, 9 + i);
  }
  pthread_mutex_lock(&hold); releaseOld = 1; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&hold);
  waitFor(&finished, 16);
  assert(atomic_load(&porteiroHls.vivos) == 8);
#ifdef PORT_VIVOS
  for (int i = 0; i < 8; i++) conexaoFim(porteiroHls.conexoes[i]);
#endif
  for (int i = 0; i < 8; i++) close(fresh[i][1]);
  sessaoDestruir(s);
  puts("PASS stop shuts sockets, immediately reuses all slots, ignores late old workers");
  return 0;
}
