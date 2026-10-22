// hostDaUrl() nao pode deixar userinfo (usuario:senha@) nos logs Android.
// Inclui rede.c com NV_ANDROID, doubles de libcurl/android_http, captura stdout.
#define NV_ANDROID 1
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rede.c"
static int androidErro;
char *android_http(const char *v, const char *u, const char *c, const char *b, int p,
                   int *st, long *n, char *erro, size_t ne) {
  (void)v; (void)u; (void)c; (void)b; (void)p; (void)n;
  if (androidErro) { snprintf(erro, ne, "falha de transporte"); *st = 0; return NULL; }
  *st = 200; return strdup("{}");
}
void android_etapa(const char *n) { (void)n; }
void android_quadro(void) {}
static int curlRc;
static void *init(void) { return calloc(1, 64); }
static int option(void *c, int o, ...) { (void)c; (void)o; return 0; }
static int perform(void *c) { (void)c; return curlRc; }
static int info(void *c, int o, ...) {
  va_list ap; va_start(ap, o); (void)c;
  if (o == INFO_RESPONSE_CODE) *va_arg(ap, long *) = 0;
  va_end(ap); return 0;
}
static void cleanup(void *c) { free(c); }
#define URL "https://usuario:SEGREDO_FICTICIO@addon.example/manifest.json"
static char saida[1 << 16];
static int le(const char *arq) {
  FILE *f = fopen(arq, "r"); size_t n; if (!f) return 0;
  n = fread(saida, 1, sizeof saida - 1, f); saida[n] = 0; fclose(f); return 1;
}
int main(int argc, char **argv) {
  const char *arq = argc > 1 ? argv[1] : "/tmp/rede_userinfo.out";
  char h[96], *r; int st = 0;
  pronto = 1; curl_init = init; curl_setopt_f = option; curl_perform = perform;
  curl_cleanup = cleanup; curl_getinfo = info;
  hostDaUrl(URL, h, sizeof h);
  assert(!strcmp(h, "https://addon.example"));
  hostDaUrl("https://a.b:443/x?y", h, sizeof h); assert(!strcmp(h, "https://a.b:443"));
  hostDaUrl("https://u:p@a.b:8080?q=a@b", h, sizeof h); assert(!strcmp(h, "https://a.b:8080"));
  freopen(arq, "w", stdout);
  // sucesso pelo Android
  curlRuim = 1; androidErro = 0;
  r = rede_baixar_st(URL, 5, NULL, &st); assert(r); free(r);
  // erro de transporte pelo Android
  androidErro = 1; r = rede_baixar_st(URL, 5, NULL, &st); assert(!r);
  // erro da libcurl (log "tempo #n") + reserva Android
  curlRuim = 0; curlRc = 7; androidErro = 0;
  r = rede_baixar_st(URL, 5, NULL, &st); free(r);
  fflush(stdout);
  freopen("/dev/null", "w", stdout);
  assert(le(arq));
  fprintf(stderr, "%s", saida);
  assert(strstr(saida, "via Android"));
  assert(strstr(saida, "tempo #"));
  assert(strstr(saida, "addon.example"));
  assert(!strstr(saida, "usuario"));
  assert(!strstr(saida, "SEGREDO_FICTICIO"));
  fprintf(stderr, "rede_userinfo_log: ok\n");
  return 0;
}
