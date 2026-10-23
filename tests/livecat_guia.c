// Exercise real guide staging and regrouping with a mocked Worker.
#define NV_PERFIL_SESSAO_FORTE
#include "../src/guia.c"
#include <assert.h>

static unsigned long long coverageSteps;
__attribute__((no_sanitize("coverage")))
void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
  (void)guard;
  coverageSteps++;
}
__attribute__((no_sanitize("coverage")))
void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
  for (; start < stop; start++) *start = 1;
}

static int enabled = 1, posts;
static char *addonPage;
static int addonPaginated, addonRequests;
char *rede_baixar_medido_controle(const char *url, int seconds,
                                 const char *const *headers,
                                 const RedeControle *control, RedeMedida *measure) {
  (void)seconds; (void)headers; (void)control; (void)measure;
  addonRequests++;
  if (addonPaginated) {
    const char *page = strstr(url, "/skip=");
    int skip = page ? atoi(page + 6) : 0;
    size_t used = (size_t)snprintf(addonPage, 16384, "{\"metas\":[");
    for (int i = skip; i < skip + 100 && i <= 1200; i++)
      used += (size_t)snprintf(addonPage + used, 16384 - used,
        "%s{\"id\":\"page-%d\",\"name\":\"page pending %04d\"}", i > skip ? "," : "", i, i);
    snprintf(addonPage + used, 16384 - used, "]}");
  }
  return strdup(addonPage);
}
int ajustes_livetv_sugerir(void) { return enabled; }
int xtream_e_id(const char *id) { return id && !strncmp(id, "xtream:", 7); }
int stalker_canais_sessao(StalkerCanal *out, int max, uint64_t session) {
  const char *names[] = {"Aurora HD", "Lumen"};
  int n = max < 2 ? max : 2;
  assert(session == 0);
  for (int i = 0; i < n; i++) {
    memset(&out[i], 0, sizeof out[i]);
    snprintf(out[i].id, sizeof out[i].id, "stalker:%d", i + 1);
    snprintf(out[i].nome, sizeof out[i].nome, "%s", names[i]);
  }
  return n;
}
int stalker_canais_cb_sessao(StalkerCb cb, void *u, uint64_t session) {
  StalkerCanal out[2];
  int n = stalker_canais_sessao(out, 2, session);
  for (int i = 0; i < n; i++) if (!cb(&out[i], u)) break;
  return n;
}
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return g == 0; }
int perfis_sessao_travar(uint64_t g) { return g == 0; }
void perfis_sessao_destravar(void) {}
const char *i18n(const char *s) { return s; }
int perfis_ativo(void) { return 1; }
char *rede_postar_st(const char *u, int s, const char *const *h, const char *b, int *st) {
  (void)u; (void)s; (void)h; (void)b; (void)st; abort();
}
static char *worker(const char *url, const char *body, int *status) {
  assert(!strcmp(url, "http://rec.test/livetv/categorias"));
  assert(strstr(body, "aurora") && strstr(body, "lumen"));
  assert(!strstr(body, "espn") && !strstr(body, "classified"));
  posts++; *status = 200;
  return strdup("{\"cats\":{\"aurora\":\"Esportes\",\"lumen\":\"Infantil\"}}");
}
static char *coldWorker(const char *url, const char *body, int *status) {
  assert(strstr(body, "velora") && strstr(body, "nimbo") && strstr(body, "talume"));
  (void)url; posts++; *status = 200;
  return strdup("{\"cats\":{\"velora\":\"Esportes\",\"nimbo\":\"Infantil\",\"talume\":\"Infantil\"}}");
}
static char *overflowWorker(const char *url, const char *body, int *status) {
  (void)url;
  char *reply = malloc(16384);
  size_t used = (size_t)snprintf(reply, 16384, "{\"cats\":{");
  const char *p = strchr(body, '[');
  int count = 0;
  while (p && (p = strchr(p, '"'))) {
    const char *end = strchr(++p, '"');
    assert(end && end - p < 64);
    used += (size_t)snprintf(reply + used, 16384 - used, "%s\"%.*s\":%s",
                            count++ ? "," : "", (int)(end - p), p,
                            !strncmp(p, "latechannel", 11) ? "\"Infantil\"" : "null");
    p = end + 1;
  }
  assert(count <= 200);
  snprintf(reply + used, 16384 - used, "}}");
  posts++; *status = 200;
  return reply;
}
static void addM3u(M3uCtx *ctx, const char *name, const char *group, const char *id) {
  LtvM3uCanal c = {0}; c.nome = name; c.grupo = group; c.url = id;
  c.tvgId = c.tvgNome = c.logo = c.ua = c.referer = "";
  assert(m3uCanal(&c, ctx));
}
static void checkGroups(void) {
  GFonteLtv *f = &sLtvF[0];
  int sports = 0, kids = 0, other = 0;
  for (int i = 0; i < f->ngi; i++) {
    if (!strcmp(f->gi[i].nome, "Esportes")) sports = f->gi[i].n;
    if (!strcmp(f->gi[i].nome, "Infantil")) kids = f->gi[i].n;
    if (!strcmp(f->gi[i].nome, "Outros")) other = f->gi[i].n;
  }
  assert(sports == 2 && kids == 1 && other == 0);
}
static int xtreamLocalSections(void) {
  int failures = 0;
  const char *selections[] = {NULL, "Esportes", "Outros"};
  for (int mode = 0; mode < 3; mode++) {
    for (enabled = 0; enabled <= 1; enabled++) {
      stagingZerar(); sTeto = 100;
      XtCtx xt = {0};
      xt.fl = sFonte(LTV_XTREAM, "", "Xtream", "");
      ltv_pref_definir_grupos(ltv_pref("xt", 1), &selections[mode], mode != 0);
      xt.nsel = sGruposEscolhidos(xt.fl, xt.sel, 256);
      xtCat("Outros", 0, 2, &xt);
      XtreamItem it = {0};
      it.catOrdem = 0; it.categoria = "Outros"; it.logo = it.epgId = "";
      it.id = "xtream:espn"; it.nome = "ESPN HD"; it.semCategoria = 1;
      xtCanal(&it, &xt);
      it.id = "xtream:provider"; it.nome = "Provider channel"; it.semCategoria = 0;
      xtCanal(&it, &xt);
      int sports = 0, other = 0;
      for (int i = 0; i < sLtvF[xt.fl].ngi; i++) {
        GGrupoInfo *g = &sLtvF[xt.fl].gi[i];
        if (!strcmp(g->nome, "Esportes")) sports += g->n;
        if (!strcmp(g->nome, "Outros")) other += g->n;
      }
      if (sports != 1 || other != 1 || sLtvF[xt.fl].total != 2 ||
          sNCanais != (mode ? 1 : 2) ||
          (mode == 1 && strcmp(sCanais[0].nome, "ESPN HD")) ||
          (mode == 2 && strcmp(sCanais[0].nome, "Provider channel"))) failures++;
      if (mode == 0) {
        canais = sCanais; cats = sCats; nCanais = sNCanais;
        ltvF[0] = sLtvF[xt.fl]; cacheFonteGravar(0);
        canais = NULL; cats = NULL; nCanais = 0; memset(ltvF, 0, sizeof ltvF);
        stagingZerar(); sTeto = 100;
        assert(cacheFonteLer("xt") == 2);
        sports = other = 0;
        for (int i = 0; i < sLtvF[0].ngi; i++) {
          GGrupoInfo *g = &sLtvF[0].gi[i];
          if (!strcmp(g->nome, "Esportes")) sports += g->n;
          if (!strcmp(g->nome, "Outros")) other += g->n;
        }
        if (sports != 1 || other != 1 || sCanais[0].sug ||
            strcmp(sCats[sCanais[0].cat].nome, "Esportes")) failures++;
      }
      free(xt.giDe);
    }
  }
  enabled = 1;
  ltv_pref_definir_grupos(ltv_pref("xt", 1), NULL, 0);
  printf("%s: Xtream inferred selector counts and selection with suggestions on/off\n", failures ? "FAIL" : "PASS");
  return failures != 0;
}

static int selectedOverflow(void) {
  int failures = 0;
  const char *selected[] = {"Infantil"};
  livecat_postar_teste(overflowWorker);
  for (int load = 0; load < 2; load++) {
    stagingZerar(); sTeto = 30000;
    M3uCtx ctx = {0};
    ctx.fl = sFonte(LTV_M3U, "https://overflow.test", "Overflow", "");
    ltv_pref_definir_grupos(ltv_pref(sLtvF[ctx.fl].chave, 1), selected, 1);
    ctx.nsel = sGruposEscolhidos(ctx.fl, ctx.sel, 256);
    for (int i = 0; i < 1201; i++) {
      char name[32]; snprintf(name, sizeof name, "unresolved %04d", i);
      addM3u(&ctx, name, "", name);
    }
    addM3u(&ctx, "Latechannel", "", "late");
    if ((!load && sNCanais > 1200) || (load && sNCanais != 2)) {
      puts("FAIL: P1 candidate storage is bounded and cached negatives free slots on reload");
      failures++;
    }
    int before = posts;
    sugerirSecoes(); selecionarSecoes();
    if (posts - before > 6 || (load && (sNCanais != 1 || strcmp(sCanais[0].nome, "Latechannel")))) {
      puts("FAIL: P1 later selected channel resolves on reload within network batch limit");
      failures++;
    }
  }
  if (!failures) puts("PASS: P1 later selected channel resolves after 1200 negatives; bounded requests");
  stagingZerar();
  return failures;
}

static char *unavailableWorker(const char *url, const char *body, int *status) {
  (void)url; (void)body; posts++; *status = 503;
  return NULL;
}

static int stagingBudget(void) {
  int failures = 0;
  const char *selected[] = {"Infantil"};
  const int limits[] = {5000, 8000, 20000};
  for (int mode = 0; mode < 3; mode++) {
    stagingZerar(); sTeto = limits[mode];
    M3uCtx ctx = {0};
    ctx.fl = sFonte(LTV_M3U, "https://budget.test", "Budget", "");
    ltv_pref_definir_grupos(ltv_pref(sLtvF[ctx.fl].chave, 1), selected, 1);
    ctx.nsel = sGruposEscolhidos(ctx.fl, ctx.sel, 256);
    size_t full = 0;
    int bounded = 1;
    for (int i = 0; i < limits[mode] + 100; i++) {
      char name[32]; snprintf(name, sizeof name, "budget candidate %05d", i);
      addM3u(&ctx, name, "", name);
      if (i == 1199) full = ltv_arena_bytes(&sArena);
      if (i >= 1200 && (sNCanais > 1200 || ltv_arena_bytes(&sArena) != full)) bounded = 0;
    }
    // All sources share the candidate budget. Known matches still have their quota.
    int addon = sFonte(LTV_ADDON, "https://budget-addon.test", "Addon", "");
    ltv_pref_definir_grupos(ltv_pref(sLtvF[addon].chave, 1), selected, 1);
    GCru c = {0}; c.id = "pending"; c.nome = "budget addon candidate";
    c.grupo = "Outros"; c.sug = 1;
    if (sCanalAdd(addon, &c) || sNCanais > 1200) bounded = 0;
    addM3u(&ctx, "Lumen", "", "late-cached-match");
    if (!bounded || sNCanais != 1201 || sPendentes != 1200 ||
        strcmp(sCanais[sNCanais - 1].nome, "Lumen") ||
        !sLtvF[ctx.fl].cortada || !sLtvF[addon].cortada) failures++;
    int before = posts;
    livecat_postar_teste(unavailableWorker);
    sugerirSecoes();
    if (posts != before + 1 || sNCanais != 1201) failures++;
    selecionarSecoes();
    if (sNCanais != 1 || strcmp(sCanais[0].nome, "Lumen")) failures++;
  }
  printf("%s: P1 staging bounds candidates and arena at low-memory/TV/Android limits; late cached match survives\n",
         failures ? "FAIL" : "PASS");
  stagingZerar();
  return failures != 0;
}

static int addonBudget(void) {
  int failures = 0;
  const char *selected[] = {"Infantil"};
  GFonte source = {0};
  snprintf(source.base, sizeof source.base, "https://addon-budget.test");
  snprintf(source.tipo, sizeof source.tipo, "tv");
  snprintf(source.id, sizeof source.id, "channels");
  GAddonCarga addon = {0};
  snprintf(addon.base, sizeof addon.base, "%s", source.base); addon.ativo = 1;
  addonsCarga = &addon; nAddonsCarga = 1;
  addonPage = malloc(16384); assert(addonPage);
  size_t used = (size_t)snprintf(addonPage, 16384, "{\"metas\":[");
  for (int i = 0; i < 100; i++)
    used += (size_t)snprintf(addonPage + used, 16384 - used,
      "%s{\"id\":\"candidate-%d\",\"name\":\"unresolved %04d\"}", i ? "," : "", i, i);
  snprintf(addonPage + used, 16384 - used, "]}");
  stagingZerar(); sTeto = 5000;
  int fl = sFonte(LTV_ADDON, source.base, "Addon", source.base);
  ltv_pref_definir_grupos(ltv_pref(sLtvF[fl].chave, 1), selected, 1);
  int novos;
  // Cached negative page must count as progress, even though it retains no channels.
  if (lerPagina(&source, fl, 0, &novos) != 100 || novos == 0 || sNCanais != 0) failures++;
  if (lerPagina(&source, fl, 100, &novos) != 0 || novos != 0) failures++;
  snprintf(addonPage, 16384, "{\"metas\":[{\"id\":\"late\",\"name\":\"Latechannel\"}]}");
  if (lerPagina(&source, fl, 200, &novos) != 1 || novos != 1 || sNCanais != 1 ||
      strcmp(sCanais[0].nome, "Latechannel")) failures++;
  // A full pending budget must stop allocating when another full page arrives.
  for (int i = 0; i < 1200; i++) {
    char name[32]; snprintf(name, sizeof name, "addon pending %04d", i);
    GCru c = {0}; c.id = c.nome = name; c.grupo = "Outros"; c.sug = 1;
    sCanalAdd(fl, &c);
  }
  used = (size_t)snprintf(addonPage, 16384, "{\"metas\":[");
  for (int i = 0; i < 100; i++)
    used += (size_t)snprintf(addonPage + used, 16384 - used,
      "%s{\"id\":\"overflow-%d\",\"name\":\"addon overflow %04d\"}", i ? "," : "", i, i);
  snprintf(addonPage + used, 16384 - used, "]}");
  size_t bytes = ltv_arena_bytes(&sArena);
  if (lerPagina(&source, fl, 100, &novos) != 100 || novos != 0 ||
      sNCanais != 1201 || ltv_arena_bytes(&sArena) != bytes || !sLtvF[fl].cortada) failures++;
  printf("%s: P1 add-on pages respect candidate cap and progress past cached negatives\n", failures ? "FAIL" : "PASS");
  addonsCarga = NULL; nAddonsCarga = 0; free(addonPage); addonPage = NULL;
  stagingZerar();
  return failures != 0;
}

static int addonPagination(void) {
  int failures = 0;
  const char *selected[] = {"Infantil"};
  GFonte source = {.base = "https://pagination.test", .tipo = "tv", .id = "channels"};
  GAddonCarga addon = {.base = "https://pagination.test", .ativo = 1};
  addonsCarga = &addon; nAddonsCarga = 1;
  addonPage = malloc(16384); assert(addonPage);
  addonPaginated = 1;
  livecat_postar_teste(overflowWorker);
  for (int load = 0; load < 2; load++) {
    stagingZerar(); sTeto = 5000; addonRequests = 0;
    int fl = sFonte(LTV_ADDON, source.base, "Addon", source.base);
    ltv_pref_definir_grupos(ltv_pref(sLtvF[fl].chave, 1), selected, 1);
    if (!carregarCatalogo(&source, fl) || addonRequests != 13 ||
        sNCanais != (load ? 1 : 1200) || (!load && !sLtvF[fl].cortada)) failures++;
    int before = posts;
    sugerirSecoes(); selecionarSecoes();
    if (sNCanais || posts - before != (load ? 1 : 6)) failures++;
    if (load) {
      int resolved = 0;
      livecat_secao("page pending 1200", &resolved);
      if (!resolved) failures++;
    }
  }
  printf("%s: P1 add-on pagination scans later pages while bounding candidates and resolves them on reload\n",
         failures ? "FAIL" : "PASS");
  addonPaginated = 0;
  addonsCarga = NULL; nAddonsCarga = 0; free(addonPage); addonPage = NULL;
  stagingZerar();
  return failures != 0;
}

static int cacheWorkBound(void) {
  int failures = 0;
  stagingZerar();
  int fl = sFonte(LTV_M3U, "https://cache-work.test", "Cache work", "");
  for (int i = 0; i < 1000; i++) {
    char name[32]; snprintf(name, sizeof name, "Group %04d", i);
    sGrupoInfo(fl, name, 30);
  }
  GCanal *fixture = calloc(30000, sizeof *fixture);
  assert(fixture);
  GCat sections[] = {{.nome = "Group 0999"}};
  for (int i = 0; i < 30000; i++) {
    GCanal *c = &fixture[i];
    c->fonte = 0; c->cat = 0; c->grupoOriginal = "Group 0000";
    c->id = c->nome = c->logo = c->desc = c->epgId = c->tvgNome = "";
    c->url = c->ua = c->ref = c->tipo = "";
  }
  canais = fixture; cats = sections; nCanais = 30000;
  ltvF[0] = sLtvF[fl]; ltvF[0].nCanais = 30000;
  for (int eligible = 0; eligible <= 1; eligible++) {
    for (int i = 0; i < nCanais; i++) fixture[i].sug = eligible;
    // Reflect the move in published metadata; cache must reverse it exactly.
    ltvF[0].gi[0].n = eligible ? 0 : 30;
    ltvF[0].gi[999].n = eligible ? 60 : 30;
    if (eligible) for (int i = 30; i < nCanais; i++) fixture[i].sug = 0;
    coverageSteps = 0;
    cacheFonteGravar(0);
    unsigned long long steps = coverageSteps;
    if (steps > 3000000) failures++;
    printf("%s: cache reconciliation bounded with %d eligible channels (%llu steps)\n",
           steps > 3000000 ? "FAIL" : "PASS", eligible ? 30 : 0, steps);
    char name[64], path[600];
    cacheNomeFonte(ltvF[0].chave, name, sizeof name);
    assert(dados_caminho(path, sizeof path, name));
    FILE *fp = fopen(path, "rb"); assert(fp);
    assert(fseek(fp, 0, SEEK_END) == 0);
    long length = ftell(fp); rewind(fp);
    char *bytes = malloc((size_t)length); assert(bytes);
    assert(fread(bytes, (size_t)length, 1, fp) == 1); fclose(fp);
    CLeitor reader = {bytes, bytes + length, 1};
    long long header[8];
    for (int i = 0; i < 8; i++) header[i] = lInt(&reader);
    assert(header[3] == 30000 && header[4] == 1000);
    char value[1024];
    for (int i = 0; i < 3; i++) lStr(&reader, value, sizeof value);
    for (int i = 0; i < 1000; i++) {
      lStr(&reader, value, sizeof value);
      assert(lInt(&reader) == 30);
      assert(lInt(&reader) == 1);
    }
    assert(reader.ok);
    assert(ltvF[0].gi[0].n == (eligible ? 0 : 30));
    assert(ltvF[0].gi[999].n == (eligible ? 60 : 30));
    free(bytes);
  }
  canais = NULL; cats = NULL; nCanais = 0;
  memset(ltvF, 0, sizeof ltvF); free(fixture);
  stagingZerar();
  return failures != 0;
}

int main(int argc, char **argv) {
  M3uCtx ctx = {0};
  dados_iniciar(NULL);
  (void)argv;
  if (argc > 1) {
    char source[LTV_CHAVE];
    const char *selected[] = {"Infantil"};
    ltv_chave_fonte(LTV_M3U, "https://private/list?token=secret", source, sizeof source);
    int failures = 0;
    ltv_pref_definir_grupos(ltv_pref(source, 1), NULL, 0);
    sTeto = 100;
    assert(cacheFonteLer(source) == 4);
    selecionarSecoes();
    if (strcmp(sCats[sCanais[0].cat].nome, "Esportes") ||
        strcmp(sCats[sCanais[1].cat].nome, "Infantil") || posts != 0) {
      puts("FAIL: cold process restores cached suggestions with all sections selected");
      failures++;
    } else puts("PASS: cold process restores cached suggestions with all sections selected");
    stagingZerar();
    ltv_pref_definir_grupos(ltv_pref(source, 1), selected, 1);
    sTeto = 1;
    assert(cacheFonteLer(source) == 1);
    selecionarSecoes();
    assert(sNCanais == 1 && !strcmp(sCanais[0].nome, "Lumen"));
    assert(!strcmp(sCats[sCanais[0].cat].nome, "Infantil") && posts == 0);
    puts("PASS: cold process restores selected suggested section from disk without network");
    return failures ? 1 : 0;
  }
  sTeto = 100;
  ctx.fl = sFonte(LTV_M3U, "https://private/list?token=secret", "My list", "");
  addM3u(&ctx, "Aurora HD", "", "https://private/1");
  addM3u(&ctx, "Lumen", "", "https://private/2");
  addM3u(&ctx, "ESPN HD", "", "https://private/3");
  addM3u(&ctx, "Already Classified", "Provider custom", "https://private/4");
  assert(sNCanais == 4);
  assert(sCanais[0].sug && sCanais[1].sug && !sCanais[2].sug && !sCanais[3].sug);
  livecat_postar_teste(worker);
  enabled = 0; sugerirSecoes(); assert(posts == 0);
  enabled = 1; sugerirSecoes();
  assert(posts == 1);
  assert(!strcmp(sCats[sCanais[0].cat].nome, "Esportes"));
  assert(!strcmp(sCats[sCanais[1].cat].nome, "Infantil"));
  assert(!strcmp(sCats[sCanais[2].cat].nome, "Esportes"));
  assert(!strcmp(sCats[sCanais[3].cat].nome, "Provider custom"));
  checkGroups();
  puts("PASS: suggested sections appear in source selector with accurate counts");
  puts("PASS: unclassified list builds suggested sections; local/provider categories untouched; setting off sends nothing");
  cargaCancelada = 1; sugerirSecoes(); assert(posts == 1);
  puts("PASS: cancelled guide load sends nothing");
  cargaCancelada = 0;

  // Persist eligibility and original groups, not the suggested section.
  char source[LTV_CHAVE]; snprintf(source, sizeof source, "%s", sLtvF[0].chave);
  canais = sCanais; cats = sCats; nCanais = sNCanais;
  ltvF[0] = sLtvF[0]; cacheFonteGravar(0);
  canais = NULL; cats = NULL; nCanais = 0; memset(ltvF, 0, sizeof ltvF);
  stagingZerar(); sTeto = 100;
  enabled = 0;
  assert(cacheFonteLer(source) == 4);
  assert(sCanais[0].sug && !strcmp(sCats[sCanais[0].cat].nome, "Outros"));
  sugerirSecoes();
  assert(!strcmp(sCats[sCanais[0].cat].nome, "Outros"));
  enabled = 1; sugerirSecoes();
  assert(posts == 1 && !strcmp(sCats[sCanais[0].cat].nome, "Esportes"));
  assert(!strcmp(sCats[sCanais[3].cat].nome, "Provider custom"));
  checkGroups();
  // Choosing one suggested section must survive a fresh provider load.
  LtvPref *pref = ltv_pref(source, 1);
  const char *selected[] = {"Infantil"};
  ltv_pref_definir_grupos(pref, selected, 1);
  stagingZerar(); sTeto = 100;
  ctx.fl = sFonte(LTV_M3U, "https://private/list?token=secret", "My list", "");
  ctx.nsel = sGruposEscolhidos(ctx.fl, ctx.sel, 256);
  addM3u(&ctx, "Aurora HD", "", "https://private/1");
  addM3u(&ctx, "Lumen", "", "https://private/2");
  addM3u(&ctx, "ESPN HD", "", "https://private/3");
  sugerirSecoes();
  selecionarSecoes();
  assert(sNCanais == 1 && !strcmp(sCanais[0].nome, "Lumen"));
  assert(!strcmp(sCats[sCanais[0].cat].nome, "Infantil"));
  assert(posts == 1);
  // Startup adopts the source cache without running the network suggestion pass.
  int failures = 0;
  stagingZerar(); sTeto = 100;
  assert(cacheFonteLer(source) > 0);
  selecionarSecoes();
  if (sNCanais != 1 || strcmp(sCanais[0].nome, "Lumen") ||
      strcmp(sCats[sCanais[0].cat].nome, "Infantil") || posts != 1) {
    puts("FAIL: cold source cache retains the selected suggested section without network");
    failures++;
  } else puts("PASS: cold source cache retains the selected suggested section without network");

  stagingZerar(); sTeto = 1;
  ltv_pref_definir_grupos(ltv_pref("st", 1), selected, 1);
  stalkerCarregar();
  sugerirSecoes(); selecionarSecoes();
  if (sNCanais != 1 || strcmp(sCanais[0].nome, "Lumen") ||
      posts != 1) {
    puts("FAIL: selected suggested section gets capacity after an unselected Stalker candidate");
    failures++;
  } else puts("PASS: selected suggested section gets capacity after an unselected Stalker candidate");

  // An earlier unselected candidate must not spend the last retained-channel slot.
  stagingZerar(); sTeto = 1;
  ctx.fl = sFonte(LTV_M3U, "https://private/list?token=secret", "My list", "");
  ctx.nsel = sGruposEscolhidos(ctx.fl, ctx.sel, 256);
  addM3u(&ctx, "Aurora HD", "", "https://private/1");
  addM3u(&ctx, "Lumen", "", "https://private/2");
  sugerirSecoes(); selecionarSecoes();
  if (sNCanais != 1 || strcmp(sCanais[0].nome, "Lumen") ||
      posts != 1) {
    puts("FAIL: selected suggested section gets capacity after an unselected M3U candidate");
    failures++;
  } else puts("PASS: selected suggested section gets capacity after an unselected M3U candidate");

  stagingZerar(); sTeto = 1;
  cacheFonteLer(source);
  selecionarSecoes();
  if (sNCanais != 1 || strcmp(sCanais[0].nome, "Lumen") ||
      posts != 1) {
    puts("FAIL: cold source cache applies suggested selection before channel capacity");
    failures++;
  } else puts("PASS: cold source cache applies suggested selection before channel capacity");
  ltv_pref_definir_grupos(pref, NULL, 0);
  stagingZerar(); sTeto = 100;
  assert(cacheFonteLer(source) == 4);
  sugerirSecoes();
  puts("PASS: source cache preserves fallback; suggested selector selection survives reload without network");

  XtCtx xt = {0}; xt.fl = sFonte(LTV_XTREAM, "", "Xtream", "");
  XtreamItem it = {0};
  it.id = "xtream:1"; it.nome = "Aurora HD"; it.categoria = "Outros";
  it.logo = it.epgId = ""; it.semCategoria = 1;
  assert(xtCanal(&it, &xt) && sCanais[4].sug);
  it.id = "xtream:2"; it.semCategoria = 0;
  assert(xtCanal(&it, &xt) && !sCanais[5].sug);
  sugerirSecoes();
  assert(!strcmp(sCats[sCanais[4].cat].nome, "Esportes"));
  assert(!strcmp(sCats[sCanais[5].cat].nome, "Outros"));
  puts("PASS: uncategorized Xtream channels get suggestions; provider category named Outros stays untouched");

  stagingZerar(); sTeto = 1;
  ltv_pref_definir_grupos(pref, selected, 1);
  ctx.fl = sFonte(LTV_M3U, "https://private/list?token=secret", "My list", "");
  ctx.nsel = sGruposEscolhidos(ctx.fl, ctx.sel, 256);
  livecat_postar_teste(coldWorker);
  addM3u(&ctx, "Velora", "", "https://private/5");
  addM3u(&ctx, "Nimbo", "", "https://private/6");
  addM3u(&ctx, "Talume", "", "https://private/7");
  sugerirSecoes(); selecionarSecoes();
  assert(sNCanais == 1 && !strcmp(sCanais[0].nome, "Nimbo"));
  assert(posts == 2 && sLtvF[ctx.fl].cortada);
  puts("PASS: uncached suggestions filter before quota and retain at most the channel cap");
  failures += xtreamLocalSections();
  failures += cacheWorkBound();
  failures += selectedOverflow();
  failures += stagingBudget();
  failures += addonBudget();
  failures += addonPagination();
  return failures ? 1 : 0;
}
