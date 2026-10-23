// Production manifest parser and stream worker; no network.
#include <assert.h>
#define main addonslista_legacy_main
#define rede_baixar legacy_baixar
#define rede_baixar_st legacy_baixar_st
#include "addonslista.c"
#undef main
#undef rede_baixar
#undef rede_baixar_st
char *rede_baixar(const char *, int);
char *rede_baixar_st(const char *, int, const char *const *, int *);
#include "../src/addons.c"
static int requests;
char *rede_baixar(const char *url, int seconds) {
  (void)seconds;
  assert(!strcmp(url, "https://fixture.invalid/stream/tv/ecipfs-aee.json")); requests++;
  return strdup("{\"streams\":[{\"url\":\"https://fixture.invalid/live.m3u8\"}]}");
}
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers; *status = 200; return rede_baixar(url, seconds);
}
int main(void) {
  FILE *f = fopen("tests/fixtures/tvplay/manifest.json", "rb"); assert(f);
  char manifest[4096]; size_t n = fread(manifest, 1, sizeof manifest - 1, f);
  assert(feof(f)); fclose(f); manifest[n] = 0;
  assert(addons_adicionar("TVPlay", "https://fixture.invalid/manifest.json"));
  addons_manifesto_lido(0, manifest);
  assert(addons_aceita_id(0, "tv", "ecipfs-aee") == 1);
  AddCatCanal catalogs[ADD_CANAL_MAX];
  assert(addons_catalogos_canal(0, catalogs, ADD_CANAL_MAX) == ADD_CANAL_MAX);
  assert(!strcmp(catalogs[0].tipo, "tv") && !strcmp(catalogs[0].id, "tvplay"));
  BaldeFonte b = {.idx = 0};
  Consulta c = {.id = "ecipfs-aee", .tipo = "tv", .timeout = 12, .baldes = &b, .nBaldes = 1, .rodada = 1};
  pthread_mutex_init(&c.trava, NULL); c.sessao = perfis_sessao_geracao();
  c.origens = capturarPedidos(c.sessao, &c.nOrigens, &c.versao); assert(c.origens);
  fioFontes(&c);
  assert(requests == 1 && b.n == 1);
  free(b.achados); free(c.origens); pthread_mutex_destroy(&c.trava);
  puts("PASS: TVPlay manifest accepted; production worker requests stream/tv/ecipfs-aee.json");
}
