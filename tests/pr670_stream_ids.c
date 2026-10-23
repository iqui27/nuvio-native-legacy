// Offline regressions for PR #670 and integration capability filtering.
#define main addonslista_fixture_main
#include "addonslista.c"
#undef main
#include "../src/addons.c"
#include <assert.h>

int main(int argc, char **argv) {
  assert(argc == 2);
  if (!strcmp(argv[1], "specials")) {
    char alt[ARM_COLS + 1][ARM_ALT_TAM];
    assert(arm_ids_stream("kitsu:7442:0:5", "series", alt, ARM_COLS + 1) == 0);
    assert(arm_ids_stream("kitsu:7442:1:5", "series", alt, ARM_COLS + 1) == 0);
    assert(arm_ids_stream("kitsu:7442:5", "series", alt, ARM_COLS + 1) > 0);
  } else if (!strcmp(argv[1], "namespaces")) {
    struct ArmFontes arm = { .trava = PTHREAD_MUTEX_INITIALIZER };
    Consulta c = { .id = "kitsu:7442:5", .tipo = "series", .arm = &arm };
    AddonPedido a = { .nStreamPref = 4, .streamPref = { "kitsu", "kitsu:", "mal", "mal:" } };
    char alt[ARM_COLS + 1][ARM_ALT_TAM];
    assert(streamIdsExtras(&c, &a, 1, alt) == 1);
    assert(!strcmp(alt[0], "mal:16498:5"));
    c.id = "tt2560140:1:5"; arm.feito = 0;
    assert(streamIdsExtras(&c, &a, 0, alt) == 2);
    assert(strcmp(alt[0], alt[1]));
    pthread_mutex_destroy(&arm.trava);
  } else {
    assert(!strcmp(argv[1], "merge"));
    assert(addons_adicionar("IMDb only", "https://anime.test/manifest.json"));
    addons_manifesto_lido(0, "{\"resources\":[{\"name\":\"stream\",\"types\":[\"series\"],\"idPrefixes\":[\"tt\"]}]}");
    Stream *out = NULL;
    pedidosAnime[0] = 0;
    assert(addons_consultar("kitsu:7442:5", "series", "https://anime.test", 1, NULL, NULL, &out) > 0);
    assert(strstr(pedidosAnime, "series/tt2560140:1:5"));
    assert(!strstr(pedidosAnime, "series/kitsu"));
    free(out); out = NULL;
    pedidosAnime[0] = 0;
    assert(addons_consultar("kitsu:7442:5", "movie", "https://anime.test", 1, NULL, NULL, &out) == 0);
    assert(!pedidosAnime[0]);
    free(out);
  }
  printf("pr670_stream_ids %s: ok\n", argv[1]);
  return 0;
}
