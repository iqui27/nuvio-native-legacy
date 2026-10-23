// Exercise the production stream worker against a local CNCVerse resource.
#define main addonslista_legacy_main
#define rede_baixar legacy_baixar
#define rede_baixar_st legacy_baixar_st
#include "addonslista.c"
#undef main
#undef rede_baixar
#undef rede_baixar_st
char *rede_baixar(const char *url, int seconds);
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status);
#include "../src/addons.c"
#include "idcanal.h"
#include <assert.h>

static char expected[4096];
static int requests, status = 200;
char *rede_baixar(const char *url, int seconds) {
  (void)seconds;
  assert(!strcmp(url, expected));
  requests++;
  if (status == 201) return strdup("<html>invalid response</html>");
  if (status == 202) return strdup("{\"streams\":[]}");
  return status == 200 ? strdup("{\"streams\":[{\"url\":\"https://fixture.invalid/live.m3u8\"}]}") : NULL;
}
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *st) {
  (void)headers;
  *st = status;
  return rede_baixar(url, seconds);
}
int main(void) {
  char id[IDCANAL_LONGO_MAX + 1], alias[64];
  char path[2048];
  snprintf(path, sizeof path, "%s/id", getenv("CNC_FIXTURE_DIR"));
  FILE *f = fopen(path, "rb"); assert(f);
  size_t len = fread(id, 1, sizeof id - 1, f); id[len] = 0; fclose(f);
  assert(len > 768);
  assert(idcanal_curto(id, alias, sizeof alias));
  char encoded[3 * IDCANAL_LONGO_MAX + 1];
  assert(nv_addon_id(encoded, sizeof encoded, id));
  snprintf(expected, sizeof expected, "https://fixture.invalid/stream/tv/%s.json", encoded);
  nAddon = 1; memset(addon, 0, sizeof addon);
  strcpy(addon[0].nome, "CNCVerse Bridge Main");
  strcpy(addon[0].base, "https://fixture.invalid");
  addon[0].ativo = addon[0].fonte = 1;
  for (int mode = 0; mode < 4; mode++) {
    BaldeFonte b = {.idx = 0};
    Consulta c = {.id = alias, .tipo = "tv", .timeout = 12,
                  .baldes = &b, .nBaldes = 1, .rodada = 1};
    pthread_mutex_init(&c.trava, NULL);
    c.sessao = perfis_sessao_geracao();
    c.origens = capturarPedidos(c.sessao, &c.nOrigens, &c.versao);
    assert(c.origens);
    if (mode) { status = 403; c.tipo = "movie";
      snprintf(expected, sizeof expected, "https://fixture.invalid/stream/movie/%s.json", encoded); }
    if (mode == 2) status = 201;
    if (mode == 3) status = 202;
    fioFontes(&c);
    if (!mode) assert(requests == 1 && b.n == 1);
    // Empty "streams" answered the addon: no retry, respondeu=1, 0 sources.
    else if (mode == 3) assert(requests == 4 && b.respondeu && b.n == 0);
    else assert(requests == mode + 1 && b.n == 0);
    free(b.achados); free(c.origens); pthread_mutex_destroy(&c.trava);
  }
  puts("PASS A: long CNC ID reaches the stream resource; HTTP errors diagnosed; empty streams answered");
}
