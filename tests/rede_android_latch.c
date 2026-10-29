/* Exercise the real Android wrappers without network or a device. */
#define NV_ANDROID 1
#include <assert.h>
#include <stdarg.h>
#include "../src/rede.c"
static int curlRc, curlCalls, androidCalls, androidFails;
static void *init(void) { return calloc(1, 64); }
static int option(void *c, int o, ...) { (void)c; (void)o; return 0; }
static int perform(void *c) { (void)c; curlCalls++; return curlRc; }
static int info(void *c, int o, ...) {
  va_list ap;
  (void)c; va_start(ap, o);
  if (o == INFO_RESPONSE_CODE) *va_arg(ap, long *) = curlRc ? 0 : 200;
  va_end(ap); return 0;
}
char *android_http(const char *v, const char *u, const char *c, const char *b, int p,
                   int *st, int *retry, long *n, char *error, size_t ne) {
  (void)v; (void)u; (void)c; (void)b; (void)p;
  androidCalls++; *st = androidFails ? 0 : 200;
  if (retry) *retry = 0;
  *n = 2; snprintf(error, ne, "test transport failure");
  return androidFails ? NULL : strdup("{}");
}
void android_etapa(const char *s) { (void)s; }
void android_quadro(void) {}
static void request(const char *url, int code, int method) {
  int st = 0;
  char *r;
  curlRc = code;
  if (method == 1) r = rede_postar_st(url, 1, NULL, "{}", &st);
  else if (method == 2) r = rede_colocar_st(url, 1, NULL, "{}", &st);
  else r = rede_baixar_st(url, 1, NULL, &st);
  assert(androidFails ? !r && !st : r && st == 200);
  free(r);
}
int main(void) {
  int i, before;
  pronto = 1; curl_init = init; curl_cleanup = free; curl_setopt_f = option;
  curl_perform = perform; curl_getinfo = info;
  /* Unsupported/malformed URLs and local failures never poison the session. */
  request("file:///test", 1, 0); assert(!curlRuim);
  request("https://bad.example/[]", 3, 1); assert(!curlRuim);
  request("https://local.example/", 27, 2); assert(!curlRuim);
  /* One slow host, even across schemes, ports, case and credentials. */
  for (i = 0; i < 4; i++) {
    request("https://slow.example/a", 28, i % 3); assert(!curlRuim);
  }
  request("http://user:pass@SLOW.example:8080/b", 28, 1); assert(!curlRuim);
  request("https://slow.example:443/c", 7, 2); assert(!curlRuim);
  /* Failed Android fallback contributes no evidence. */
  androidFails = 1;
  request("https://failed.example/", 35, 1); assert(!curlRuim);
  androidFails = 0;
  request("https://second.example/", 56, 1); assert(!curlRuim);
  request("https://third.example/", 35, 2); assert(curlRuim);
  /* Once the threshold is met, unrelated simple requests bypass curl. */
  before = curlCalls; i = androidCalls;
  request("https://healthy.example/", 0, 0);
  assert(curlCalls == before && androidCalls == i + 1);
  puts("rede_android_latch: ok");
  return 0;
}
