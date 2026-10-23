/* Live localhost integration test: see dts_range.sh. */
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int http_port, tls_port;
static char url[256];
static const char *HEADERS =
  "Authorization: Bearer range-test-secret\r\n"
  "Cookie: session=range-test-cookie\n"
  "X-Provider-Token: provider-secret\n"
  "Referer: http://example.invalid/watch\n"
  "Origin: http://example.invalid\n"
  "User-Agent: DTS-range-test\n"
  "Range: bytes=0-99999999\n"
  "Accept-Encoding: gzip\n";
static long size;
static int64_t total;
static int status;

static char *get(const char *path, int64_t lo, int64_t hi, const char *headers,
                 volatile int *cancel) {
  snprintf(url, sizeof url, "http://127.0.0.1:%d%s", http_port, path);
  size = -2; total = -2; status = -2;
  return rede_baixar_trecho64_cab(url, headers, lo, hi, &size, &total, &status, cancel);
}
static unsigned long now_ms(void) {
  struct timespec ts;
  assert(!clock_gettime(CLOCK_MONOTONIC, &ts));
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}
static void *cancel_later(void *value) {
  usleep(200000);
  *(volatile int *)value = 1;
  return NULL;
}
int main(int argc, char **argv) {
  const int64_t start = (INT64_C(1) << 31) + 31;
  const char *fail[] = { "/mismatch", "/bad-total", "/malformed", "/missing",
                         "/truncated", "/ignored", "/oversized", "/duplicate" };
  char *body;
  size_t i;
  pthread_t thread;
  volatile int cancel = 0;
  unsigned long begun;
  assert(argc == 5);
  http_port = atoi(argv[1]); tls_port = atoi(argv[3]);
  // The new DTS transport must verify TLS even though legacy generic fetches
  // historically disable it. A self-signed origin is rejected before trusted.
  snprintf(url, sizeof url, "https://127.0.0.1:%d/range", tls_port);
  body = rede_baixar_trecho64_cab(url, HEADERS, 0, 31, &size, &total, &status, NULL);
  assert(!body && size == 0 && total == -1 && status == 0);
  rede_discord_ca(argv[4]);
  body = rede_baixar_trecho64_cab(url, HEADERS, 0, 31, &size, &total, &status, NULL);
  assert(body && size == 32 && status == 206);
  free(body);
  body = get("/range", start, start + 31, NULL, NULL);
  assert(body && size == 32 && status == 206 && total == INT64_C(5) * 1024 * 1024 * 1024);
  for (i = 0; i < 32; i++) assert((unsigned char)body[i] == (start + (int64_t)i) % 251);
  free(body);
  body = get("/range", 0, 4 * 1024 * 1024 - 1, NULL, NULL);
  assert(body && size == 4 * 1024 * 1024 && (unsigned char)body[size-1] == (size-1) % 251);
  free(body);
  body = get("/unknown", INT64_MAX, INT64_MAX, NULL, NULL);
  assert(body && size == 1 && total == -1); free(body);
  body = get("/overflow", INT64_MAX, INT64_MAX, NULL, NULL);
  assert(!body && size == 0 && status == 206);
  body = get("/unknown", start, start + 31, NULL, NULL);
  assert(body && size == 32 && total == -1); free(body);
  body = get("/short", start, start + 31, NULL, NULL);
  assert(body && size == 8 && total == start + 8); free(body);
  for (i = 0; i < sizeof fail / sizeof *fail; ++i) {
    body = get(fail[i], start, start + 31, NULL, NULL);
    assert(!body && size == 0 && total == -1);
    assert(status == (!strcmp(fail[i], "/ignored") ? 200 : 206));
  }
  // Caller headers reach the origin; native request Range cannot be replaced.
  body = get("/headers", 0, 1023, HEADERS, NULL);
  assert(body && strstr(body, "authorization: Bearer range-test-secret") &&
         strstr(body, "cookie: session=range-test-cookie") &&
         strstr(body, "x-provider-token: provider-secret") &&
         strstr(body, "range: bytes=0-1023") && !strstr(body, "accept-encoding: gzip"));
  free(body);
  body = get("/same", 0, 1023, HEADERS, NULL);
  assert(body && strstr(body, "authorization: Bearer range-test-secret") &&
         strstr(body, "cookie: session=range-test-cookie")); free(body);
  // A different localhost port is a distinct origin. Keep only public hints.
  body = get("/cross", 0, 1023, HEADERS, NULL);
  assert(body && !strstr(body, "authorization:") && !strstr(body, "cookie:") &&
         !strstr(body, "x-provider-token:") &&
         strstr(body, "referer: http://example.invalid/watch") &&
         strstr(body, "origin: http://example.invalid") &&
         strstr(body, "user-agent: DTS-range-test")); free(body);
  body = get("/long-redirect", 0, 1023, HEADERS, NULL);
  assert(!body && size == 0 && status == 302);
  {
    char userinfo[161], long_url[512];
    memset(userinfo, 'u', sizeof userinfo - 1); userinfo[160] = 0;
    snprintf(long_url, sizeof long_url, "http://%s@127.0.0.1:%d/headers", userinfo, http_port);
    body = rede_baixar_trecho64_cab(long_url, HEADERS, 0, 1023, &size, &total, &status, NULL);
    assert(!body && size == 0 && status == 0);
  }
  // Trusted local TLS verifies the HTTPS path; a downgrade to HTTP is
  // followed, but NO caller header reaches the plain-HTTP hop (only libcurl's
  // own Host/Accept/Range).
  snprintf(url, sizeof url, "https://127.0.0.1:%d/downgrade", tls_port);
  body = rede_baixar_trecho64_cab(url, HEADERS, 0, 1023, &size, &total, &status, NULL);
  assert(body && size > 0 && status == 206);
  assert(!strstr(body, "authorization") && !strstr(body, "cookie") && !strstr(body, "x-provider-token"));
  assert(!strstr(body, "referer") && !strstr(body, "origin") && !strstr(body, "DTS-range-test"));
  assert(strstr(body, "range: bytes=0-1023"));
  free(body);
  begun = now_ms();
  assert(!pthread_create(&thread, NULL, cancel_later, (void *)&cancel));
  body = get("/idle", 0, 31, NULL, &cancel);
  assert(!pthread_join(thread, NULL));
  assert(!body && size == 0 && now_ms() - begun < 2500);
  body = get("/range", 0, 31, NULL, &cancel);
  assert(!body && size == 0 && status == 0);
  body = get("/range", -1, 31, NULL, NULL); assert(!body && status == 0);
  body = get("/range", 31, 0, NULL, NULL); assert(!body && status == 0);
  body = get("/range", 0, 4 * 1024 * 1024, NULL, NULL); assert(!body && status == 0);
  puts("dts_range: large offsets, bounds, redirects, credentials and cancellation passed");
  return 0;
}
