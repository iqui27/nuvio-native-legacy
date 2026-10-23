/* #418: exercise native POST with synthetic curl failures, no network. */
#include <assert.h>
#include <stdarg.h>
#ifdef NV_TPK40
void nv_tpk40_etapa(const char *s) { (void)s; }
#endif
#include "../src/rede.c"
static int result, noHandle, http, verifyPeer, verifyHost;
static long timeoutMs, connectMs;
static void *init(void) { return noHandle ? NULL : malloc(1); }
static void cleanup(void *p) { free(p); }
static int option(void *c, int o, ...) {
  va_list ap; (void)c; va_start(ap, o);
  if (o == OPT_TIMEOUT_MS) timeoutMs = va_arg(ap, long);
  if (o == OPT_CONNECTTIMEOUT_MS) connectMs = va_arg(ap, long);
  if (o == OPT_SSL_VERIFYPEER) verifyPeer = (int)va_arg(ap, long);
  if (o == OPT_SSL_VERIFYHOST) verifyHost = (int)va_arg(ap, long);
  va_end(ap); return 0;
}
static int perform(void *c) {
  (void)c;
  snprintf(redeErroBuf, sizeof redeErroBuf, "SECRET_URL SECRET_TOKEN");
  return result;
}
static int info(void *c, int o, ...) {
  va_list ap; (void)c; va_start(ap, o);
  if (o == INFO_RESPONSE_CODE) *va_arg(ap, long *) = http;
  va_end(ap); return 0;
}
int main(void) {
  int st; char *r;
  pronto = 1; curl_init = init; curl_cleanup = cleanup;
  curl_setopt_f = option; curl_perform = perform; curl_getinfo = info;
  result = 6;
  r = rede_postar_st("https://example.test/auth", 25, NULL, "{}", &st);
  assert(!r && st == 0 && strstr(rede_ultimo_erro(), "curl 6"));
  assert(!strstr(rede_ultimo_erro(), "SECRET"));
  assert(timeoutMs == 25000 && connectMs == 10000);
  assert(verifyPeer == 0 && verifyHost == 0);
  noHandle = 1;
  r = rede_postar_st("https://example.test/auth", 25, NULL, "{}", &st);
  assert(!r && st == 0 && strstr(rede_ultimo_erro(), "curl 27"));
  noHandle = 0; result = 0; http = 503;
  r = rede_postar_st("https://example.test/auth", 25, NULL, "{}", &st);
  assert(r && st == 503 && !rede_ultimo_erro()[0]); free(r);
  puts("login418_rede: ok");
}
