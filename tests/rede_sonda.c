// HTTP500 nao e uma URL de filme; Range ignorado nao baixa o filme inteiro.
// Servidor/CDN local em rede_sonda.sh. Nenhuma credencial/rede externa.
#include "rede.h"
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  char url[200], fim[4096];
  const char *cab[] = { "Referer: https://media.example/", "User-Agent: Nuvio-test", NULL };
  char pequeno[100];
  int http;
  assert(argc == 2);
  snprintf(url, sizeof url, "%s/erro500", argv[1]);
  assert(!rede_url_final(url, 2, fim, sizeof fim));
  assert(!fim[0]);
  assert(!stream_url_serve(url, "Referer: https://media.example/"));
  assert(!rede_url_final_cab(url, 2, cab, fim, sizeof fim, &http) && http == 500 && !fim[0]);
  snprintf(url, sizeof url, "%s/referer", argv[1]);
  assert(!rede_url_final_cab(url, 2, NULL, fim, sizeof fim, &http) && http == 403 && !fim[0]);
  assert(rede_url_final_cab(url, 2, cab, fim, sizeof fim, &http) && http == 200 && !strcmp(fim, url));
  assert(stream_url_serve(url, "Referer: https://media.example/\nUser-Agent: Nuvio-test"));
  snprintf(url, sizeof url, "%s/ref-redirect", argv[1]);
  assert(rede_url_final_cab(url, 2, cab, fim, sizeof fim, &http) && http == 200 && strstr(fim, "/referer"));
  { char mime[64]; long corpo = 7;
    // O tipo e o tamanho do corpo vem da mesma sonda (2.0.2, naovideo.h).
    snprintf(url, sizeof url, "%s/html", argv[1]);
    assert(rede_url_final_tipo(url, 2, NULL, fim, sizeof fim, &http, mime, sizeof mime, &corpo));
    assert(!strcmp(mime, "text/html") && corpo == -1 && !stream_url_serve(url, NULL));
    snprintf(url, sizeof url, "%s/json", argv[1]);
    assert(rede_url_final_tipo(url, 2, NULL, fim, sizeof fim, &http, mime, sizeof mime, &corpo));
    assert(!strcmp(mime, "application/json") && corpo == 7 && !stream_url_serve(url, NULL));
    snprintf(url, sizeof url, "%s/video", argv[1]);
    assert(rede_url_final_tipo(url, 2, NULL, fim, sizeof fim, &http, mime, sizeof mime, &corpo));
    assert(!strcmp(mime, "video/mp4") && corpo == 64 && stream_url_serve(url, NULL)); }
  snprintf(url, sizeof url, "%s/partial", argv[1]);
  assert(rede_url_final_cab(url, 2, NULL, fim, sizeof fim, &http) && http == 206);
  snprintf(url, sizeof url, "%s/redirect", argv[1]);
  assert(rede_url_final(url, 2, fim, sizeof fim) && strstr(fim, "/media"));
  snprintf(url, sizeof url, "%s/long", argv[1]);
  strcpy(pequeno, "stale");
  assert(!rede_url_final_cab(url, 2, NULL, pequeno, sizeof pequeno, &http) && http == 200 && !pequeno[0]);
  assert(rede_url_final_cab(url, 2, NULL, fim, sizeof fim, &http) && strlen(fim) > 2000);
  snprintf(url, sizeof url, "%s/large", argv[1]);
  assert(rede_url_final_cab(url, 2, NULL, fim, sizeof fim, &http) && http == 200);
  snprintf(url, sizeof url, "%s/truncated", argv[1]);
  assert(!rede_url_final_cab(url, 2, NULL, fim, sizeof fim, &http) && http == 200 && !fim[0]);
  snprintf(url, sizeof url, "%s/delay", argv[1]);
  assert(!rede_url_final_cab(url, 1, NULL, fim, sizeof fim, &http) && !http && !fim[0]);
  strcpy(fim, "stale"); http = 999;
  assert(!rede_url_final_cab(NULL, 2, NULL, fim, sizeof fim, &http) && !http && !fim[0]);
  assert(!rede_url_final_cab(url, 2, NULL, NULL, 0, &http) && !http);
  puts("rede_sonda: HTTP500/403, Referer200/206, redirect, URL longa, Range ignorado, truncamento e prazo ok");
  return 0;
}
