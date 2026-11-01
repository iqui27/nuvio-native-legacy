// Cold-start account resources must wait for an explicit manifest declaration.
#define main addonslista_fixture_main
#include "addonslista.c"
#undef main
#include "../src/addons.c"
#include <assert.h>

static int subtitleRequests;
char *rede_baixar_medido_controle(const char *url, int seconds,
    const char *const *headers, const RedeControle *control, RedeMedida *measure) {
  (void)seconds; (void)headers; (void)control;
  assert(strstr(url, "legenda.test/subtitles/"));
  subtitleRequests++;
  *measure = (RedeMedida){ .status = 200 };
  return strdup("{\"subtitles\":[]}");
}
char *rede_baixar_trecho_st(const char *url, int seconds, long long start,
    long long end, long *size, int *status, int *error, char *final, unsigned cap) {
  (void)url; (void)seconds; (void)start; (void)end; (void)size;
  (void)status; (void)error; (void)final; (void)cap;
  assert(!"Unexpected range request"); return NULL;
}

static void search(void) {
  Stream *out = NULL;
  Resumo result = {0};
  assert(consultar("tt1", "movie", NULL, 1, NULL, NULL, &out, &result, 1,
                   perfis_sessao_geracao()) == 0);
  assert(result.consultados == 1);
  free(out);
  snprintf(legId, sizeof legId, "tt1");
  snprintf(legTipo, sizeof legTipo, "movie");
  legSessao = perfis_sessao_geracao();
  legParar = 0; fioLegVivo = 1;
  buscarLegendas(NULL);
}
int main(void) {
  AddonRemoto account[] = {
    { .nome = "Subtitle Sync", .url = "https://legenda.test/manifest.json", .ativo = 1 },
    { .nome = "Streams only", .url = "https://exemplo.test/manifest.json", .ativo = 1 }
  };
  assert(addons_definir_lista(account, 2));
  // Resolve unknown stream capabilities before querying; subtitles stay gated.
  search();
  assert(pedidosLegendaManifesto == 1);
  assert(pedidosLegendaStream == 0 && subtitleRequests == 1);
  addons_manifesto_lido(0, "{\"id\":\"subs\",\"resources\":[\"subtitles\"]}");
  addons_manifesto_lido(1, "{\"id\":\"streams\",\"resources\":[\"stream\"]}");
  resp[0] = "{\"streams\":[]}";
  search();
  assert(pedidosLegendaStream == 0 && subtitleRequests == 2);
  // Repeated searches must never retry the subtitles-only stream endpoint.
  search();
  assert(pedidosLegendaStream == 0 && subtitleRequests == 3);
  puts("addons_manifest_gate: manifests resolved; declared resources preserved");
  return 0;
}
