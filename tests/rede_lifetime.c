/* Retained connections must not retain request-owned callback contexts. */
#include <assert.h>
#include <stdarg.h>
#include "../src/rede.c"

typedef struct {
  void *progress, *header, *write, *error;
  long nosignal;
  int resets;
} Handle;
static int cleaned;
static void reset(void *p) {
  Handle *h = p;
  int n = h->resets + 1;
  memset(h, 0, sizeof *h);
  h->resets = n;
}
static int option(void *p, int opt, ...) {
  Handle *h = p;
  va_list ap;
  va_start(ap, opt);
  switch (opt) {
    case OPT_NOSIGNAL: h->nosignal = va_arg(ap, long); break;
    case OPT_XFERINFODATA: h->progress = va_arg(ap, void *); break;
    case OPT_HEADERDATA: h->header = va_arg(ap, void *); break;
    case OPT_WRITEDATA: h->write = va_arg(ap, void *); break;
    case OPT_ERRORBUFFER: h->error = va_arg(ap, void *); break;
  }
  va_end(ap);
  return 0;
}
static void cleanup(void *p) {
  Handle *h = p;
  /* curl_easy_cleanup may call progress/header callbacks. Their request
   * contexts have expired when a cached handle's owning thread exits. */
  assert(!h->progress && !h->header && !h->write && !h->error);
  __atomic_add_fetch(&cleaned, 1, __ATOMIC_RELAXED);
}
static void *worker(void *unused) {
  Handle h = {0};
  int i;
  (void)unused;
  pthread_once(&handleUma, handleCriarChave);
  for (i = 0; i < 128; i++) {
    Vigia v;
    int request;
    pthread_setspecific(handleChave, &h);
    opcoesComuns(&h, 250);
    ligarVigia(&h, &v, 250);
    curl_setopt(&h, OPT_HEADERDATA, &request);
    curl_setopt(&h, OPT_WRITEDATA, &request);
    assert(h.nosignal == 1); /* Reapplied after every easy reset. */
    assert(h.progress && h.header && h.write && h.error);
    soltarHandleR(&h, 0, "http://localhost/");
    assert(h.resets == i + 1);
    assert(!h.progress && !h.header && !h.write && !h.error);
  }
  pthread_setspecific(handleChave, NULL);
  handleSoltar(&h); /* Thread exit or parked-handle eviction. */
  return NULL;
}
int main(void) {
  pthread_t threads[16];
  unsigned i;
  curl_setopt_f = option;
  curl_reset = reset;
  curl_cleanup = cleanup;
  for (i = 0; i < 16; i++) assert(!pthread_create(&threads[i], NULL, worker, NULL));
  for (i = 0; i < 16; i++) assert(!pthread_join(threads[i], NULL));
  assert(cleaned == 16);
  puts("PASS: 2048 concurrent request lifetimes, NOSIGNAL after reset, safe cached cleanup");
  return 0;
}
