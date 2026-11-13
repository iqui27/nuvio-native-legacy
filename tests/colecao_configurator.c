// #414: imported collection, failed catalog, OK on the current tab retries.
#include "../src/vertudo.c"
#include <assert.h>
#include <unistd.h>

// #789: addon catalogs cap at VT_ADDON_MAX (16384); fixtures follow the cap
// with 1024-item pages so the bound stays tested without 164 round trips.
#define CAP VT_ADDON_MAX
#define PAG 1024
_Static_assert(CAP >= 14000 && CAP % PAG == 0, "cap covers 14k catalogs in whole pages");
_Static_assert(VT_MAX == 1000, "non-addon contract unchanged");

static int falhar;
char *config414_baixar(const char *url, int timeout) {
  (void)timeout;
  if (strstr(url, "/catalog/movie/cap") || strstr(url, "/catalog/movie/overflow") ||
      strstr(url, "/catalog/movie/single") || strstr(url, "/catalog/movie/straddle")) {
    const char *p = strstr(url, "skip=");
    int skip = p ? atoi(p + 5) : 0;
    int total = strstr(url, "/cap") ? CAP : CAP + 100;
    int page = strstr(url, "/single") ? CAP + 100 : strstr(url, "/straddle") ? 3000 : PAG;
    if (falhar) return NULL;
    size_t size = (size_t)page * 96 + 64;
    char *body = malloc(size); assert(body);
    int used = snprintf(body, size, "{\"metas\":[");
    for (int i = skip; i < skip + page && i < total; i++)
      used += snprintf(body + used, size - used,
        "%s{\"id\":\"tt%d\",\"name\":\"Item %d\",\"poster\":\"p.jpg\",\"releaseInfo\":\"%d\"}",
        i == skip ? "" : ",", i + 1, i + 1, 1900 + i % 120);
    snprintf(body + used, size - used, "]}");
    return body;
  }
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
  /* Real view + worker: failed later page leaves loaded cards actionable. */
  falhar = 0; vertudo_abrir("https://example.invalid", "movie", "cap", "Cap"); esperar();
  assert(desc_vertudo_n() == PAG);
  falhar = 1; desc_vertudo_mais(); esperar();
  assert(desc_vertudo_erro() && desc_vertudo_n() == PAG && celulaAceitaMenu());
  foco = 0; tecla(SDLK_RETURN);
  { SDL_Event e = {0}; e.type = SDL_KEYUP; e.key.keysym.sym = SDLK_RETURN; vertudo_evento(&e); }
  /* This harness has no global CatItem catalog, so opening cannot yield an index. */
  assert(desc_vertudo_erro() && desc_vertudo_n() == PAG && !desc_vertudo_carregando());
  falhar = 0; foco = PAG - 1; tecla(SDLK_RIGHT); esperar();
  assert(!desc_vertudo_erro() && desc_vertudo_n() == 2 * PAG);
  for (int i = 2; i < CAP / PAG; i++) { desc_vertudo_mais(); esperar(); }
  assert(desc_vertudo_n() == CAP && !desc_vertudo_fim());
  lay = VT_LAY_FLUXO; celLay = -1; orderN = -1; fluxoPendente = 0;
  vertudo_atualizar(0.016f, SDL_GetTicks()); esperar();
  assert(desc_vertudo_fim() && !desc_vertudo_erro());
  vertudo_atualizar(0.016f, SDL_GetTicks());
  assert(!fluxoPendente && orderN == CAP);
  vertudo_abrir("https://example.invalid", "movie", "overflow", "Overflow"); esperar();
  for (int i = 1; i < CAP / PAG; i++) { desc_vertudo_mais(); esperar(); }
  assert(desc_vertudo_n() == CAP && !desc_vertudo_fim());
  desc_vertudo_mais(); esperar();
  assert(desc_vertudo_erro() && !desc_vertudo_fim() && desc_vertudo_n() == CAP && desc_vertudo_pagina() == CAP);
  assert(celulaAceitaMenu());
  { CatItem item; char want[16]; snprintf(want, sizeof want, "tt%d", CAP);
    assert(desc_vertudo_item(0, &item) && !strcmp(item.imdb, "tt1"));
    assert(desc_vertudo_item(CAP - 1, &item) && !strcmp(item.imdb, want)); }
  vertudo_abrir("https://example.invalid", "movie", "single", "Single"); esperar();
  assert(desc_vertudo_erro() && !desc_vertudo_fim() && desc_vertudo_n() == CAP && desc_vertudo_pagina() == 0);
  { CatItem item; char want[16]; snprintf(want, sizeof want, "tt%d", CAP);
    assert(desc_vertudo_item(CAP - 1, &item) && !strcmp(item.imdb, want)); }
  desc_vertudo_mais(); esperar();
  assert(desc_vertudo_erro() && desc_vertudo_n() == CAP && desc_vertudo_pagina() == 0);
  vertudo_abrir("https://example.invalid", "movie", "straddle", "Straddle"); esperar();
  for (int i = 1; i < 6; i++) { desc_vertudo_mais(); esperar(); }
  assert(desc_vertudo_erro() && desc_vertudo_n() == CAP && desc_vertudo_pagina() == 15000);
  const char *ids[] = {"capfilter", "overflowfilter"};
  for (int j = 0; j < 2; j++) {
    vertudo_abrir("https://example.invalid", "movie", ids[j], "Filtered"); esperar();
    for (int i = 1; i < CAP / PAG; i++) { desc_vertudo_mais(); esperar(); }
    assert(desc_vertudo_n() == CAP && !desc_vertudo_fim() && !desc_vertudo_erro());
    vitTudo = 1; vitTudoFiltro = 1; vitTudoGrupo = 1; vitTudoBruto = -1; vitTudoMaisBruto = -1;
    assert(nItens() == 0);
    vitTudoAvancar(0); esperar();
    assert(j ? (desc_vertudo_erro() && !desc_vertudo_fim()) : (desc_vertudo_fim() && !desc_vertudo_erro()));
    vitTudo = 0; vitTudoFiltro = 0;
  }
  puts("colecao_configurator: PASS (offline import, current-tab retry, all tab styles)");
  return 0;
}
