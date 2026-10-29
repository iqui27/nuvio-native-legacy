/* #630: exercise common options, reset and handle destruction without a TV. */
#include <assert.h>
#include <stdarg.h>
#include "../src/rede.c"

static void *shared;
static long deadline, connectDeadline;
static int ipResolve, shareCalls;
static int option(void *c, int opt, ...) {
  va_list ap;
  (void)c;
  va_start(ap, opt);
  if (opt == 10100) { shared = va_arg(ap, void *); shareCalls++; }
  if (opt == OPT_TIMEOUT_MS) deadline = va_arg(ap, long);
  if (opt == OPT_CONNECTTIMEOUT_MS) connectDeadline = va_arg(ap, long);
  if (opt == 113) ipResolve = (int)va_arg(ap, long);
  va_end(ap);
  return 0;
}
int main(void) {
  int handle;
  void *library = dlopen("libcurl.4.dylib", RTLD_NOW);
  if (!library) library = dlopen("libcurl.so.4", RTLD_NOW);
  assert(library);
  unsetenv("RES_OPTIONS");
  prepararDns(library, "1.17.1");
  assert(!strcmp(getenv("RES_OPTIONS"), "retrans:250 retry:2"));
  /* Two nameservers, both rounds, still fit the 2 s connect deadline;
     a dropped first nameserver advances to the next after 250 ms. */
  {
    int retrans = 0, tries = 0;
    assert(sscanf(getenv("RES_OPTIONS"), "retrans:%d retry:%d", &retrans, &tries) == 2);
    assert(tries == 2 && 2 * (retrans + retrans * 2) < 2000);
  }
  curl_setopt_f = option;
  opcoesComuns(&handle, 2500);
  /* Before the fix, every handle has its own DNS cache. */
  assert(shared != NULL);
  assert(deadline == 2500 && connectDeadline == 2000);
  assert(ipResolve == 0); /* Keep dual-stack, including IPv6-only networks. */
  assert(shared == dnsShare);
  shared = NULL;
  opcoesComuns(&handle, 4000); /* Also reapplied after curl_easy_reset. */
  assert(shared == dnsShare && shareCalls == 2);
  setenv("RES_OPTIONS", "retrans:900 retry:3", 1);
  prepararDns(library, "1.17.1");
  assert(!strcmp(getenv("RES_OPTIONS"), "retrans:900 retry:3"));
  unsetenv("RES_OPTIONS");
  prepararDns(library, "1.34.0");
  assert(!getenv("RES_OPTIONS"));
  puts("PASS: old c-ares retry budget, shared DNS after reset, operator override, dual-stack and deadlines");
  return 0;
}
