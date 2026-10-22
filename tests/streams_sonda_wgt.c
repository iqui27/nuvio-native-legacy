// Compila streams.c no ramo WGT, rede simulada: navegador nao manda Referer,
// Origin nem User-Agent, mas AVPlay manda. 401/403 com esses headers sao
// inconclusivos; 5xx, transporte e recusa sem header continuam falha.
#include "streams.h"
#include "rede.h"
#include <assert.h>
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
  const char *url = "https://media.example/media.m3u8";
  statusSonda = 403;
  assert(!stream_url_serve(url, NULL));
  assert(!stream_url_serve(url, "X-Token: test"));
  assert(stream_url_serve(url, "Referer: https://addon.example/"));
  assert(strstr(cabEnviado, "Referer:"));
  assert(stream_url_serve(url, "origin: https://addon.example/"));
  assert(stream_url_serve(url, "User-Agent: Player"));
  statusSonda = 401;
  assert(stream_url_serve(url, "Referer: https://addon.example/"));
  statusSonda = 500;
  assert(!stream_url_serve(url, "Referer: https://addon.example/"));
  statusSonda = 0;
  assert(!stream_url_serve(url, "Referer: https://addon.example/"));
  statusSonda = 200;
  assert(stream_url_serve(url, "Referer: https://addon.example/"));
  puts("streams_sonda_wgt: AVPlay decide 401/403 com header restrito, 500/transporte recusados");
  return 0;
}
