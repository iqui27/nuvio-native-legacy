/* Local-only HTTP/TLS reuse, cancellation and deadline stress. */
#include <assert.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rede.c"

static const char *httpBase, *tlsBase;
static int (*realPerform)(void *);
static int (*realOption)(void *, int, ...);
static _Thread_local int signalSafe, dnsTimeout;
static unsigned performed, cancelled, deadlines, reused, dnsDeadlines;
static int trackedOption(void *c, int opt, ...) {
  va_list ap;
  int r;
  va_start(ap, opt);
  /* Options used by rede.c have curl's stable long/pointer/function ABI. */
  if (opt < 10000) {
    long value = va_arg(ap, long);
    if (opt == OPT_NOSIGNAL) signalSafe = value == 1;
    r = realOption(c, opt, value);
  } else if (opt < 20000) {
    void *value = va_arg(ap, void *);
    r = realOption(c, opt, value);
  } else {
    void (*value)(void) = va_arg(ap, void (*)(void));
    r = realOption(c, opt, value);
  }
  va_end(ap);
  return r;
}
static int trackedPerform(void *c) {
  int r;
  long connections = -1;
  assert(signalSafe); /* All exercised handle paths, including after reset. */
  signalSafe = 0;
  __atomic_add_fetch(&performed, 1, __ATOMIC_RELAXED);
  /* Deterministic resolver timeout: no external DNS traffic or host-specific
   * assumptions. NOSIGNAL must already be set before curl can resolve. */
  if (dnsTimeout) return 28;
  r = realPerform(c);
  if (!r && !curl_getinfo(c, INFO_NUM_CONNECTS, &connections) && !connections)
    __atomic_add_fetch(&reused, 1, __ATOMIC_RELAXED);
  return r;
}
static void *worker(void *arg) {
  char url[256];
  int i, status;
  long n;
  (void)arg;
  for (i = 0; i < 12; i++) {
    char *body;
    redeCancelouLocal = 0;
    snprintf(url, sizeof url, "%s/ok", i % 2 ? tlsBase : httpBase);
    body = rede_baixar_bin(url, 2, &n);
    assert(body && n == 7); free(body);
    if (i % 3 == 0) {
      volatile int cancel = 1;
      redeCancelLocal = &cancel;
      body = rede_baixar_bin(url, 2, &n);
      assert(!body);
      assert(redeCancelouLocal);
      __atomic_add_fetch(&cancelled, 1, __ATOMIC_RELAXED);
      redeCancelLocal = NULL;
      redeCancelouLocal = 0;
    }
    if (i % 4 == 0) {
      RedePedido p = {0};
      RedeResposta r;
      snprintf(url, sizeof url, "%s/slowheaders", httpBase);
      p.url = url; p.prazo_ms = 30;
      assert(!rede_pedir(&p, &r));
      assert(r.erro == REDE_PRAZO);
      __atomic_add_fetch(&deadlines, 1, __ATOMIC_RELAXED);
      rede_resposta_limpar(&r);
    }
    snprintf(url, sizeof url, "%s/ok", httpBase);
    body = rede_postar_st(url, 2, NULL, "{}", &status);
    assert(body && status == 200); free(body);
  }
  {
    RedePedido p = {0};
    RedeResposta r;
    p.url = "http://dns-timeout.invalid/"; p.prazo_ms = 30;
    dnsTimeout = 1;
    assert(!rede_pedir(&p, &r) && r.erro == REDE_PRAZO && r.curl_erro == 28);
    dnsTimeout = 0;
    __atomic_add_fetch(&dnsDeadlines, 1, __ATOMIC_RELAXED);
    rede_resposta_limpar(&r);
  }
  return NULL;
}
int main(int argc, char **argv) {
  pthread_t threads[12];
  unsigned i;
  assert(argc == 3);
  httpBase = argv[1]; tlsBase = argv[2];
  rede_preparar();
  assert(pronto == 1);
  realPerform = curl_perform; curl_perform = trackedPerform;
  realOption = curl_setopt_f; curl_setopt_f = trackedOption;
  for (i = 0; i < 12; i++) assert(!pthread_create(&threads[i], NULL, worker, NULL));
  for (i = 0; i < 12; i++) assert(!pthread_join(threads[i], NULL));
  assert(performed == 384 && cancelled == 48 && deadlines == 36 && reused > 0 && dnsDeadlines == 12);
  puts("PASS: 372 local HTTP/TLS requests, connection reuse, 48 cancellations, 36 deadlines, 12 simulated DNS timeouts, NOSIGNAL before every transfer");
  return 0;
}
