// #414: imported collection, failed catalog, OK on the current tab retries.
#include "../src/vertudo.c"
#include <assert.h>
#include <unistd.h>

static int falhar;
char *config414_baixar(const char *url, int timeout) {
  (void)timeout;
  // No network: accept only the hand-written fixture's catalog endpoints.
  assert(!strcmp(url, "https://example.invalid/catalog/movie/popular.json") ||
         !strcmp(url, "https://example.invalid/catalog/movie/recent.json") ||
         !strcmp(url, "https://example.invalid/catalog/series/popular.json"));
  if (falhar) return NULL;
  return strdup("{\"metas\":[{\"id\":\"tt0000001\",\"name\":\"Offline title\",\"poster\":\"offline.jpg\"}]}");
}

char *config414_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)headers;
  char *body = config414_baixar(url, timeout);
  *status = body ? 200 : 0;
  return body;
}

static void esperar(void) {
  for (int i = 0; i < 2000 && desc_vertudo_carregando(); i++) usleep(1000);
  assert(!desc_vertudo_carregando());
}
static void tecla(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  vertudo_evento(&e);
}

int main(int argc, char **argv) {
  char dir[600], caminho[700], json[4096];
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
  assert(col_folder(0)->nSources == 2 && col_folder(1)->nSources == 2);
  // The second folder checks the real addonId resolution after sync.
  assert(addons_adicionar("Offline", "https://example.invalid/manifest.json"));
  addons_manifesto_lido(0, "{\"id\":\"offline.metadata\",\"name\":\"Offline\",\"resources\":[\"catalog\"]}");

  for (int estilo = VT_AB_CHIPS; estilo <= VT_AB_PAGINAS; estilo++) {
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "abasEstiloLocal %d\n", estilo); fclose(f); ajustes_dir(dados_dir());
    assert(ajustes_abas_estilo() == estilo);
    for (int pasta = 0; pasta < 2; pasta++) {
      gravarEscolha(col_folder(pasta), VT_LAY_GRADE);
      falhar = 1;
      // Leave the previous successful catalog before opening the next folder.
      desc_vertudo_abrir("https://example.invalid", "movie", "recent"); esperar();
      vertudo_colecao(col_folder(pasta)); esperar();
      assert(desc_vertudo_erro() && !desc_vertudo_n());
      falhar = 0;
      tecla(pasta ? SDLK_KP_ENTER : SDLK_RETURN);
      // Menu with two catalogs: OK opens the selector even while failed.
      assert(ddAberto == (estilo == VT_AB_MENU && !pasta));
      if (ddAberto) tecla(SDLK_RETURN); // Select the same failed catalog.
      esperar();
      assert(!desc_vertudo_erro() && desc_vertudo_n() == 1);
      CatItem it;
      assert(desc_vertudo_item(0, &it) && !strcmp(it.titulo, "Offline title"));
      if (estilo == VT_AB_MENU) {
        // The dropdown can already be open when the in-flight request fails.
        falhar = 1;
        desc_vertudo_abrir("https://example.invalid", "movie", "recent"); esperar();
        openSource(); esperar();
        assert(desc_vertudo_erro());
        tabFocus = 1; tabCursor = cursorInicial(); ddAberto = 0;
        falhar = 0;
        tecla(SDLK_RETURN);
        assert(ddAberto == !pasta);
        if (ddAberto) tecla(SDLK_RETURN);
        esperar();
        assert(!ddAberto && !desc_vertudo_erro() && desc_vertudo_n() == 1);
      }
      // A failed catalog must not hijack the layout button or a different tab.
      falhar = 1;
      desc_vertudo_abrir("https://example.invalid", "movie", "recent"); esperar();
      openSource(); esperar();
      assert(desc_vertudo_erro());
      tabFocus = 1; tabCursor = cabN();
      tecla(SDLK_RETURN); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);   // abre o seletor e escolhe
      assert(lay == VT_LAY_LINHA && tabCursor == cabN() && desc_vertudo_erro());
      falhar = 0;
      if (estilo == VT_AB_MENU && !pasta) {
        // Persistent failure: the selector still reaches the second catalog.
        falhar = 1;
        tabCursor = cursorInicial();
        tecla(SDLK_RETURN); assert(ddAberto && ddCursor == 0);
        tecla(SDLK_DOWN); falhar = 0;
      } else tabCursor = 1;
      tecla(SDLK_RETURN);
      esperar();
      assert(source == 1 && !desc_vertudo_erro() && desc_vertudo_n() == 1);
    }
  }
  puts("colecao_configurator: PASS (offline import, current-tab retry, all tab styles)");
  return 0;
}
