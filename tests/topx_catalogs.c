// Exercise the production quota/parser helpers without network or TV services.
#include "../src/descoberta.c"
#include <assert.h>

const ColFolder *col_por_catalogo(const char *b, const char *t, const char *i) {
  (void)b; (void)t; (void)i; return NULL;
}
void col_chave_pasta(const ColFolder *f, char *s, unsigned n) { (void)f; if (n) s[0] = 0; }
int fil_oculta(const char *k) { (void)k; return 0; }
int fil_adicionada_na_tv(const char *k) { (void)k; return 0; }
int fil_escolhida(const char *k) { (void)k; return -1; }
int catordem_oculta(const char *k, const char *d) { (void)k; (void)d; return 0; }
void addons_catalogo_chave(const char *id, const char *base, const char *type,
                           const char *catalog, char *dst, unsigned size) {
  (void)base; snprintf(dst, size, "%s_%s_%s", id, type, catalog);
}
int catordem_n(void) { return 0; }
const char *catordem_chave(int i) { (void)i; return ""; }

int main(void) {
  char body[8192];
  FILE *f = fopen("tests/fixtures/topx_manifest.json", "rb");
  assert(f);
  size_t n = fread(body, 1, sizeof body - 1, f);
  assert(!ferror(f) && feof(f)); fclose(f); body[n] = 0;
  nPrefOrdem = 1;
  snprintf(prefOrdem[0], sizeof prefOrdem[0], "com.topx.fixture_movie_tx.in.amazon-prime.movie");
  int eligible = 0, promoted = 0;
  char *chosen = escolherPelaCota(body, body + n, "com.topx.fixture", "https://topx.example", 2,
                                 &eligible, &promoted);
  assert(eligible == 3 && chosen);
  // Only the last catalog is explicitly ordered; nested IDs must not hide it.
  assert(chosen[0] && !chosen[1] && chosen[2]);
  assert(promoted == 1);
  free(chosen);
  int count = 0;
  for (const char *p = js_array(body, NULL, "catalogs"); p; p = js_prox(js_fim(p))) {
    const char *end = js_fim(p);
    char name[96] = "", type[8] = "", id[96] = "", url[512];
    assert(!exigeBusca(p, end) && aceitaSkip(p, end));
    assert(js_texto_raiz_em(p, end, "name", name, sizeof name) && name[0] == '#');
    assert(js_texto_raiz_em(p, end, "type", type, sizeof type));
    assert(js_texto_raiz_em(p, end, "id", id, sizeof id));
    assert(catalogoUrl(url, sizeof url, "https://topx.example/c/fixture", type, id, "") > 0);
    assert(strstr(url, "/catalog/") && strstr(url, id));
    count++;
  }
  assert(count == 3);
  // Synthetic personalized catalog: no account URL or network involved.
  const char *personal = "{\"extraRequired\":[\"genre\",\"skip\"],\"extra\":["
    "{\"name\":\"genre\",\"options\":[\"For you & me\",\"Other\"]},"
    "{\"name\":\"skip\"}]}";
  const char *base = "https://fixture.example/user/config?key=fixture";
  registrarNomeCatalogo(base, "movie", "ai", "AI for you");
  registrarExtrasCatalogo(base, "movie", "ai", personal, js_fim(personal));
  char url[1024];
  assert(catalogoUrl(url, sizeof url, base, "movie", "ai", "") > 0);
  assert(!strcmp(url, "https://fixture.example/user/config/catalog/movie/ai/genre=For%20you%20%26%20me&skip=0.json?key=fixture"));
  assert(catalogoUrl(url, sizeof url, base, "movie", "ai", "genre=Other&skip=24") > 0);
  assert(!strcmp(url, "https://fixture.example/user/config/catalog/movie/ai/genre=Other&skip=24.json?key=fixture"));
  const char *modern = "{\"extra\":[{\"name\":\"genre\",\"isRequired\":true,\"options\":[\"Drama\"]},"
    "{\"name\":\"search\"}]}";
  registrarNomeCatalogo(base, "series", "ai", "AI for you");
  registrarExtrasCatalogo(base, "series", "ai", modern, js_fim(modern));
  assert(catalogoUrl(url, sizeof url, base, "series", "ai", "") > 0);
  assert(strstr(url, "/genre=Drama.json?key=fixture"));
  const char *optional = "{\"extra\":[{\"name\":\"genre\",\"options\":[\"Drama\"]}]}";
  registrarNomeCatalogo(base, "series", "ai", "AI for you");
  registrarExtrasCatalogo(base, "series", "ai", optional, js_fim(optional));
  assert(catalogoUrl(url, sizeof url, base, "series", "ai", "") > 0);
  assert(strstr(url, "/ai.json?key=fixture"));
  const char *search = "{\"extraRequired\":[\"search\"]}";
  assert(exigeBusca(search, js_fim(search)));
  puts("PASS: catalog identity, quota, required defaults, overrides and config paths");
  return 0;
}
