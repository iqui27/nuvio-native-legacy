// #748 (Samsung .wgt): "Movies in collections are not populating".
//
// MECANISMO PROVADO AQUI: uma pasta de colecao cujas fontes so tem addonId
// (sem addonBaseUrl no atalho — o caso "mixed" do configurator) abre VAZIA
// enquanto o manifesto do addon ainda nao chegou nesta TV. A tela nao exige
// sair e entrar de novo: vertudo_atualizar reconferere o endereco por quadro
// no estado semFonte e abre a fonte sozinha quando o addon responde.
//
//   bash tests/colecao748_late.sh
#include "../src/vertudo.c"
#include <assert.h>
#include <unistd.h>

static int falhar;
char *late748_baixar(const char *url, int timeout) {
  (void)timeout;
  // Sem rede: so os endpoints do fixture do configurator.
  assert(!strcmp(url, "https://example.invalid/catalog/movie/popular.json") ||
         !strcmp(url, "https://example.invalid/catalog/movie/recent.json") ||
         !strcmp(url, "https://example.invalid/catalog/series/popular.json"));
  if (falhar) return NULL;
  return strdup("{\"metas\":[{\"id\":\"tt0000001\",\"name\":\"Offline title\",\"poster\":\"offline.jpg\"}]}");
}
char *late748_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)headers;
  char *body = late748_baixar(url, timeout);
  *status = body ? 200 : 0;
  return body;
}

static void esperar(void) {
  for (int i = 0; i < 2000 && desc_vertudo_carregando(); i++) usleep(1000);
  assert(!desc_vertudo_carregando());
}
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) vertudo_atualizar(0.016f, (Uint32)(i * 16));
}

int main(int argc, char **argv) {
  char dir[600], json[4096];
  FILE *f;
  size_t n;
  assert(argc == 2);
  snprintf(dir, sizeof dir, "%s", argv[1]);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar("deploy/app/art");
  f = fopen("tests/fixtures/collections/configurator.json", "r"); assert(f);
  n = fread(json, 1, sizeof json - 1, f); assert(!ferror(f) && feof(f));
  json[n] = 0; fclose(f);
  assert(col_definir_json(json) == 2);

  // 1. O addon AINDA NAO chegou: a pasta abre sem um unico titulo.
  assert(!addons_base_por_id("offline.metadata")[0]);
  vertudo_colecao(col_folder(1)); esperar();
  assert(desc_vertudo_n() == 0);
  puts("ok pasta abre vazia enquanto o manifesto nao chegou");

  // 2. O manifesto responde (sync dos addons, como no .wgt): a pasta se
  //    preenche SOZINHA, sem sair e entrar.
  assert(addons_adicionar("Offline", "https://example.invalid/manifest.json"));
  addons_manifesto_lido(0, "{\"id\":\"offline.metadata\",\"name\":\"Offline\",\"resources\":[\"catalog\"]}");
  quadros(30); esperar();
  assert(desc_vertudo_n() == 1);
  { CatItem it;
    assert(desc_vertudo_item(0, &it) && !strcmp(it.titulo, "Offline title")); }
  puts("ok filmes entram sozinhos quando o addon responde");

  // 3. A outra aba (series) tambem popula.
  tabFocus = 1; tabCursor = 1; openSource(); esperar();
  assert(desc_vertudo_n() == 1);
  puts("ok a segunda fonte popula tambem");

  // 4. Falha de rede num catalogo que ainda nao tinha cache (recent, pasta 0).
  falhar = 1;
  vertudo_colecao(col_folder(0));
  source = 1;
  tabCursor = 1;
  tabFocus = 0;
  openSource();
  esperar();
  assert(desc_vertudo_erro());
  falhar = 0;
  puts("ok falha de rede vira erro, nao some");
  puts("colecao748_late: ok");
  return 0;
}
