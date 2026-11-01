// FrostView TV (TCL 23/10): manifesto lido mas "resources" ilegivel na raiz
// (aqui: numero antes do array de verdade, dentro de uma string em description).
// Filme/serie nao consultam; canal ao vivo continua; 200 sem "streams" nao e erro.
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
#include <string.h>

static int frostStream, aioStream;
static const char *frostManifest =
  "{\"id\":\"tv.frostview\",\"name\":\"FrostView TV\","
  "\"description\":\"Lista de canais com resources variados\","
  "\"resources\":0,\"types\":[\"channel\"],"
  "\"catalogs\":[{\"id\":\"fv-channels\",\"type\":\"channel\",\"name\":\"Canais\"}]}";
static const char *aioManifest =
  "{\"id\":\"com.aiostreams.test\",\"name\":\"AIO fake\","
  "\"resources\":[{\"name\":\"stream\",\"types\":[\"movie\"],\"idPrefixes\":[\"tt\"]}]}";

char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "frostview.test")) {
    if (strstr(url, "/manifest.json")) return strdup(frostManifest);
    if (strstr(url, "/stream/movie/")) { frostStream++; return strdup("{\"ok\":true}"); }
    if (strstr(url, "/stream/tv/")) { frostStream++; return strdup("{\"ok\":true}"); }
  }
  if (strstr(url, "aio.test")) {
    if (strstr(url, "/manifest.json")) return strdup(aioManifest);
    if (strstr(url, "/stream/")) { aioStream++; return strdup("{\"err\":\"no VOD\"}"); }
  }
  return NULL;
}
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, seconds);
  if (status) *status = body ? 200 : 0;
  return body;
}

static void shapes(void) {
  int a = addons_adicionar("Shape str", "https://shape-str.test/manifest.json");
  assert(a >= 0);
  addons_manifesto_lido(a,
    "{\"id\":\"shape.str\",\"resources\":[\"catalog\",\"stream\"]}");
  assert(addons_sondado(a) && addons_fornece(a, ADD_STREAM) && !addon[a].capPresumida);
  int b = addons_adicionar("Shape obj", "https://shape-obj.test/manifest.json");
  assert(b >= 0);
  addons_manifesto_lido(b,
    "{\"id\":\"shape.obj\",\"resources\":[{\"name\":\"stream\",\"types\":[\"movie\"]}]}");
  assert(addons_fornece(b, ADD_STREAM) && !addon[b].capPresumida);
  int c = addons_adicionar("Shape chan", "https://shape-chan.test/manifest.json");
  assert(c >= 0);
  addons_manifesto_lido(c,
    "{\"id\":\"shape.chan\",\"resources\":[{\"name\":\"stream\",\"types\":[\"channel\"]}]}");
  assert(addons_fornece(c, ADD_STREAM) && !addon[c].capPresumida);
  assert(!strstr(addon[c].streamTipos, "|movie|"));
}

int main(void) {
  AddonRemoto lista[2];
  memset(lista, 0, sizeof lista);
  snprintf(lista[0].nome, sizeof lista[0].nome, "FrostView TV");
  snprintf(lista[0].url, sizeof lista[0].url, "https://frostview.test/manifest.json");
  lista[0].ativo = 1;
  snprintf(lista[1].nome, sizeof lista[1].nome, "AIO fake");
  snprintf(lista[1].url, sizeof lista[1].url, "https://aio.test/manifest.json");
  lista[1].ativo = 1;
  assert(addons_definir_lista(lista, 2));
  addons_manifesto_lido(0, frostManifest);
  addons_manifesto_lido(1, aioManifest);
  assert(addons_sondado(0));
  assert(addon[0].capPresumida);
  assert(addons_fornece(0, ADD_STREAM));
  { AddCatCanal c[2];
    assert(addons_catalogos_canal(0, c, 2) == 1);
    assert(!strcmp(c[0].tipo, "channel")); }

  frostStream = aioStream = 0;
  { Stream *l = NULL;
    assert(addons_consultar("tt12042730", "movie", NULL, 1, NULL, NULL, &l) == 0);
    assert(frostStream == 0);
    assert(aioStream > 0);
    free(l); }

  frostStream = 0;
  addons_definir_origem("https://frostview.test");
  addons_buscar("cs:channel:axn", "tv");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  assert(frostStream > 0);
  { char m[200];
    if (!addons_motivo_vazio(m, sizeof m)) m[0] = 0;
    assert(!strstr(m, "não respondeu")); }

  shapes();

  puts("addons_frostview_presumido: ok");
  return 0;
}
