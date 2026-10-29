// Real Guide and Stalker ingestion; only portal HTTP is replaced.
#define GUIAFILTRO_STALKER_TEST
#include "guiafiltro_loader.c"
#undef main
#ifndef STALKER_TEST_SOURCE
#define STALKER_TEST_SOURCE "../src/stalker.c"
#endif
#include STALKER_TEST_SOURCE

static int scenario, pages, linkRequests;
char *rede_baixar_com(const char *url, int seconds, const char *const *headers) {
  (void)seconds; (void)headers;
  if (strstr(url, "action=handshake")) return strdup("{\"js\":1,\"token\":\"fixture\"}");
  if (strstr(url, "action=get_genres")) {
    if (scenario == 7) {
      char genres[5000]; size_t used = (size_t)snprintf(genres, sizeof genres, "{\"js\":[");
      for (int i = 0; i < 65; i++)
        used += (size_t)snprintf(genres + used, sizeof genres - used,
          "%s{\"id\":\"%d\",\"title\":\"%s\"}", i ? "," : "", i, i == 64 ? "Hidden" : "Sports");
      snprintf(genres + used, sizeof genres - used, "]}"); return strdup(genres);
    }
    return strdup("{\"js\":[{\"id\":\"1\",\"title\":\"Hidden\"},{\"id\":\"2\",\"title\":\"Sports\"}]}");
  }
  if (strstr(url, "action=create_link")) {
    linkRequests++;
    return strdup("{\"js\":1,\"cmd\":\"ffmpeg https://fixture.invalid/play\"}");
  }
  if (!strstr(url, "action=get_ordered_list")) return strdup("{\"js\":{}}");
  const char *p = strstr(url, "&p=");
  assert(p);
  int page = atoi(p + 3), count = scenario == 6 ? 20 : 100;
  int total = scenario == 5 ? 2401 : scenario == 6 ? 1301 : 1300;
  char body[32000];
  size_t used = (size_t)snprintf(body, sizeof body, "{\"js\":{\"data\":[");
  pages++;
  for (int i = (page - 1) * count; i < page * count && i < total; i++) {
    char name[64];
    int hidden = (scenario == 0 || scenario == 6 || scenario == 7) && i < 1200;
    if (scenario == 3 && i < 688) hidden = 1;
    if (scenario == 4 || scenario == 5)
      snprintf(name, sizeof name, "Station%d %s", i == total - 1 ? 0 : i, i == total - 1 ? "4K" : "HD");
    else snprintf(name, sizeof name, "Station%d %s", i, scenario == 1 && i < 1200 ? "HD" : "4K");
    used += (size_t)snprintf(body + used, sizeof body - used,
      "%s{\"id\":\"%d\",\"name\":\"%s\",\"tv_genre_id\":\"%d\",\"cmd\":\"ffmpeg http://fixture.invalid/%d\"}",
      i == (page - 1) * count ? "" : ",", i, name, scenario == 7 && hidden ? 64 : hidden ? 1 : 2, i);
  }
  snprintf(body + used, sizeof body - used, "]}}");
  return strdup(body);
}

static void regression(int which) {
  scenario = which; pages = linkRequests = 0;
  stalker_definir_portal("https://fixture.invalid");
  stalker_definir_mac("00:1a:79:00:00:01");
  begin(which >= 4 && which <= 5 ? GF_PREFERIR_4K : which == 1 ? GF_SO_4K : GF_TODAS, 900);
  if (which == 0 || which == 3 || which == 6 || which == 7) guiafiltro_cat_alternar(&sFltLive, "Hidden");
  if (which == 3)
    for (int i = 688; i < 1200; i++) {
      char id[32]; snprintf(id, sizeof id, "stalker:%d", i);
      guiafiltro_canal_alternar_fonte(&sFltLive, "st", id, "", "Sports");
    }
  if (which == 2) {
    const char *selected[] = {"Sports"};
    ltv_pref_definir_grupos(ltv_pref("st", 1), selected, 1);
    scenario = 0;
  }
#ifdef NV_IPTVFONTES_H
  IptvConta conta = {.tipo=IPTV_STALKER,.ativa=1,.nome="Stalker"};
  stalkerCarregar(&conta);
#else
  stalkerCarregar();
#endif
  selecionarSecoes();
  if (which <= 3 || which == 7) assert(sNCanais == 100 && !strcmp(sCanais[0].id, "stalker:1200"));
  else if (which <= 5) assert(sNCanais == 900 && !strcmp(sCanais[0].id, which == 4 ? "stalker:1299" : "stalker:2400"));
  else assert(sNCanais == 101 && pages >= 66);
  char url[256];
  assert(stalker_resolver_sessao(sCanais[0].id, url, sizeof url, 0));
  assert(!strcmp(url, "https://fixture.invalid/play") && linkRequests == 1);
  assert(nTabela <= 1200);
  // A subsequent load must not strand commands behind the preceding load's cache.
  if (which == 5) {
    scenario = 1;
    begin(GF_SO_4K, 900);
#ifdef NV_IPTVFONTES_H
    stalkerCarregar(&conta);
#else
    stalkerCarregar();
#endif
    selecionarSecoes();
    assert(sNCanais == 100);
    assert(stalker_resolver_sessao(sCanais[0].id, url, sizeof url, 0));
    assert(nTabela <= 1200);
  }
  printf("PASS: Stalker case %d, playable late channels, bounded commands\n", which);
}
int main(int argc, char **argv) {
  assert(argc == 3); dados_iniciar(argv[1]); fltPerfil = 1;
  if (!strcmp(argv[2], "addon")) addonLateUpgradeRegression();
  else regression(atoi(argv[2]));
  return 0;
}
