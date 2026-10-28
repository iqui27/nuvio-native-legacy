// Exercise the production source gate with browser/native transport fixtures.
#include "streams.h"
#include "rede.h"
#include <assert.h>
#include "stub_perfisessao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int statusSonda;
static char cabEnviado[512];
int rede_url_final_tipo(const char *u, int seg, const char *const *cab,
                        char *dst, unsigned n, int *status,
                        char *mime, unsigned mimeTam, long *corpo) {
  if (mime && mimeTam) mime[0] = 0;
  if (corpo) *corpo = -1;
  int i;
  assert(seg > 0); cabEnviado[0] = 0; dst[0] = 0; *status = statusSonda;
  for (i = 0; cab && cab[i]; i++) {
    assert(strlen(cabEnviado) + strlen(cab[i]) + 2 < sizeof cabEnviado);
    strcat(cabEnviado, cab[i]); strcat(cabEnviado, "\n");
  }
  if (statusSonda >= 200 && statusSonda < 300) { snprintf(dst, n, "%s", u); return 1; }
  return 0;
}
char *rede_baixar_st(const char *u, int seg, const char *const *cab, int *st) {
  (void)u; (void)seg; (void)cab; *st = 403; return strdup("denied");
}
int main(void) {
  const char *url = "https://example.invalid/media.m3u8";
  const char *headers = "Referer: https://example.invalid/";
  statusSonda = 403;
  assert(!stream_url_serve(url, NULL));
  assert(!stream_url_serve(url, "X-Fixture: test"));
#ifdef __EMSCRIPTEN__
  assert(stream_url_serve(url, headers));
  assert(strstr(cabEnviado, "Referer:"));
  assert(stream_url_serve(url, "origin: https://example.invalid/"));
  assert(stream_url_serve(url, "User-Agent: Player"));
  statusSonda = 401;
  assert(stream_url_serve(url, headers));
#else
  assert(!stream_url_serve(url, headers));
#endif
  statusSonda = 500;
  assert(!stream_url_serve(url, headers));
  // CORS rejection exposes no HTTP status, even when AVPlay can play media.
  statusSonda = 0;
#if defined(__EMSCRIPTEN__) && !defined(NV_VIDAA)
  assert(stream_url_serve(url, NULL));
  assert(stream_url_serve(url, headers));
#else
  assert(!stream_url_serve(url, NULL));
  assert(!stream_url_serve(url, headers));
#endif
  statusSonda = 200;
  assert(stream_url_serve(url, headers));
  puts("streams_sonda_wgt: PASS (HTTP refusal, browser restrictions, transport policy)");
  return 0;
}
