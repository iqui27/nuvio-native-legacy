// Exercise the real staging, Xtream/M3U callbacks, cache and fallback paths.
#include "guiafiltro.h"
#include <errno.h>
#include "xtream.h"
static int inventoryFixture(XtreamCb cb, void *u, int *total);
#ifdef GUIAFILTRO_LEGACY_RULES
// Historical rule implementations predate source-qualified entry points.
int guiafiltro_canal_oculto_fonte(const GuiaFiltro *f, const char *fonte, const char *id) {
  (void)fonte; return guiafiltro_canal_oculto(f, id);
}
int guiafiltro_canal_alternar_fonte(GuiaFiltro *f, const char *fonte, const char *id,
                                   const char *nome, const char *cat) {
  (void)fonte; return guiafiltro_canal_alternar(f, id, nome, cat);
}
#endif
static char *filtroResposta(const char *url);
#define GUIAFILTRO_LOADER
#define main cache_test_main
#include "guiacache.c"
#undef main

// Historical sources still admit incrementally; current staging keeps candidates.
#if defined(G_CANDIDATOS_MAX)
#define GUIAFILTRO_FINAL_ADMISSION 1
#endif
static void rebuild(void) {
#ifdef GUIAFILTRO_FINAL_ADMISSION
  selecionarSecoes();
#endif
}
static int begin(int modo, int teto) {
  xtOk = 1;
  memset(&fltLive, 0, sizeof fltLive);
  fltLive.modo = modo;
  sFltLive = fltLive;
  stagingZerar(); sTeto = teto;
  return sFonte(LTV_XTREAM, "", "Fixture", "");
}
static void channel(int fl, const char *cat, const char *id, const char *nome) {
  GCru c = {0};
  c.id = id; c.nome = nome; c.grupo = cat;
  c.logo = c.desc = c.epgId = c.tvgNome = c.url = c.ua = c.ref = c.tipo = "";
  sGrupoInfo(fl, cat, 1);
  sCanalAdd(fl, &c);
}
void desc_remontar_fileiras(void) {}
void desc_repetir(void) {}
int player_aberto(void) { return 0; }
int segurar_venceu(Uint32 now, Uint32 since, const char *screen) {
  (void)now; (void)since; (void)screen; return 0;
}
void teclado_atualizar(float dt, Uint32 now) { (void)dt; (void)now; }
int teclado_resultado(void) { return 0; }
const char *teclado_texto(void) { return ""; }
int xtream_epg_curto(const char *id, XtreamProg *out, int cap, int *status) {
  (void)id; (void)out; (void)cap; *status = 0; return 0;
}
int xtream_ultimo_retry_after(void) { return 0; }
char *rede_baixar_bin_medido_controle(const char *url, int seconds, const char *const *headers,
                                     const RedeControle *control, long *size, RedeMedida *measure) {
  (void)url; (void)seconds; (void)headers; (void)control; (void)measure; *size = 0; return NULL;
}
static int inventoryRefetch, inventoryRequests;
static int inventoryFixture(XtreamCb cb, void *u, int *total) {
  if (!inventoryRefetch) return 0;
  inventoryRequests++;
  char id[GF_ID], name[GF_NOME];
  memset(id, 'i', sizeof id - 1); id[sizeof id - 1] = 0;
  memset(name, 'n', sizeof name - 1); name[sizeof name - 1] = 0;
  XtreamItem item = {0};
  item.id = id; item.nome = name; item.categoria = "Sports";
  item.logo = item.epgId = ""; item.catOrdem = -1;
  for (int i = 0; i < 20000; i++) {
    char prefix[32]; snprintf(prefix, sizeof prefix, "xtream:%014d", i);
    memcpy(id, prefix, strlen(prefix));
    cb(&item, u);
  }
  if (total) *total = 20000;
  return 1;
}
static int pageRequests, pageScenario, sourceScenario, sourceRequests;
static int discoveryRequests, loadRequests;
int cat_n_fileiras(void) { discoveryRequests++; return 0; }
const CatFileira *cat_fileira(int i) { (void)i; return NULL; }
// Keep all test work foreground; record load launches without creating threads.
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *u) {
  (void)t; (void)a; (void)fn; (void)u; loadRequests++; return inventoryRefetch ? 0 : EAGAIN;
}
int pthread_detach(pthread_t t) { (void)t; return 0; }
static char *filtroResposta(const char *url) {
  char body[24000];
  size_t used = 0;
  if (sourceScenario) {
    sourceRequests++;
    if (sourceScenario == 3) return NULL;
    if (!strcmp(url, "xtream")) return strdup(sourceScenario == 1 ? "X 4K" : "X HD");
    if (strstr(url, ".m3u")) return strdup(sourceScenario == 1 ?
      "#EXTM3U\n#EXTINF:-1 tvg-id=\"x\" group-title=\"Sports\",X 4K\nhttps://fixture.invalid/live\n" :
      "#EXTM3U\n#EXTINF:-1 tvg-id=\"x\" group-title=\"Sports\",X HD\nhttps://fixture.invalid/live\n");
    assert(strstr(url, "/catalog/tv/channels"));
    return strdup(sourceScenario == 1 ?
      "{\"metas\":[{\"id\":\"channel:1\",\"name\":\"X 4K\",\"genre\":[\"Sports\"]}]}" :
      "{\"metas\":[{\"id\":\"channel:1\",\"name\":\"X HD\",\"genre\":[\"Sports\"]}]}");
  }
  if (!pageScenario) return NULL;
  assert(strstr(url, "/catalog/tv/channels"));
  pageRequests++;
  if (pageScenario >= 3 && pageRequests > 300) {
    if (pageScenario == 4 && pageRequests == 301)
      return strdup("{\"metas\":[{\"id\":\"late-4k\",\"name\":\"Station0 4K\",\"genre\":[\"Sports\"]}]}");
    return strdup("{\"metas\":[]}");
  }
  used += (size_t)snprintf(body + used, sizeof body - used, "{\"metas\":[");
  int offset = pageScenario >= 3 ? (pageRequests - 1) * 100 :
               pageScenario == 2 && pageRequests > 1 ? 100 : 0;
  int count = pageScenario == 2 && pageRequests > 1 ? 1 : 100;
  for (int i = 0; i < count; i++) {
    char name[64];
    if (pageScenario >= 3) snprintf(name, sizeof name, "Station%d %s", offset + i, pageScenario == 4 ? "HD" : "4K");
    else snprintf(name, sizeof name, "X %s", offset ? "4K" : "HD");
    used += (size_t)snprintf(body + used, sizeof body - used,
      "%s{\"id\":\"channel:%d\",\"name\":\"%s\",\"genre\":[\"%s\"]}",
      i ? "," : "", offset + i, name, pageScenario != 4 && i == 0 ? "Hidden" : "Sports");
  }
  snprintf(body + used, sizeof body - used, "]}");
  return strdup(body);
}
static void paginationRegression(int mode) {
  memset(&fltLive, 0, sizeof fltLive); fltLive.modo = mode; sFltLive = fltLive;
  guiafiltro_cat_alternar(&sFltLive, "Hidden");
  sNFontes = nAddonsCarga = 1; nCargaUrls = 0;
  addonsCarga = calloc(1, sizeof *addonsCarga);
  strcpy(addonsCarga[0].base, "https://fixture.invalid"); addonsCarga[0].ativo = 1;
  memset(sFontes, 0, sizeof sFontes);
  strcpy(sFontes[0].base, addonsCarga[0].base);
  strcpy(sFontes[0].id, "channels"); strcpy(sFontes[0].tipo, "tv");
  pageRequests = 0; pageScenario = 1; cargaCancelada = 0;
  fioGuia(NULL);
  printf("pagination mode=%d requests=%d (expected 2)\n", mode, pageRequests); fflush(stdout);
  assert(pageRequests == 2);
  // A full filtered page must not hide a later, distinct 4K page.
  if (mode == GF_SO_4K) {
    sFltLive.nCat = 0;
    nAddonsCarga = 1; addonsCarga = calloc(1, sizeof *addonsCarga);
    strcpy(addonsCarga[0].base, sFontes[0].base); addonsCarga[0].ativo = 1;
    pageRequests = 0; pageScenario = 2;
    fioGuia(NULL);
    assert(pageRequests == 2 && sNCanais == 1 && !strcmp(sCanais[0].nome, "X 4K"));
  }
  pageScenario = 0;
}
static void capRegression(void) {
  paginationRegression(GF_TODAS);
  memset(&sFltLive, 0, sizeof sFltLive);
  tetoCarga = 900;
  nAddonsCarga = 1; addonsCarga = calloc(1, sizeof *addonsCarga);
  strcpy(addonsCarga[0].base, sFontes[0].base); addonsCarga[0].ativo = 1;
  pageRequests = 0; pageScenario = 3;
  fioGuia(NULL);
  rebuild();
  printf("cap default requests=%d (maximum 9)\n", pageRequests); fflush(stdout);
  assert(pageRequests <= 9 && sNCanais == 900);
#ifdef GUIAFILTRO_FINAL_ADMISSION
  // Active rules retain the bounded scan for later candidates and upgrades.
  for (int mode = GF_TODAS; mode < GF_MODOS; mode++) {
    memset(&sFltLive, 0, sizeof sFltLive); sFltLive.modo = mode;
    if (mode == GF_TODAS) guiafiltro_cat_alternar(&sFltLive, "Unused");
    nAddonsCarga = 1; addonsCarga = calloc(1, sizeof *addonsCarga);
    strcpy(addonsCarga[0].base, sFontes[0].base); addonsCarga[0].ativo = 1;
    pageRequests = 0;
    fioGuia(NULL); rebuild();
    printf("cap filtered mode=%d requests=%d\n", mode, pageRequests); fflush(stdout);
    assert(pageRequests >= 300 && pageRequests <= 301 && sNCanais == 900);
  }
#endif
}
#ifdef GUIAFILTRO_STALKER_TEST
static void addonLateUpgradeRegression(void) {
  begin(GF_PREFERIR_4K, 900);
  GFonte f = {0};
  strcpy(f.base, "https://fixture.invalid"); strcpy(f.tipo, "tv"); strcpy(f.id, "channels");
  nAddonsCarga = 1; addonsCarga = calloc(1, sizeof *addonsCarga);
  strcpy(addonsCarga[0].base, f.base); addonsCarga[0].ativo = 1;
  int fl = sFonte(LTV_ADDON, f.base, "Fixture", f.base);
  pageScenario = 4; pageRequests = 0;
  assert(carregarCatalogo(&f, fl));
  rebuild();
  assert(sNCanais == 900 && !strcmp(sCanais[0].id, "late-4k"));
  assert(pageRequests == 301);
  puts("PASS: addon upgrade beyond candidate budget");
}
#endif
static void cacheRefreshRegression(void) {
  for (int mode = GF_SO_4K; mode < GF_MODOS; mode++) {
    int fl = begin(mode, 5);
    guiafiltro_cat_alternar(&sFltLive, "Hidden");
    guiafiltro_canal_alternar(&sFltLive, "xtream:4", "Z 4K", "Sports");
    channel(fl, "Sports", "xtream:1", "X HD");
    channel(fl, "Sports", "xtream:2", "X 4K");
    channel(fl, "Hidden", "xtream:3", "Y HD");
    channel(fl, "Sports", "xtream:4", "Z 4K");
    adotarStaging(); cacheGravar();
    begin(GF_TODAS, 10);
    int restored = cacheFonteLer("xt");
    printf("cache mode=%d restored=%d (expected 4)\n", mode, restored); fflush(stdout);
    assert(restored == 4);
    assert(!strcmp(sCanais[0].nome, "X HD") && !strcmp(sCanais[3].nome, "Z 4K"));
    // Same selected ID, changed raw metadata: publication must still refresh disk.
    fl = begin(mode, 5); fltLive = sFltLive;
    channel(fl, "Sports", "xtream:2", "X 4K"); publicar();
    fl = begin(mode, 5);
    channel(fl, "Sports", "xtream:2", "X 4K");
    channel(fl, "Sports", "xtream:1", "X SD"); publicar();
    begin(GF_TODAS, 10); assert(cacheFonteLer("xt") == 2);
    assert(!strcmp(sCanais[1].nome, "X SD"));
    // A successful source filtered to zero must also refresh its raw cache.
    fl = begin(GF_SO_4K, 5); fltLive = sFltLive;
    channel(fl, "Sports", "xtream:5", "New SD"); publicar();
    begin(GF_TODAS, 10); assert(cacheFonteLer("xt") == 1);
    assert(!strcmp(sCanais[0].nome, "New SD"));
  }
}
static void filteredEmptyRegression(int type, int hidden, int retry) {
  const char *ref = type == LTV_M3U ? "https://fixture.invalid/list.m3u" :
                    type == LTV_ADDON ? "https://fixture.invalid" : "";
  char key[LTV_CHAVE];
  ltv_chave_fonte(type, ref, key, sizeof key);
  xtOk = type == LTV_XTREAM;
#ifdef NV_IPTVFONTES_H
  nCargaContas=iptv_contas(cargaContas,IPTV_MAX_CONTAS);
#endif
  nCargaUrls = type == LTV_M3U;
  if (nCargaUrls) {
    cargaUrls[0].tipo = LTV_URL_M3U;
    strcpy(cargaUrls[0].url, ref); strcpy(cargaUrls[0].nome, "Fixture");
  }
  nAddons = type == LTV_ADDON;
  bases[0] = ref;
  memset(sFontes, 0, sizeof sFontes);
  if (nAddons) {
    strcpy(sFontes[0].base, ref); strcpy(sFontes[0].tipo, "tv");
    strcpy(sFontes[0].id, "channels");
  }
  // Seed stale 4K cache through the actual provider load and publication.
  for (int pass = 1; pass <= 2; pass++) {
    memset(&fltLive, 0, sizeof fltLive);
    if (pass == 2) {
      if (hidden) guiafiltro_cat_alternar(&fltLive, "Sports");
      else fltLive.modo = GF_SO_4K;
    }
    sFltLive = fltLive; sourceScenario = retry ? 2 : pass;
    sNFontes = nAddons;
    nAddonsCarga = nAddons;
    if (nAddons) {
      addonsCarga = calloc(1, sizeof *addonsCarga);
      strcpy(addonsCarga[0].base, ref); addonsCarga[0].ativo = 1;
    }
    fioGuia(NULL);
    if (pass == 2 && !retry) {
      assert(sNCanais == 0 && !sLtvF[0].deCache && sLtvF[0].saude == LTV_SAUDE_OK);
    }
    drenarCarga();
    assert(nCanais == (pass == 1 ? 1 : 0));
  }
  if (retry) {
    aberta = 1; previewLigado = 0; ultTentativa = 1;
    int requests = sourceRequests;
    for (Uint32 now = 11002; now < 50000; now += 11001) guia_atualizar(0.016f, now);
    assert(loadRequests == 0 && discoveryRequests == 0 && sourceRequests == requests);
    // Explicit refresh and configuration changes still launch loads.
    guia_limpar_lista(); assert(loadRequests == 1);
    guia_fontes_mudaram(); assert(loadRequests == 2);
    estado = G_FALHOU; ultTentativa = 1;
    guia_atualizar(0.016f, 60000);
    assert(loadRequests == 3 && discoveryRequests > 0);
  } else {
    // Fresh raw HD replaces stale 4K on disk; failure still uses that cache.
    memset(&sFltLive, 0, sizeof sFltLive);
    stagingZerar(); sTeto = 10;
    assert(cacheFonteLer(key) == 1 && !strcmp(sCanais[0].nome, "X HD"));
    sourceScenario = 3; sNFontes = nAddons;
    fioGuia(NULL);
    assert(sNCanais == 1 && sLtvF[0].deCache && sLtvF[0].saude == LTV_SAUDE_DEGRADADA);
  }
  sourceScenario = 0;
}
static void visibilityRegression(void) {
  int fl = begin(GF_PREFERIR_4K, 5);
  channel(fl, "Sports", "xtream:1", "X 4K");
  channel(fl, "News", "xtream:2", "Y 4K");
  channel(fl, "News", "xtream:3", "Z 4K");
  adotarStaging(); cacheGravar();
  char name[64]; cacheNomeFonte("xt", name, sizeof name);
  char *cached = dados_ler(name); free(cached);
  filtrosAbrir();
  guiafiltro_cat_alternar(&fltLive, "Sports");
  guiafiltro_canal_alternar(&fltLive, "xtream:2", "Y 4K", "News");
  fSujo = 1; fioVivo = 1; // Exercise cancellation without starting a thread.
  painelFechar(); fioVivo = 0;
  assert(nCanais == 1 && !strcmp(canais[0].id, "xtream:3"));
  assert(nCats == 1 && nZapL == 1 && ltvF[0].nCanais == 1);
  sNFontes = nAddonsCarga = nCargaUrls = 0; cargaCancelada = 0;
  sFltLive = fltLive; fioGuia(NULL); publicar();
  assert(nCanais == 1 && !strcmp(canais[0].id, "xtream:3"));
  filtrosAbrir(); guiafiltro_cat_alternar(&fltLive, "News");
  fSujo = 1; fioVivo = 1; painelFechar(); fioVivo = 0;
  assert(nCanais == 0 && nCats == 0 && nZapL == 0);
}
static void variantsRegression(void) {
  int fl = begin(GF_PREFERIR_4K, 5);
  channel(fl, "Sports", "xtream:1", "X SD");
  channel(fl, "Sports", "xtream:2", "X HD");
  channel(fl, "Sports", "xtream:3", "X FHD");
  channel(fl, "Sports", "xtream:4", "X 4K");
  adotarStaging();
  GuiaVariante v[5];
  assert(guia_variantes("xtream:4", v, 3) == 3);
  assert(!strcmp(v[0].nome, "X 4K") && !strcmp(v[1].nome, "X FHD") && !strcmp(v[2].nome, "X HD"));
  assert(guia_variantes("xtream:4", v, 1) == 1 && !strcmp(v[0].nome, "X 4K"));
  guiafiltro_canal_alternar(&fltLive, "xtream:3", "X FHD", "Sports");
  assert(guia_variantes("xtream:4", v, 3) == 3);
  assert(!strcmp(v[1].nome, "X HD") && !strcmp(v[2].nome, "X SD"));
}
static void siblingVisibilityRegression(void) {
  const char *ids[] = {"xtream:1", "xtream:2", "xtream:3"};
  const char *names[] = {"X 4K", "X FHD", "X HD"};
  // Literal winners: [quality mode][arrival order][disabled resolution].
  const int winners[GF_MODOS][2][3] = {
    {{2, 1, 1}, {3, 3, 2}},
    {{0, 1, 1}, {0, 1, 1}},
    {{2, 1, 1}, {2, 1, 1}}
  };
  for (int mode = 0; mode < GF_MODOS; mode++)
    for (int split = 0; split < 2; split++)
      for (int reverse = 0; reverse < 2; reverse++)
        for (int hidden = 2; hidden >= 0; hidden--) {
          const char *groups[] = {split ? "UHD" : "Sports", "Sports", split ? "HD" : "Sports"};
          int fl = begin(mode, 1), winner = winners[mode][reverse][hidden];
          for (int i = 0; i < 3; i++) {
            int k = reverse ? 2 - i : i;
            channel(fl, groups[k], ids[k], names[k]);
          }
          adotarStaging(); cacheGravar();
          // Use the actual per-ID switch, then reload its persisted profile rules.
          filtrosAbrir(); fNivel = 1;
          snprintf(fCatNome, sizeof fCatNome, "%s", groups[hidden]);
          fMontarCanais(fCatNome);
          for (paFoco = 0; paFoco < fN && strcmp(fIt[paFoco].id, ids[hidden]); paFoco++) {}
          assert(paFoco < fN && !fIt[paFoco].oculto);
          filtrosOk();
          fioVivo = 1; painelFechar(); fioVivo = 0;
          fltPerfil = -1; fltGarantir();
          assert(fltLive.nCan == 1 && guiafiltro_canal_oculto(&fltLive, ids[hidden]));
          // Online admission and offline replay must select the same enabled ID.
          for (int offline = 0; offline < 2; offline++) {
            stagingZerar(); sTeto = 1; sFltLive = fltLive;
            if (offline) cacheFonteLer("xt");
            else {
              fl = sFonte(LTV_XTREAM, "", "Fixture", "");
              for (int i = 0; i < 3; i++) {
                int k = reverse ? 2 - i : i;
                channel(fl, groups[k], ids[k], names[k]);
              }
            }
            rebuild();
            assert(sNCanais == (winner != 0));
            if (winner) assert(!strcmp(sCanais[0].id, ids[winner - 1]));
            adotarStaging(); cacheGravar();
            if (winner) {
              GuiaVariante variants[3];
              int n = guia_variantes(ids[winner - 1], variants, 3);
              assert(n == (mode == GF_PREFERIR_4K ? 2 : 1));
              for (int i = 0; i < n; i++) assert(strcmp(variants[i].id, ids[hidden]));
            }
          }
          // Hidden and collapsed candidates remain recoverable from the raw cache.
          begin(GF_TODAS, 3);
          assert(cacheFonteLer("xt") == 3);
        }
}
static void sourceVisibilityRegression(void) {
  nAddons = 2; bases[0] = "https://a.invalid"; bases[1] = "https://b.invalid";
  for (int mode = GF_TODAS; mode < GF_MODOS; mode++) {
    begin(mode, 10);
    int a = sFonte(LTV_ADDON, "https://a.invalid", "A", "https://a.invalid");
    int b = sFonte(LTV_ADDON, "https://b.invalid", "B", "https://b.invalid");
    channel(a, "News", "tv:news", "News 4K");
    channel(b, "News", "tv:news", "News 4K");
    adotarStaging(); cacheGravar();
    filtrosAbrir(); fNivel = 1; strcpy(fCatNome, "News"); fMontarCanais("News");
    assert(fN == 2);
    paFoco = 0; filtrosOk();
    assert(fIt[0].oculto && !fIt[1].oculto);
    empacotar();
    assert(nCanais == 1 && !strcmp(canais[0].base, "https://b.invalid"));
    fltPerfil = -1; fltGarantir();
    for (int offline = 0; offline < 2; offline++) {
      stagingZerar(); sTeto = 10; sFltLive = fltLive;
      if (offline) {
        char key[LTV_CHAVE];
        ltv_chave_fonte(LTV_ADDON, "https://a.invalid", key, sizeof key); cacheFonteLer(key);
        ltv_chave_fonte(LTV_ADDON, "https://b.invalid", key, sizeof key); cacheFonteLer(key);
      } else {
        a = sFonte(LTV_ADDON, "https://a.invalid", "A", "https://a.invalid");
        b = sFonte(LTV_ADDON, "https://b.invalid", "B", "https://b.invalid");
        channel(a, "News", "tv:news", "News 4K");
        channel(b, "News", "tv:news", "News 4K");
      }
      adotarStaging();
      assert(nCanais == 1 && !strcmp(canais[0].base, "https://b.invalid"));
      fMontarCanais("News"); assert(fN == 2);
    }
  }
}
#if defined(G_INVENTARIO_BLOCO) || defined(GUIAFILTRO_SPOOL_TEST)
static long inventorySpoolBytes(FILE *fp) {
  if (!fp) return 0;
  long pos = ftell(fp);
  assert(pos >= 0 && fseek(fp, 0, SEEK_END) == 0);
  long bytes = ftell(fp);
  assert(bytes >= 0 && fseek(fp, pos, SEEK_SET) == 0);
  return bytes;
}
static void inventorySpoolRegression(void) {
  for (int pass = 0; pass < 2; pass++) {
    int fl = begin(GF_TODAS, 900);
    for (int cat = 0; cat < 1000; cat++) {
      char group[32]; snprintf(group, sizeof group, "Category %d", cat);
      for (int i = 0; i < 64; i++) {
        char id[40]; snprintf(id, sizeof id, "xtream:%d:%d", cat, i);
        channel(fl, group, id, "News HD");
      }
    }
    long bytes = inventorySpoolBytes(sInventarioDisco);
    long peak = bytes + inventorySpoolBytes(inventarioDisco);
    printf("inventory spool: %ld bytes, refresh peak: %ld bytes\n", bytes, peak); fflush(stdout);
    assert(bytes <= G_INVENTARIO_BYTES_MAX);
    assert(peak <= 2 * G_INVENTARIO_BYTES_MAX);
    adotarStaging();
    assert(!sInventarioDisco && nCanais == 900);
    filtrosAbrir(); assert(fN == 1000);
    for (int cat = 0; cat < 1000; cat += cat == 0 ? 500 : 499) {
      char group[32]; snprintf(group, sizeof group, "Category %d", cat);
      fMontarCanais(group); assert(fN == 64);
    }
  }
  // Long, distinct records exhaust the byte budget; late controls must survive.
  int fl = begin(GF_TODAS, 900);
  char id[GF_ID], name[GF_NOME];
  memset(id, 'i', sizeof id - 1); id[sizeof id - 1] = 0;
  memset(name, 'n', sizeof name - 1); name[sizeof name - 1] = 0;
  for (int i = 0; i < 20000; i++) {
    char prefix[32]; snprintf(prefix, sizeof prefix, "xtream:%014d", i);
    memcpy(id, prefix, strlen(prefix));
    channel(fl, "Sports", id, name);
  }
  channel(fl, "Late", "xtream:late", "Late HD");
  long bytes = inventorySpoolBytes(sInventarioDisco);
  assert(bytes == G_INVENTARIO_BYTES_MAX);
  assert(bytes + inventorySpoolBytes(inventarioDisco) <= 2 * G_INVENTARIO_BYTES_MAX);
  adotarStaging();
  filtrosAbrir(); fMontarCanais("Late");
  assert(fN == 1 && !strcmp(fIt[0].id, "xtream:late"));
  fMontarCanais("Sports");
  inventoryRefetch = xtOk = 1;
  nFontes = sNFontes = nAddons = nAddonsCarga = nCargaUrls = 0;
  cargaCancelada = 0;
  fNivel = 1; snprintf(fCatNome, sizeof fCatNome, "Sports");
  char middle[GF_ID]; memcpy(middle, id, sizeof middle);
  memcpy(middle, "xtream:00000000002000", 21);
  int found = 0, middleFound = 0, pages = 0;
  do {
    for (int i = 0; i < fN; i++) {
      if (!strcmp(fIt[i].id, id)) found++;
      if (!strcmp(fIt[i].id, middle)) {
        middleFound++;
        assert(guiafiltro_canal_alternar_fonte(&fltLive, fIt[i].fonte, middle, name, "Sports"));
        assert(guiafiltro_canal_oculto_fonte(&fltLive, fIt[i].fonte, middle));
        assert(!guiafiltro_canal_alternar_fonte(&fltLive, fIt[i].fonte, middle, name, "Sports"));
      }
    }
    if (!fProximo) break;
    assert(++pages < 20);
    int launches = loadRequests;
    fMontarCanaisPagina("Sports", fProximo);
    if (loadRequests != launches) {
      fioGuia(NULL); drenarCarga();
      assert(nCanais == 900);
      assert(!sInventarioDisco && sNInventario == 0);
    }
    assert(fN <= G_PF_CANAIS_MAX + 1);
  } while (1);
  assert(found >= 1);
  assert(middleFound >= 1);
  assert(inventoryRequests == 5);
  inventoryRefetch = xtOk = 0;
  stagingZerar(); assert(!sInventarioDisco);
}
#endif
static void inventoryBoundRegression(void) {
#ifdef G_INVENTARIO_POR_CAT
  for (int pass = 0; pass < 2; pass++) {
    int fl = begin(GF_TODAS, 900);
    for (int cat = 0; cat < 1000; cat++) {
      char group[32]; snprintf(group, sizeof group, "Category %d", cat);
      for (int i = 0; i < 64; i++) {
        char id[40]; snprintf(id, sizeof id, "xtream:%d:%d", cat, i);
        channel(fl, group, id, "News HD");
      }
    }
    printf("inventory: %d entries, %zu bytes\n", sNInventario, (size_t)sNInventario * sizeof *sInventario);
    fflush(stdout);
    assert(sNInventario <= 4000);
    assert((size_t)sNInventario * sizeof *sInventario <= 4 * 1024 * 1024);
    adotarStaging();
    assert(!sInventario && !sNInventario && nInventario <= 4000);
    filtrosAbrir(); assert(fN == 1000);
    fMontarCanais("Category 0"); assert(fN > 0);
#ifdef G_INVENTARIO_MAX
    // Callbacks keep late category counts but stop constructing channel controls.
    fl = begin(GF_TODAS, 900);
    M3uCtx mctx = {0}; mctx.fl = fl;
    for (int cat = 0; cat < 1000; cat++) {
      char group[32]; snprintf(group, sizeof group, "Category %d", cat);
      for (int i = 0; i < 64; i++) {
        char name[40]; snprintf(name, sizeof name, "Station %d %d HD", cat, i);
        LtvM3uCanal m = {0};
        m.nome = name; m.grupo = group; m.tvgId = ""; m3uCanal(&m, &mctx);
      }
    }
    assert(sNInventario == 4000 && sNCanais == 900 && sLtvF[fl].ngi == 1000);
    assert(sLtvF[fl].total == 64000);
    adotarStaging();
    filtrosAbrir(); assert(fN == 1000);
    fl = begin(GF_TODAS, 900);
    XtCtx x = {0}; x.fl = fl;
    for (int cat = 0; cat < 1000; cat++) {
      char group[32]; snprintf(group, sizeof group, "Category %d", cat);
      xtCat(group, cat, 64, &x);
      for (int i = 0; i < 64; i++) {
        char id[40]; snprintf(id, sizeof id, "xtream:%d:%d", cat, i);
        XtreamItem it = {0}; it.id = id; it.nome = "News HD"; it.categoria = group; it.catOrdem = cat;
        xtCanal(&it, &x);
      }
    }
    assert(sNInventario == 4000 && sNCanais == 900 && sLtvF[fl].ngi == 1000);
    free(x.giDe); adotarStaging();
#endif
  }
#endif
}
static void categoriesRegression(void) {
  int fl = begin(GF_PREFERIR_4K, 1);
  channel(fl, "Sports", "xtream:1", "X FHD");
  channel(fl, "UHD", "xtream:2", "X 4K");
  rebuild();
  assert(sNCanais == 1 && !strcmp(sCanais[0].id, "xtream:2"));
  adotarStaging();
  GuiaVariante v[3];
  assert(guia_variantes("xtream:2", v, 3) == 2);
  fl = begin(GF_TODAS, 3);
  channel(fl, "Sports", "xtream:1", "X FHD");
  channel(fl, "UHD", "xtream:2", "X 4K");
  adotarStaging();
  assert(guia_variantes("xtream:2", v, 3) == 2);
}
static void inventoryRegression(void) {
  for (int mode = GF_TODAS; mode < GF_MODOS; mode++) {
    int fl = begin(mode, 5);
    if (mode == GF_TODAS) guiafiltro_cat_alternar(&sFltLive, "Sports");
    channel(fl, "Sports", "xtream:1", "X FHD");
    channel(fl, "Sports", "xtream:2", "X 4K");
    adotarStaging(); cacheGravar();
    filtrosAbrir(); fMontarCanais("Sports");
    assert(fN == 2);
    assert(!strcmp(fIt[0].id, "xtream:1") && !strcmp(fIt[1].id, "xtream:2"));
    // Offline inventory must remain available after staging reset.
    begin(mode, 5); cacheFonteLer("xt"); adotarStaging();
    filtrosAbrir(); fMontarCanais("Sports"); assert(fN == 2);
  }
}
static void lateInventoryRegression(void) {
  for (int scenario = 0; scenario < 3; scenario++) {
    int fl = begin(scenario == 1 ? GF_TODAS : GF_SO_4K, 5);
    if (scenario == 1) guiafiltro_cat_alternar(&sFltLive, "Hidden");
    for (int i = 0; i < 4000; i++) {
      char id[32]; snprintf(id, sizeof id, "xtream:%d", i + 1);
      channel(fl, scenario == 1 ? "Hidden" : scenario == 2 ? "UHD" : "Sports", id, "X HD");
    }
    channel(fl, "UHD", "xtream:4001", "Late 4K");
    channel(fl, "UHD", "xtream:4002", "Later 4K");
    channel(fl, "UHD", "xtream:4003", "Late HD");
    fltLive = sFltLive; adotarStaging(); cacheGravar();
    for (int offline = 0; offline < 2; offline++) {
      if (offline) { stagingZerar(); sFltLive = fltLive; sTeto = 5; cacheFonteLer("xt"); adotarStaging(); }
      assert(nCanais == (scenario == 1 ? 3 : 2));
      filtrosAbrir(); fMontarCanais("UHD");
      int found = 0;
      for (int i = 0; i < fN; i++) if (!strcmp(fIt[i].id, "xtream:4001")) found++;
      assert(found == 1);
      int later = 0;
      for (int i = 0; i < fN; i++) if (!strcmp(fIt[i].id, "xtream:4002")) later++;
      assert(later == 1);
      if (scenario != 2) {
        int raw = 0;
        for (int i = 0; i < fN; i++) if (!strcmp(fIt[i].id, "xtream:4003")) raw++;
        assert(raw == 1);
      }
      strcpy(fCatNome, "UHD"); fNivel = 1;
      for (paFoco = 0; paFoco < fN && strcmp(fIt[paFoco].id, "xtream:4001"); paFoco++) {}
      filtrosOk();
      assert(guiafiltro_canal_oculto(&fltLive, "xtream:4001"));
      filtrosOk();
    }
  }
}
static void defaultInventoryRegression(void) {
  int fl = begin(GF_TODAS, 900);
  for (int i = 0; i < 900; i++) {
    char id[32]; snprintf(id, sizeof id, "xtream:sport%d", i);
    channel(fl, "Sports", id, "Sports HD");
  }
  for (int i = 0; i < 65; i++) {
    char id[32]; snprintf(id, sizeof id, "xtream:movie%d", i);
    channel(fl, "Movies", id, "Movie HD");
  }
  adotarStaging(); cacheGravar();
  filtrosAbrir(); fMontarCanais("Movies");
  assert(fN == 65);
  strcpy(fCatNome, "Movies"); fNivel = 1; paFoco = 64;
  assert(!strcmp(fIt[paFoco].id, "xtream:movie64"));
  filtrosOk();
  assert(guiafiltro_canal_oculto(&fltLive, "xtream:movie64"));
  filtrosOk();
  assert(!guiafiltro_canal_oculto(&fltLive, "xtream:movie64"));
  fl = begin(GF_TODAS, 900);
  XtCtx x = {0}; x.fl = fl;
  xtCat("Movies", 0, 4002, &x);
  for (int i = 0; i < 4002; i++) {
    char id[32]; snprintf(id, sizeof id, "xtream:page%d", i);
    XtreamItem it = {0}; it.id = id; it.nome = "Movie HD";
    it.categoria = "Movies"; it.catOrdem = 0;
    xtCanal(&it, &x);
  }
  free(x.giDe);
  adotarStaging(); cacheGravar();
  filtrosAbrir(); fMontarCanais("Movies");
  assert(fN == 4001 && fIt[4000].n == -1);
  strcpy(fCatNome, "Movies"); fNivel = 1; paFoco = 4000;
  filtrosOk();
  assert(fN <= 4000);
  for (paFoco = 0; paFoco < fN && strcmp(fIt[paFoco].id, "xtream:page4001"); paFoco++) {}
  assert(paFoco < fN);
  filtrosOk();
  assert(guiafiltro_canal_oculto(&fltLive, "xtream:page4001"));
  filtrosOk();
  assert(!guiafiltro_canal_oculto(&fltLive, "xtream:page4001"));
  fl = begin(GF_TODAS, 900);
  // Exceed both the old inventory prefix and raw cache byte budget.
  for (int i = 0; i < 30000; i++) {
    char id[32]; snprintf(id, sizeof id, "xtream:%d", i);
    channel(fl, "Sports", id, "Sports HD");
  }
  channel(fl, "Movies", "xtream:movie1", "Movie One HD");
  channel(fl, "Movies", "xtream:movie2", "Movie Two HD");
#ifdef GUIAFILTRO_FINAL_ADMISSION
  assert(sNCanais == 900);
#endif
  adotarStaging(); cacheGravar();
  assert(nCanais == 900 && !canalPorId("xtream:movie1"));
  filtrosAbrir();
  for (paFoco = 3; paFoco < 3 + fN && strcmp(fIt[paFoco - 3].nome, "Movies"); paFoco++) {}
  assert(paFoco < 3 + fN);
  filtrosEvento(SDLK_RIGHT, SDL_GetTicks());
  assert(fNivel && fN == 2);
  assert(!strcmp(fIt[0].id, "xtream:movie1") && !strcmp(fIt[1].id, "xtream:movie2"));
  filtrosOk();
  assert(guiafiltro_canal_oculto(&fltLive, "xtream:movie1"));
}
static void replacementMetadataRegression(void) {
  for (int reverse = 0; reverse < 2; reverse++) {
    int fl = begin(GF_PREFERIR_4K, 1);
    for (int i = 0; i < 2; i++) {
      int uhd = reverse ? !i : i;
      XtreamItem it = {0}; XtCtx x = {0};
      x.fl = fl;
      it.id = uhd ? "xtream:2" : "xtream:1";
      it.nome = uhd ? "X 4K" : "X FHD";
      it.categoria = uhd ? "UHD" : "Sports";
      it.num = uhd ? 202 : 101; it.catOrdem = -1;
      sGrupoInfo(fl, it.categoria, 1);
      xtCanal(&it, &x);
    }
    rebuild();
    assert(sNCanais == 1);
    adotarStaging(); cacheGravar();
    for (int offline = 0; offline < 2; offline++) {
      if (offline) { begin(GF_PREFERIR_4K, 1); cacheFonteLer("xt"); adotarStaging(); }
      char cat[64]; int number = 0;
      estado = G_PRONTO;
      assert(guia_info_canal("xtream:2", &number, NULL, cat, sizeof cat));
      assert(!strcmp(cat, "UHD") && number == 202);
      filtrosAbrir(); fMontarCanais("UHD");
      assert(fN == 1 && !strcmp(fIt[0].id, "xtream:2"));
      GuiaVariante v[3]; assert(guia_variantes("xtream:2", v, 3) == 2);
      guiafiltro_cat_alternar(&fltLive, "Sports"); empacotar();
      assert(nCanais == 1 && !strcmp(canais[0].id, "xtream:2"));
      guiafiltro_cat_alternar(&fltLive, "Sports");
    }
  }
  // Repeated replacements must not exhaust category or channel slots.
  int fl = begin(GF_PREFERIR_4K, GUIACORTE_MAX_CAT);
  for (int i = 0; i < GUIACORTE_MAX_CAT; i++) {
    char cat[32], id[32], name[32];
    snprintf(cat, sizeof cat, "Category %d", i);
    snprintf(id, sizeof id, "xtream:%d", i);
    snprintf(name, sizeof name, "Station %d FHD", i);
    channel(fl, cat, id, name);
  }
  channel(fl, "UHD", "xtream:999", "Station 0 4K");
  rebuild();
  assert(sNCanais == GUIACORTE_MAX_CAT);
  assert(!strcmp(sCanais[0].id, "xtream:999") && !strcmp(sCats[sCanais[0].cat].nome, "UHD"));
}
static void replacementCategoryCapRegression(void) {
  int fl = begin(GF_PREFERIR_4K, GUIACORTE_MAX_CAT + 1);
  channel(fl, "Sports", "xtream:1", "X FHD");
  channel(fl, "Sports", "xtream:2", "Y FHD");
  for (int i = 1; i < GUIACORTE_MAX_CAT; i++) {
    char cat[32], id[32], name[32];
    snprintf(cat, sizeof cat, "Category %d", i);
    snprintf(id, sizeof id, "xtream:%d", i + 2);
    snprintf(name, sizeof name, "Station %d FHD", i);
    channel(fl, cat, id, name);
  }
  channel(fl, "UHD", "xtream:999", "X 4K");
#ifndef GUIAFILTRO_FINAL_ADMISSION
  assert(!strcmp(sCanais[0].id, "xtream:999"));
  assert(sNCanais == GUIACORTE_MAX_CAT + 1);
#endif
  // A later lower-quality version must remain a fallback, never replace 4K.
  channel(fl, "UHD", "xtream:1000", "X HD");
  rebuild();
  assert(!strcmp(sCanais[0].id, "xtream:999"));
  assert(!strcmp(sCats[sCanais[0].cat].nome, "UHD"));
  assert(sCanais[0].cat != sCanais[1].cat);
  assert(!strcmp(sCats[sCanais[1].cat].nome, "Sports"));
  adotarStaging();
  GuiaVariante v[4];
  assert(guia_variantes("xtream:999", v, 4) == 3);
  assert(!strcmp(v[0].nome, "X 4K"));
  assert(!strcmp(v[1].nome, "X FHD"));
  assert(!strcmp(v[2].nome, "X HD"));
}
static void collapsedCategoryCapRegression(void) {
  for (int mode = GF_SO_4K; mode <= GF_PREFERIR_4K; mode++) {
    int fl = begin(mode, GUIACORTE_MAX_CANAL);
    channel(fl, "UHD", "xtream:anchor", "Anchor 4K");
    for (int i = 1; i < GUIACORTE_MAX_CAT; i++) {
      char cat[32], id[32], name[40];
      snprintf(cat, sizeof cat, "Category %d", i);
      snprintf(id, sizeof id, "xtream:low%d", i);
      snprintf(name, sizeof name, "Station %d FHD", i);
      channel(fl, cat, id, name);
    }
    for (int i = 1; i < GUIACORTE_MAX_CAT; i++) {
      char id[32], name[40];
      snprintf(id, sizeof id, "xtream:high%d", i);
      snprintf(name, sizeof name, "Station %d 4K", i);
      channel(fl, "UHD", id, name);
    }
    // Every surviving station is now in an existing UHD row.
    channel(fl, "News", "xtream:news", "News 4K");
    assert(sCanalPorId(fl, "xtream:news") >= 0);
    adotarStaging();
    assert(nCanais == GUIACORTE_MAX_CAT + 1 && nCats == 2);
    assert(!strcmp(cats[canalPorId("xtream:news")->cat].nome, "News"));
  }
}
static void categoryChannelCapRegression(void) {
  for (int mode = GF_TODAS; mode <= GF_PREFERIR_4K; mode++) {
    if (mode == GF_SO_4K) continue;
    int fl = begin(mode, GUIACORTE_MAX_CANAL);
    for (int i = 0; i < GUIACORTE_MAX_CAT; i++) {
      char cat[32], id[32], name[40];
      snprintf(cat, sizeof cat, "Category %d", i);
      snprintf(id, sizeof id, "xtream:cat%d", i);
      snprintf(name, sizeof name, "Station %d FHD", i);
      channel(fl, cat, id, name);
    }
    for (int i = GUIACORTE_MAX_CAT; i < GUIACORTE_MAX_CANAL; i++) {
      char id[32], name[40];
      snprintf(id, sizeof id, "xtream:overflow%d", i);
      snprintf(name, sizeof name, "Overflow %d FHD", i);
      channel(fl, "Overflow", id, name);
    }
    channel(fl, "Category 0", "xtream:late", "Late FHD");
    adotarStaging();
    assert(nCanais == GUIACORTE_MAX_CAT + 1);
    assert(nCats == GUIACORTE_MAX_CAT && canalPorId("xtream:late"));
  }
  // A late winner empties the first row and frees a category before admission.
  int fl = begin(GF_PREFERIR_4K, GUIACORTE_MAX_CAT);
  for (int i = 0; i < GUIACORTE_MAX_CAT; i++) {
    char cat[32], id[32], name[40];
    snprintf(cat, sizeof cat, "Category %d", i);
    snprintf(id, sizeof id, "xtream:cat%d", i);
    snprintf(name, sizeof name, "Station %d FHD", i);
    channel(fl, cat, id, name);
  }
  channel(fl, "Overflow", "xtream:better", "Station 0 4K");
  channel(fl, "Category 1", "xtream:extra", "Extra FHD");
  adotarStaging();
  assert(nCats == GUIACORTE_MAX_CAT && nCanais == GUIACORTE_MAX_CAT);
  assert(canalPorId("xtream:better") && !canalPorId("xtream:cat0"));
  assert(!canalPorId("xtream:extra"));
}
static void rawCandidateCapRegression(void) {
  int fl = begin(GF_PREFERIR_4K, 900);
  for (int i = 0; i < 30000; i++) {
    char id[32], name[40];
    snprintf(id, sizeof id, "xtream:%d", i);
    snprintf(name, sizeof name, "Station %d HD", i % 100);
    channel(fl, "Sports", id, name);
  }
  channel(fl, "News", "xtream:late", "Late HD");
  channel(fl, "UHD", "xtream:better", "Station 0 4K");
  adotarStaging();
  assert(nCanais == 101 && canalPorId("xtream:late"));
  assert(canalPorId("xtream:better") && !canalPorId("xtream:0"));
  GuiaVariante variants[3];
  assert(guia_variantes("xtream:better", variants, 3) == 3);
  assert(!strcmp(variants[0].nome, "Station 0 4K"));
  // Even a full unique-channel budget must accept an existing channel's upgrade.
  fl = begin(GF_PREFERIR_4K, 900);
  for (int i = 0; i < 30000; i++) {
    char id[32], name[40];
    snprintf(id, sizeof id, "xtream:%d", i);
    snprintf(name, sizeof name, "Station %d HD", i);
    channel(fl, "Sports", id, name);
  }
  channel(fl, "UHD", "xtream:better", "Station 0 4K");
  adotarStaging();
  assert(nCanais == 900 && canalPorId("xtream:better"));
}
#ifdef G_INVENTARIO_BLOCO
// Catch admission-time growth, including refresh storage, before publication frees it.
static void admissionStorageRegression(void) {
  for (int mode = GF_TODAS; mode < GF_MODOS; mode++) {
    int fl = begin(mode, 900);
    guiafiltro_cat_alternar(&sFltLive, "Unused");
    char description[1025];
    memset(description, 'x', sizeof description - 1); description[1024] = 0;
    size_t peak = 0;
    for (int i = 0; i < 30000; i++) {
      char id[32], name[40];
      snprintf(id, sizeof id, "xtream:%d", i);
      snprintf(name, sizeof name, "Station n%d %s", i, mode == GF_SO_4K ? "4K" : "FHD");
      GCru c = {.id = id, .nome = name, .grupo = "Sports", .desc = description};
      sGrupoInfo(fl, c.grupo, 1); sCanalAdd(fl, &c);
      size_t bytes = ltv_arena_bytes(&sArena) + (size_t)sCapCanais * sizeof *sCanais +
        (size_t)sCapCats * sizeof *sCats + (size_t)sNInventario * sizeof *sInventario +
        (size_t)sNAlts * sizeof *sAlts + (size_t)sDup.cap * sizeof *sDup.t +
        sCacheBytes + inventorySpoolBytes(sInventarioDisco) +
        ltv_arena_bytes(&arena) + (size_t)capCanais * sizeof *canais +
        (size_t)nInventario * sizeof *inventario + inventorySpoolBytes(inventarioDisco);
      if (bytes > peak) peak = bytes;
    }
    printf("admission mode=%d peak=%zu bytes (budget 16 MiB)\n", mode, peak); fflush(stdout);
    assert(peak <= 16 * 1024 * 1024);
    channel(fl, "UHD", "xtream:better", "Station n0 4K");
    adotarStaging();
    assert(nCanais == 900 && canalPorId("xtream:899"));
    assert(strlen(canalPorId("xtream:899")->desc) == 1024);
    if (mode == GF_PREFERIR_4K) assert(canalPorId("xtream:better"));
  }
  // Repeated upgrades must reclaim dead arena strings without losing duplicate indexes.
  int fl = begin(GF_PREFERIR_4K, 900);
  char description[1025];
  memset(description, 'x', 1024); description[1024] = 0;
  const char *qualities[] = {"SD", "HD", "FHD", "4K"};
  for (int quality = 0; quality < 4; quality++)
    for (int i = 0; i < 900; i++) {
      char id[32], name[40];
      snprintf(id, sizeof id, "xtream:%d-%d", quality, i);
      snprintf(name, sizeof name, "Station n%d %s", i, qualities[quality]);
      GCru c = {.id = id, .nome = name, .grupo = "Sports", .desc = description};
      sGrupoInfo(fl, c.grupo, 1); sCanalAdd(fl, &c);
      assert(ltv_arena_bytes(&sArena) < 3 * 1024 * 1024);
    }
  channel(fl, "Overflow", "xtream:extra", "Extra HD");
  adotarStaging();
  assert(nCanais == 900 && canalPorId("xtream:3-899"));
  GuiaVariante variants[4];
  assert(guia_variantes("xtream:3-899", variants, 4) == 4);
}
static void sourceRemovalInventoryRegression(void) {
  nAddons = 2; bases[0] = "https://a.invalid"; bases[1] = "https://b.invalid";
  begin(GF_TODAS, 900);
  int a = sFonte(LTV_ADDON, bases[0], "A", bases[0]);
  int b = sFonte(LTV_ADDON, bases[1], "B", bases[1]);
  for (int i = 0; i < 80; i++) {
    char id[32], name[40];
    snprintf(id, sizeof id, "tv:%d", i);
    snprintf(name, sizeof name, "A station %d", i);
    channel(a, "Sports", id, name);
  }
  channel(b, "Sports", "tv:79", "B station");
  adotarStaging(); cacheGravar();
  char removed[LTV_CHAVE], remaining[LTV_CHAVE];
  ltv_chave_fonte(LTV_ADDON, bases[0], removed, sizeof removed);
  ltv_chave_fonte(LTV_ADDON, bases[1], remaining, sizeof remaining);
  fioVivo = 1; // Settings/sync removal while refresh is still running.
  guia_fonte_esquecer(removed);
  filtrosAbrir(); fMontarCanais("Sports");
  for (int i = 0; i < fN; i++) {
    assert(strcmp(fIt[i].fonte, removed));
    assert(strcmp(fIt[i].fonte, remaining) || strncmp(fIt[i].nome, "A station", 9));
  }
  assert(fN == 1 && !strcmp(fIt[0].nome, "B station") && !fIt[0].oculto);
  assert(!guiafiltro_canal_oculto_fonte(&fltLive, remaining, "tv:79"));
  assert(nCanais == 1 && !strcmp(canais[0].nome, "B station"));
  fioVivo = cargaCancelada = recarregarPend = 0;
}
#endif
static void publicationStorageRegression(void) {
  for (int pass = 0; pass < 2; pass++) {
    int fl = begin(GF_TODAS, 900);
    guiafiltro_cat_alternar(&sFltLive, "Unused");
    for (int i = 0; i < 30000; i++) {
      char id[32], name[40], description[1024];
      snprintf(id, sizeof id, "xtream:%d", i);
      snprintf(name, sizeof name, "Station %d HD", i);
      memset(description, 'x', sizeof description - 1);
      description[sizeof description - 1] = 0;
      GCru c = {0};
      c.id = id; c.nome = name; c.grupo = "Sports";
      c.desc = i < 900 ? "Kept description" : description;
      sGrupoInfo(fl, c.grupo, 1); sCanalAdd(fl, &c);
    }
    adotarStaging();
    assert(nCanais == 900 && capCanais == 900);
    assert(ltv_arena_bytes(&arena) <= 2 * LTV_BLOCO);
    assert(!sCanais && !sArena.prim);
    assert(!strcmp(canais[899].id, "xtream:899"));
    assert(!strcmp(canais[899].desc, "Kept description"));
    assert(!strcmp(cats[canais[899].cat].nome, "Sports"));
    assert(ltvF[0].ngi == 1 && ltvF[0].gi[0].n == 30000);
  }
}
static void rawBudgetRegression(void) {
#ifdef GUIAFILTRO_RAW_TEST
  int fl = begin(GF_SO_4K, 1);
  for (int i = 0; i < 100000; i++) channel(fl, "Sports", "xtream:1", "X FHD");
  assert(sCache[fl] && ftell(sCache[fl]) <= 2 * 1024 * 1024);
  adotarStaging(); cacheGravar();
  assert(sCache[fl] == NULL);
#endif
}
static void rawOfflineRegression(void) {
#ifdef GUIAFILTRO_RESET_TEST
  char description[4096];
  memset(description, 'x', sizeof description - 1); description[sizeof description - 1] = 0;
  for (int mode = GF_TODAS; mode < GF_MODOS; mode++) {
    int fl = begin(mode, 5);
    if (mode == GF_TODAS) guiafiltro_cat_alternar(&sFltLive, "Sports");
    GCru c = {0};
    c.id = "xtream:1"; c.nome = "X HD"; c.grupo = "Sports"; c.desc = description;
    sGrupoInfo(fl, c.grupo, 1);
    for (int i = 0; i < 600; i++) sCanalAdd(fl, &c);
    c.id = "xtream:2"; c.nome = "X 4K"; c.grupo = "UHD";
    sGrupoInfo(fl, c.grupo, 1); sCanalAdd(fl, &c);
    int other = sFonte(LTV_M3U, "https://fixture.invalid/overflow.m3u", "Other", "");
    char key[LTV_CHAVE]; snprintf(key, sizeof key, "%s", sLtvF[other].chave);
    c.id = "m3u:late"; c.nome = "Y 4K";
    sGrupoInfo(other, c.grupo, 1); sCanalAdd(other, &c);
    assert(sCache[fl] && sCacheBytes <= 2 * 1024 * 1024);
    fltLive = sFltLive; adotarStaging(); cacheGravar();
    assert(nCanais == 2);
    // Restart offline with the same rules, including a source after the shared budget fills.
    stagingZerar(); sTeto = 5; sFltLive = fltLive;
    assert(cacheFonteLer("xt") == 1);
    assert(cacheFonteLer(key) == 1);
    adotarStaging();
    assert(nCanais == 2 && !strcmp(canais[0].id, "xtream:2"));
    assert(!strcmp(canais[0].nome, "X 4K") && !strcmp(cats[canais[0].cat].nome, "UHD"));
    assert(!strcmp(canais[1].id, "m3u:late"));
  }
#endif
}
static void sessionInventoryRegression(void) {
#ifdef GUIAFILTRO_RESET_TEST
  perfilAtivo = fltPerfil = 2;
  memset(&fltLive, 0, sizeof fltLive);
  guiafiltro_cat_alternar(&fltLive, "Sports"); fltGravar();
  perfilAtivo = fltPerfil = 1;
  int fl = begin(GF_TODAS, 5);
  channel(fl, "Sports", "xtream:private", "Previous profile HD");
  adotarStaging();
  assert(nInventario == 1);
  fl = begin(GF_TODAS, 5);
  channel(fl, "Sports", "xtream:pending", "Pending HD");
  GInventario *pending = sInventario;
  perfilAtivo = 2;
  guia_abrir_sessao();
  assert(sInventario == pending && sNInventario == 1);
  filtrosAbrir();
  assert(guiafiltro_cat_oculta(&fltLive, "Sports"));
  fMontarCanais("Sports");
  assert(fN == 0);
  assert(inventario == NULL && nInventario == 0);
#endif
}
static void panelPublicationRegression(void) {
  filtrosAbrir();
  assert(fN == 0);
  paFoco = GF_PREFERIR_4K;
  int fl = begin(GF_TODAS, 5);
  channel(fl, "Sports", "xtream:1", "X HD");
  channel(fl, "News", "xtream:2", "Y HD");
  rebuild(); atomic_store(&pendPronto, 1); drenarCarga();
  assert(fN == 2);
  assert(!fNivel && paFoco == GF_PREFERIR_4K);

  paFoco = 4; paRol = 20.0f;
  fl = begin(GF_TODAS, 5);
  channel(fl, "News", "xtream:2", "Y HD");
  channel(fl, "Movies", "xtream:3", "Z HD");
  rebuild(); atomic_store(&pendPronto, 1); drenarCarga();
  assert(fN == 2 && !strcmp(fIt[0].nome, "News") && !strcmp(fIt[1].nome, "Movies"));
  assert(!fNivel && paFoco == 3 && paRol == 20.0f);

  filtrosEvento(SDLK_RIGHT, 0);
  assert(fNivel && !strcmp(fCatNome, "News") && fN == 1);
  fl = begin(GF_TODAS, 5);
  channel(fl, "News", "xtream:4", "New HD");
  channel(fl, "News", "xtream:2", "Y FHD");
  rebuild(); atomic_store(&pendPronto, 1); drenarCarga();
  assert(fNivel && !strcmp(fCatNome, "News") && fN == 2);
  assert(paFoco == 1 && !strcmp(fIt[paFoco].id, "xtream:2"));
  assert(!strcmp(fIt[paFoco].nome, "Y FHD"));

  fl = begin(GF_TODAS, 5);
  channel(fl, "Movies", "xtream:3", "Z HD");
  rebuild(); atomic_store(&pendPronto, 1); drenarCarga();
  assert(fNivel && !strcmp(fCatNome, "News") && fN == 0 && paFoco == 0);
}
#ifdef GUIAFILTRO_STALKER_TEST
#define main filter_test_main
#endif
int main(int argc, char **argv) {
  int fl;
  assert(argc == 2); dados_iniciar(argv[1]);
  fltPerfil = 1;
  const char *regression = getenv("GUIAFILTRO_CASE");
  if (regression) {
    if (!strncmp(regression, "pagination", 10))
      paginationRegression(!strcmp(regression, "pagination-4k") ? GF_SO_4K :
                           !strcmp(regression, "pagination-prefer") ? GF_PREFERIR_4K : GF_TODAS);
    else if (!strcmp(regression, "panel-publication")) panelPublicationRegression();
    else if (!strcmp(regression, "cap")) capRegression();
    else if (!strcmp(regression, "cache-refresh")) cacheRefreshRegression();
    else if (!strncmp(regression, "empty-", 6))
      filteredEmptyRegression(strstr(regression, "m3u") ? LTV_M3U :
                              strstr(regression, "addon") ? LTV_ADDON : LTV_XTREAM,
                              strstr(regression, "hidden") != NULL, strstr(regression, "retry") != NULL);
    else if (!strcmp(regression, "visibility")) visibilityRegression();
    else if (!strcmp(regression, "variants")) variantsRegression();
    else if (!strcmp(regression, "sibling-visibility")) siblingVisibilityRegression();
    else if (!strcmp(regression, "source-visibility")) sourceVisibilityRegression();
    else if (!strcmp(regression, "inventory-bound")) inventoryBoundRegression();
#if defined(G_INVENTARIO_BLOCO) || defined(GUIAFILTRO_SPOOL_TEST)
    else if (!strcmp(regression, "inventory-spool")) inventorySpoolRegression();
#endif
    else if (!strcmp(regression, "categories")) categoriesRegression();
    else if (!strcmp(regression, "inventory")) inventoryRegression();
    else if (!strcmp(regression, "late-inventory")) lateInventoryRegression();
    else if (!strcmp(regression, "default-inventory")) defaultInventoryRegression();
    else if (!strcmp(regression, "replacement-metadata")) replacementMetadataRegression();
    else if (!strcmp(regression, "replacement-category-cap")) replacementCategoryCapRegression();
    else if (!strcmp(regression, "collapsed-category-cap")) collapsedCategoryCapRegression();
    else if (!strcmp(regression, "category-channel-cap")) categoryChannelCapRegression();
    else if (!strcmp(regression, "raw-candidate-cap")) rawCandidateCapRegression();
    else if (!strcmp(regression, "publication-storage")) publicationStorageRegression();
#ifdef G_INVENTARIO_BLOCO
    else if (!strcmp(regression, "admission-storage")) admissionStorageRegression();
    else if (!strcmp(regression, "source-removal-inventory")) sourceRemovalInventoryRegression();
#endif
    else if (!strcmp(regression, "raw-budget")) rawBudgetRegression();
    else if (!strcmp(regression, "raw-offline")) rawOfflineRegression();
    else if (!strcmp(regression, "session-inventory")) sessionInventoryRegression();
    else assert(0);
    printf("PASS: %s regression\n", regression);
    return 0;
  }
  for (int modo = 0; modo < GF_MODOS; modo++) {
    fl = begin(modo, 5);
    channel(fl, "Sports", "xtream:1", "X 4K");
    channel(fl, "Sports", "xtream:2", "X FHD");
    channel(fl, "Sports", "xtream:3", "X HD");
    channel(fl, "Sports", "xtream:4", "Y FHD");
    channel(fl, "Sports", "xtream:5", "Y SD");
    rebuild();
    assert(sNCanais == (modo == GF_TODAS ? 5 : modo == GF_SO_4K ? 1 : 2));
    if (modo) assert(!strcmp(sCanais[0].nome, "X 4K"));
    if (modo == GF_PREFERIR_4K) assert(!strcmp(sCanais[1].nome, "Y FHD"));
    adotarStaging();
    if (modo == GF_PREFERIR_4K) {
      GuiaVariante v[5];
      assert(guia_variantes("xtream:1", v, 5) == 3);
      assert(!strcmp(v[0].nome, "X 4K") && !strcmp(v[1].nome, "X FHD") && !strcmp(v[2].nome, "X HD"));
    }
  }
  // Hidden categories consume neither category nor channel slots.
  fl = begin(GF_TODAS, 2);
  guiafiltro_cat_alternar(&sFltLive, "Hidden");
  for (int i = 0; i < GUIACORTE_MAX_CAT + 1; i++) channel(fl, "Hidden", "hidden", "Hidden HD");
  channel(fl, "Visible", "1", "Visible HD");
  channel(fl, "Visible", "2", "Visible FHD");
  rebuild();
  assert(sNCanais == 2 && sNCats == 1);
  // A later best variant replaces its slot even when the cap is full.
  fl = begin(GF_PREFERIR_4K, 1);
  channel(fl, "Sports", "1", "X SD"); channel(fl, "Sports", "2", "X UHD HEVC");
  rebuild();
  assert(sNCanais == 1 && !strcmp(sCanais[0].nome, "X UHD HEVC"));
  // Xtream must not reject a large raw group before quality filtering.
  { XtCtx x = {0}; XtreamItem c = {0};
    fl = begin(GF_SO_4K, 1); x.fl = fl;
    xtCat("Sports", 0, 10000, &x);
    c.id = "1"; c.nome = "X SD"; c.categoria = "Sports"; c.catOrdem = 0;
    xtCanal(&c, &x); c.id = "2"; c.nome = "X 4K"; xtCanal(&c, &x);
    rebuild();
    assert(sNCanais == 1 && !strcmp(sCanais[0].nome, "X 4K")); free(x.giDe);
  }
  // M3U may improve an admitted channel after reaching the cap.
  { M3uCtx x = {0}; LtvM3uCanal c = {0};
    fl = begin(GF_PREFERIR_4K, 1); x.fl = fl;
    c.grupo = "Sports"; c.nome = "X SD"; c.tvgId = "x"; c.url = "https://fixture.invalid/sd";
    c.logo = c.tvgNome = c.ua = c.referer = "";
    m3uCanal(&c, &x); c.nome = "X 4K"; m3uCanal(&c, &x);
    rebuild();
    assert(sNCanais == 1 && !strcmp(sCanais[0].nome, "X 4K"));
  }
  // Keep the unfiltered cache and reconstruct fallbacks on offline reload.
  fl = begin(GF_TODAS, 5);
  channel(fl, "Sports", "xtream:1", "X HD"); channel(fl, "Sports", "xtream:2", "X 4K");
  adotarStaging(); cacheGravar();
  begin(GF_PREFERIR_4K, 5); cacheFonteLer("xt"); rebuild(); assert(sNCanais == 1); adotarStaging();
  { GuiaVariante v[3]; assert(guia_variantes("xtream:2", v, 3) == 2); }
  cacheGravar();
  begin(GF_TODAS, 5); assert(cacheFonteLer("xt") == 2);
  // Profile persistence round-trip through the real data directory.
  memset(&fltLive, 0, sizeof fltLive); fltLive.modo = GF_PREFERIR_4K;
  guiafiltro_cat_alternar(&fltLive, "Hidden"); fltGravar();
  fltPerfil = -1; fltGarantir();
  assert(fltLive.modo == GF_PREFERIR_4K && guiafiltro_cat_oculta(&fltLive, "Hidden"));
  // Switching profile reloads its own filter instead of leaking the old one.
  perfilAtivo = 2; fltPerfil = -1; fltGarantir();
  assert(fltLive.modo == GF_TODAS && !guiafiltro_cat_oculta(&fltLive, "Hidden"));
  perfilAtivo = 1; fltPerfil = -1; fltGarantir();
  assert(fltLive.modo == GF_PREFERIR_4K && guiafiltro_cat_oculta(&fltLive, "Hidden"));
  // Filtering everything must publish an empty grid, not retain old channels.
  adotarStaging();
  fl = begin(GF_SO_4K, 1); channel(fl, "Sports", "1", "X SD");
  publicar(); assert(nCanais == 0);
  // Real platform caps remain unchanged.
  fl = begin(GF_TODAS, GUIACORTE_MAX_CANAL + 5);
  for (int i = 0; i < GUIACORTE_MAX_CANAL + 5; i++) channel(fl, "Sports", "1", "X");
  rebuild();
  assert(sNCanais == GUIACORTE_MAX_CANAL);
  fl = begin(GF_TODAS, GUIACORTE_MAX_CANAL);
  for (int i = 0; i < GUIACORTE_MAX_CAT + 5; i++) {
    char cat[32]; snprintf(cat, sizeof cat, "Category %d", i); channel(fl, cat, "1", "X");
  }
  adotarStaging();
  assert(nCats == GUIACORTE_MAX_CAT && nCanais == GUIACORTE_MAX_CAT);
  puts("PASS: real Live TV loader, source callbacks, empty results and caps");
  return 0;
}
