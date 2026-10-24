// Offline manifest + metadata + stream regression through the production worker.
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
#include <assert.h>

static const char manifest[] = "{\"types\":[\"series\",\"movie\"],\"resources\":[\"meta\",{\"name\":\"stream\",\"idPrefixes\":[\"kart:\",\"moviebox:\"]}]}";
static const char meta[] = "{\"meta\":{\"id\":\"kart:dora\",\"videos\":[{\"id\":\"kart:é /pilot\",\"season\":1,\"episode\":1}]}}";
static const char streams[] = "{\"streams\":[{\"url\":\"https://fixture.invalid/video.mp4\"}]}";
static int calls;
static char expected[512];
char *rede_baixar(const char *url, int seconds) {
  assert(seconds == (strstr(expected, "/stream/tv/") ? 12 : 30));
  assert(!strcmp(url, expected));
  calls++;
  return strdup(streams);
}
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers; *status = 200; return rede_baixar(url, seconds);
}
int main(void) {
  nAddon = 2; memset(addon, 0, sizeof addon);
  strcpy(addon[0].base, "https://fixture.invalid");
  addon[0].ativo = addon[0].fonte = 1;
  lerDeclaracao(manifest, chaveNaRaiz(manifest, NULL, "resources"), "stream",
                addon[0].streamTipos, addon[0].streamPref, &addon[0].nStreamPref);
  AddonPedido owner; copiarPedido(0, &owner);
  assert(aceitaStream(&owner, "series", "kart:dora"));
  assert(aceitaStream(&owner, "movie", "moviebox:film"));
  assert(!aceitaStream(&owner, "series", "tt123:1:1"));
  assert(!aceitaStream(&owner, "tv", "kart:dora"));
  strcpy(addon[1].base, "https://unrelated.invalid");
  addon[1].ativo = addon[1].fonte = 1;
  strcpy(addon[1].streamPref[0], "tt"); addon[1].nStreamPref = 1;
  char video[64];
  const char *v = js_array(meta, NULL, "videos");
  assert(v && js_texto(v, js_fim(v), "id", video, sizeof video));
  for (int movie = 0; movie < 2; movie++) {
    const char *id = movie ? "moviebox:film /é" : video;
    const char *type = movie ? "movie" : "series";
    char encoded[256]; assert(nv_addon_id(encoded, sizeof encoded, id));
    assert(!strcmp(encoded, movie ? "moviebox:film%20%2F%C3%A9" : "kart:%C3%A9%20%2Fpilot"));
    snprintf(expected, sizeof expected, "https://fixture.invalid/stream/%s/%s.json", type, encoded);
    if (movie) {
      // String-form stream with no prefixes: the catalog owner is sufficient.
      addon[0].nStreamPref = addon[1].nStreamPref = 0;
      addons_definir_origem(addon[0].base);
    }
    addons_buscar(id, type);
    assert(fioVivo);
    pthread_join(fio, NULL); fioVivo = 0;
    assert(!strcmp(alvoId, id));
    assert(nResultado == 1);
    free(resultado); resultado = NULL; nResultado = 0;
  }
  char longId[160], alias[64];
  memset(longId, 'a', sizeof longId - 1); longId[sizeof longId - 1] = 0;
  memcpy(longId, "cnc:", 4);
  assert(idcanal_curto(longId, alias, sizeof alias));
  assert(!strncmp(alias, "~c:", 3));
  strcpy(addon[0].streamTipos, "|tv|");
  strcpy(addon[0].streamPref[0], "cnc:"); addon[0].nStreamPref = 1;
  strcpy(addon[1].streamPref[0], "tt"); addon[1].nStreamPref = 1;
  copiarPedido(0, &owner);
  assert(aceitaStream(&owner, "tv", alias));
  assert(!aceitaStream(&owner, "tv", "other:channel"));
  snprintf(expected, sizeof expected, "https://fixture.invalid/stream/tv/%s.json", longId);
  addons_definir_origem(addon[0].base);
  addons_buscar(alias, "tv");
  assert(fioVivo);
  pthread_join(fio, NULL); fioVivo = 0;
  assert(nResultado == 1);
  free(resultado); resultado = NULL; nResultado = 0;
  assert(calls == 3);
  puts("PASS custom video IDs and long channel aliases reach their stream resource");
}
