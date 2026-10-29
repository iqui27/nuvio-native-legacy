// Issue #565: offline PenguPlay fixture through real manifest/parser/filter code.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *fixture(const char *name) {
  char path[160];
  snprintf(path, sizeof path, "tests/fixtures/pengu/%s.json", name);
  FILE *f = fopen(path, "rb");
  assert(f);
  assert(fseek(f, 0, SEEK_END) == 0);
  long n = ftell(f);
  assert(n >= 0 && fseek(f, 0, SEEK_SET) == 0);
  char *body = calloc((size_t)n + 1, 1);
  assert(body && fread(body, 1, (size_t)n, f) == (size_t)n);
  fclose(f);
  return body;
}

#ifdef PENGU_ADDONS
#include "../src/addons.c"

static char requested[NV_ADDON_PEDIDO_MAX];
// Isolated module fixture: same weak-session contract as tests/addons_sonda_cache.c.
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
void fontecache_vod_limpar(void) {}
char *desc_manifesto_cache_obter(const char *url, unsigned version) {
  (void)url; (void)version; return NULL;
}
void desc_manifesto_cache_guardar(const char *url, unsigned version, const char *body) {
  assert(!strcmp(url, requested) && version == addons_versao());
  assert(strstr(body, "com.penguplay"));
}
static const char *manifesto = "manifest";

char *rede_baixar(const char *url, int timeout) {
  assert(timeout > 0);
  snprintf(requested, sizeof requested, "%s", url);
  return fixture(manifesto); // No sockets; no requests to the provider.
}
const char *rede_url_publica(const char *url, char *dst, unsigned n) {
  (void)url; snprintf(dst, n, "pengu.uk"); return dst;
}

int main(void) {
  const char *urls[] = {
    "https://pengu.uk/", "https://pengu.uk/manifest.json",
    "https://pengu.uk/test-token/manifest.json", "https://pengu.uk/test-token/",
    " stremio://pengu.uk/test-token/manifest.json?setting=1 "
  };
  const char *bases[] = {
    "https://pengu.uk", "https://pengu.uk", "https://pengu.uk/test-token",
    "https://pengu.uk/test-token", "https://pengu.uk/test-token?setting=1"
  };
  for (unsigned k = 0; k < sizeof urls / sizeof *urls; k++) {
    memset(addon, 0, sizeof addon); nAddon = 0;
    assert(addons_adicionar("PenguPlay", urls[k]));
    assert(!strcmp(addons_base(0), bases[k]));
    PedidoSonda *p = calloc(1, sizeof *p);
    assert(p);
    p->n = 1; p->versao = addons_versao(); p->sessao = nv_perfil_sessao_ler();
    p->origens = calloc(1, sizeof *p->origens);
    assert(p->origens);
    copiarPedido(0, p->origens);
    sondar(p);
    char expected[NV_ADDON_PEDIDO_MAX], id[80], streamUrl[NV_ADDON_PEDIDO_MAX];
    nv_addon_url(expected, sizeof expected, bases[k], "/manifest.json");
    assert(!strcmp(requested, expected)); // Never GET / or follow its HTML redirect.
    assert(addons_sondado(0) && !strcmp(addons_id_manifesto(0), "com.penguplay"));
    assert(addons_fornece(0, ADD_STREAM) && addons_fornece(0, ADD_LEGENDA));
    assert(addons_fornece(0, ADD_META) && addons_fornece(0, ADD_CATALOGO));
    assert(addons_catalogos_canal(0, NULL, 0) == 0); // Empty catalogs are valid.
    assert(addons_aceita_id(0, "tv", "pp-live:channel") == 1);
    assert(addons_aceita_id(0, "movie", "tt0111161") == 0);
    assert(addons_aceita_id(0, "tv", "tmdb:278") == 0);
    const char *ids[] = {"tt0111161", "tmdb:278", "pp-live:channel"};
    const char *types[] = {"movie", "movie", "tv"};
    for (unsigned j = 0; j < 3; j++) {
      assert(nv_addon_id(id, sizeof id, ids[j]));
      nv_addon_url(streamUrl, sizeof streamUrl, addons_base(0), "/stream/%s/%s.json", types[j], id);
      nv_addon_url(expected, sizeof expected, bases[k], "/stream/%s/%s.json", types[j], ids[j]);
      assert(!strcmp(streamUrl, expected));
    }
  }

  // #565: manifest served behind the token path — same id/resources plus a
  // config array. Must install identically.
  manifesto = "manifest-auth";
  memset(addon, 0, sizeof addon); nAddon = 0;
  assert(addons_adicionar("PenguPlay", "https://pengu.uk/test-token/manifest.json"));
  PedidoSonda *p = calloc(1, sizeof *p);
  assert(p);
  p->n = 1; p->versao = addons_versao(); p->sessao = nv_perfil_sessao_ler();
  p->origens = calloc(1, sizeof *p->origens);
  assert(p->origens);
  copiarPedido(0, p->origens);
  sondar(p);
  assert(strstr(requested, "/test-token/manifest.json"));
  assert(addons_sondado(0) && !strcmp(addons_id_manifesto(0), "com.penguplay"));
  assert(addons_fornece(0, ADD_STREAM) && addons_fornece(0, ADD_LEGENDA));
  assert(addons_aceita_id(0, "tv", "pp-live:channel") == 1);
  assert(addons_aceita_id(0, "movie", "tmdb:278") ==
         addons_aceita_id(0, "movie", "tt0111161"));

  puts("pengu: root/configured/authed install, object resources, tv, prefixes and empty catalogs PASS");
  return 0;
}
#else
#ifndef PENGU_STREAMS_SOURCE
#define PENGU_STREAMS_SOURCE "../src/streams.c"
#endif
#include PENGU_STREAMS_SOURCE

int p2p_ativo(void) { return 0; }
int debrid_ativo(void) { return 0; }
void fontefiltro_classificar(const FonteFiltroEntrada *entrada, FonteFiltroDados *out) {
  (void)entrada; (void)out;
}

int main(void) {
  char *body = fixture("stream");
  Stream *v = NULL;
  assert(stream_extrair(body, "PenguPlay", &v) == 1);
  assert(!strcmp(v[0].url, "https://pengu.uk/signin.mp4"));
  assert(!strcmp(v[0].rotulo, "PenguPlay"));
  assert(strstr(v[0].descricao, "authentication is missing"));
  assert(!v[0].notWebReady && !v[0].cabecalhos[0]);
  assert(fonteOferecida(v));
  puts("pengu: supplied unauthenticated sign-in stream PASS");
  free(v); free(body);

  // #565: authenticated captures — real responses behind the token path.
  // First entry is the externalUrl donate card (no url/infoHash): the parser
  // must skip it; every other stream is http(s) + notWebReady and must be
  // offered with P2P off (the v2.0.6 bug treated all notWebReady as P2P-only).
  // Also proves the "|^|" filename and huge videoSize parse.
  struct { const char *name; int expect; } auth[] = {
    { "stream-auth-movie_tt0111161", 27 },
    { "stream-auth-movie_tmdb_278", 27 },
    { "stream-auth-series_tt0903747_1_1", 26 },
  };
  for (unsigned k = 0; k < sizeof auth / sizeof *auth; k++) {
    body = fixture(auth[k].name);
    v = NULL;
    int n = stream_extrair(body, "PenguPlay", &v);
    assert(n == auth[k].expect);
    int pipe = 0;
    uint64_t maior = 0;
    for (int i = 0; i < n; i++) {
      assert(v[i].url[0] && !strncmp(v[i].url, "https://", 8));
      assert(v[i].notWebReady);
      assert(fonteOferecida(&v[i]));
      pipe |= strstr(v[i].arquivo, "|^|") != NULL;
      if (v[i].tamanhoBytes > maior) maior = v[i].tamanhoBytes;
    }
    assert(pipe && maior > 4000000000u);
    printf("pengu: %s -> %d playable http+notWebReady, externalUrl skipped, |^| filename PASS\n",
           auth[k].name, n);
    free(v); free(body);
  }

  // Controlled variant, not a captured authenticated response: isolate #552.
  body = fixture("stream");
  const char *hint = "\"notWebReady\":false";
  char *value = strstr(body, hint);
  assert(value);
  value += strlen(hint) - 5;
  memcpy(value, "true ", 5);
  assert(stream_extrair(body, "PenguPlay", &v) == 1 && v[0].notWebReady);
  int offered = fonteOferecida(v);
  free(v); free(body);
  if (!offered) {
    fputs("FAIL: public HTTP stream marked notWebReady requires P2P (#552)\n", stderr);
    return 1;
  }
  puts("pengu: public notWebReady stream with P2P off PASS");
  return 0;
}
#endif
