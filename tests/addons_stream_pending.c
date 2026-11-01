// A cold-start search must deliver streams without a second search.
#define rede_baixar_st fixture_download_st
#define rede_baixar fixture_download
#define main addonslista_fixture_main
#include "addonslista.c"
#undef main
#undef rede_baixar
#undef rede_baixar_st
char *rede_baixar(const char *url, int seconds);
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status);
#include "../src/addons.c"
#include <assert.h>

static int failManifest, manifestRequests, streamRequests;
char *rede_baixar(const char *url, int seconds) {
  if (strstr(url, "/manifest.json")) {
    assert(seconds > 0 && seconds <= 12);
    manifestRequests++;
    if (failManifest) return NULL;
    usleep(5000000); // Reproduce the reviewed five-second cold start.
    return strdup("{\"resources\":[{\"name\":\"stream\",\"types\":[\"movie\"],\"idPrefixes\":[\"tt\"]}]}");
  }
  streamRequests++;
  return strdup("{\"streams\":[{\"url\":\"https://x/movie.mp4\"}]}");
}
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers;
  *status = 200;
  return rede_baixar(url, seconds);
}
static void searchPending(void) {
  Stream *out = NULL;
  Resumo result = {0};
  assert(consultar("tt1", "movie", NULL, 1, NULL, NULL, &out, &result, 1,
                   perfis_sessao_geracao()) == 1);
  assert(result.consultados == 1);
  free(out);
}
int main(void) {
  AddonRemoto account = { .nome = "Streams", .url = "https://exemplo.test/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&account, 1));
  addons_buscar("tt1", "movie");
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  assert(addons_estado() == ADD_PRONTO);
  assert(manifestRequests == 1 && streamRequests == 1);
  snprintf(account.url, sizeof account.url, "https://antigo.test/manifest.json");
  assert(addons_definir_lista(&account, 1));
  failManifest = 1;
  searchPending();
  searchPending(); // A failed manifest cannot permanently disable streams.
  assert(manifestRequests == 3 && streamRequests == 3);
  addons_manifesto_lido(0, "{\"resources\":[\"subtitles\"]}");
  Stream *out = NULL;
  assert(consultar("tt1", "movie", NULL, 1, NULL, NULL, &out, NULL, 1,
                   perfis_sessao_geracao()) == 0);
  assert(streamRequests == 3);
  puts("addons_stream_pending: delayed and failed manifests preserve delivery");
  return 0;
}
