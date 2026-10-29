// CACHE POR FONTE do guia (M1): a fonte REMOVIDA nao volta do disco.
//
// Relato do dono (Discord): removeu a conta Xtream e os addons e o guia
// continuava mostrando "714 canais" — o cache por fonte em disco nao sabia
// que a fonte tinha saido da configuracao. A regra agora:
//
//   cacheLer() pula a chave que a configuracao atual nao justifica;
//   a fonte que so FALHOU a rede continua configurada e entra normal;
//   guia_fonte_esquecer() apaga o arquivo e some os canais na hora;
//   guia_limpar_lista() clears all profile list caches and reloads sources.
//
// Sem rede e com disco de verdade: dados.c le e grava num diretorio de teste.
#define NV_PERFIL_SESSAO_FORTE
#include <unistd.h>
#ifndef GUIACACHE_SOURCE
#define GUIACACHE_SOURCE "../src/guia.c"
#endif
#include GUIACACHE_SOURCE
#include <assert.h>

// --- dubles: a "configuracao atual" mora aqui --------------------------------
static int nAddons;
static const char *bases[128];
static int xtOk, stOk, addonDisabled;
static int perfilAtivo = 1;
int perfis_ativo(void) { return perfilAtivo; }
int addons_n(void) { return nAddons; }
const char *addons_base(int i) { return bases[i]; }
const char *addons_nome(int i) { (void)i; return "addon"; }
int addons_ativo(int i) { return !(addonDisabled && i == 0); }
int addons_base_desligada(const char *b) { return addonDisabled && !strcmp(b, bases[0]); }
int addons_sondado(int i) { (void)i; return 1; }
int addons_catalogos_canal(int i, AddCatCanal *s, int m) { (void)i; (void)s; (void)m; return 0; }
int addons_ocupado(void) { return 1; }   // 1 = prefetch nao atrapalha
int player_carregando(void) { return 0; }
int addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida) {
  (void)id; (void)tipo; (void)base; (void)fios; (void)cancelado; (void)ctx;
  *saida = NULL; return 0;
}
int addons_consultar_sessao(const char *id, const char *tipo, const char *base, int fios,
                            int (*cancelado)(void *), void *ctx, Stream **saida, uint64_t se) {
  (void)se;
  return addons_consultar(id, tipo, base, fios, cancelado, ctx, saida);
}
const char *ajustes_epg_pais(void) { return ""; }
int ajustes_idioma(void) { return 0; }
int ajustes_livetv_sugerir(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
int xtream_e_id(const char *id) {
#ifdef GUIAFILTRO_LOADER
  return id && !strncmp(id, "xtream:", 7);
#else
  (void)id; return 0;
#endif
}
Uint32 SDL_GetTicks(void) { static Uint32 t = 1000; return t += 16; }
int xtream_configurado(void) { return xtOk; }
#ifdef GUIAFILTRO_LOADER
static int fixtureFailure = XT_SEM_RESPOSTA;
#endif
int xtream_ultima_falha(void) {
#ifdef GUIAFILTRO_LOADER
  return fixtureFailure;
#else
  return 0;
#endif
}
int xtream_ultimo_http(void) { return 0; }
#ifndef GUIAFILTRO_STALKER_TEST
int stalker_configurado(void) { return stOk; }
#endif
int xtream_url_xmltv(char *d, unsigned n) { (void)d; (void)n; return 0; }
int xtream_conta(XtreamConta *c) { (void)c; return 0; }
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return g == 0; }
int perfis_sessao_travar(uint64_t g) { (void)g; return 1; }
void perfis_sessao_destravar(void) {}
const char *i18n(const char *s) { return s; }

// Rede: nunca responde (o que vale aqui e o cache, nao a carga).
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *rede_baixar_medido_controle(const char *u, int s, const char *const *c,
                                  const RedeControle *ct, RedeMedida *m) {
  (void)s; (void)c; (void)ct; (void)m;
#ifdef GUIAFILTRO_LOADER
  return filtroResposta(u);
#else
  (void)u; return NULL;
#endif
}
int rede_baixar_fluxo(const char *u, int s, const char *const *c,
                      int (*cb)(const char *, size_t, void *), void *x,
                      long long max, int *st, long long *bytes) {
  (void)s; (void)c; (void)max;
  if (st) *st = 0; if (bytes) *bytes = 0;
#ifdef GUIAFILTRO_LOADER
  char *body = filtroResposta(u);
  if (body) {
    size_t n = strlen(body);
    int ok = cb(body, n, x);
    free(body);
    if (st) *st = 200; if (bytes) *bytes = (long long)n;
    return ok;
  }
#else
  (void)u; (void)cb; (void)x;
#endif
  return 0;
}
#ifndef GUIAFILTRO_STALKER_TEST
int stalker_canais_sessao(StalkerCanal *s, int m, uint64_t se) { (void)s; (void)m; (void)se; return 0; }
int stalker_canais_cb_sessao(StalkerCb cb, void *u, uint64_t se) { (void)cb; (void)u; (void)se; return 0; }
#endif
int xtream_canais_cb_sessao(void (*cat)(const char *, int, int, void *),
                            XtreamCb cb, void *u, int *total, uint64_t sessao) {
  (void)sessao;
  if (total) *total = 0;
#ifdef GUIAFILTRO_LOADER
  if (inventoryFixture(cb, u, total)) { fixtureFailure = XT_OK; return 1; }
  char *name = filtroResposta("xtream");
  fixtureFailure = name ? XT_OK : XT_SEM_RESPOSTA;
  if (name) {
    XtreamItem item = {0};
    item.id = "xtream:1"; item.nome = name; item.categoria = "Sports";
    item.logo = item.epgId = ""; item.catOrdem = 0;
    cat("Sports", 0, 1, u); cb(&item, u);
    free(name);
    if (total) *total = 1;
    return 1;
  }
#else
  (void)cat; (void)cb; (void)u;
#endif
  return 0;
}

// Account-aware adapter preserves the same removed/configured source fixtures.
#ifdef NV_IPTVFONTES_H
int ajustes_guia_ordem(void) { return 1; }
int ajustes_guia_fonte(void) { return 0; }
int iptv_contas(IptvConta *out,int max) {
  int n=0;
  if(xtOk && n<max)out[n++]=(IptvConta){.tipo=IPTV_XTREAM,.ativa=1,.nome="Xtream"};
  if(stOk && n<max)out[n++]=(IptvConta){.tipo=IPTV_STALKER,.ativa=1,.nome="Stalker"};
  return n;
}
int xtream_fonte_configurado(unsigned id) { return id==0 && xtOk; }
#ifndef GUIAFILTRO_STALKER_TEST
int stalker_fonte_configurado(unsigned id) { return id==0 && stOk; }
int stalker_fonte_canais_cb(unsigned id, StalkerCb cb, void *u, uint64_t sessao) { (void)id;return stalker_canais_cb_sessao(cb,u,sessao); }
#endif
int xtream_fonte_canais_cb(unsigned id,void (*cat)(const char*,int,int,void*),XtreamCb cb,void *u,int *total,uint64_t sessao) {
  (void)id;return xtream_canais_cb_sessao(cat,cb,u,total,sessao);
}
int xtream_fonte_estado(unsigned id,int *http) { (void)id;*http=0;return xtream_ultima_falha(); }
int xtream_fonte_xmltv(unsigned id,char *out,unsigned n) { (void)id;return xtream_url_xmltv(out,n); }
#ifndef GUIAFILTRO_STALKER_TEST
int stalker_fonte_canais(unsigned id,StalkerCanal *out,int n,uint64_t sessao) { (void)id;return stalker_canais_sessao(out,n,sessao); }
#endif
int m3u_fonte_urls(unsigned id,char *url,unsigned n,char *epg,unsigned ne) { (void)id;(void)url;(void)n;(void)epg;(void)ne;return 0; }
#endif

// Tex/orcamento so para ltv_teto_canais nao consultar nada.
void tex_orcamento_info(int *mb, long *memTotal, int *fixo, int *slots) {
  if (mb) *mb = 0; if (memTotal) *memTotal = 4096;
  if (fixo) *fixo = 0; if (slots) *slots = 0;
}
PtvPlataforma ptv_plataforma(void) { return PTV_LG; }

// Drawing stubs keep the real confirmation's pointer registration executable.
static int pointerTargets, pointerLayers, clearInk, cancelInk, focusInk = 18;
void ponteiro_camada(void) { pointerTargets = 0; pointerLayers++; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn foco,
                   PonteiroFn ativar, int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)foco; (void)ativar; (void)a; (void)b;
  pointerTargets++;
}
int ajustes_vidro(void) { return 0; }
int ajustes_tinta_foco(void) { return focusInk; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float px, float py,
              float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)px; (void)py;
  (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_folha(GfxRect r, float raio, float a) { (void)r; (void)raio; (void)a; }
void plrui_linha_foco(GfxRect r, float raio, float a) { (void)r; (void)raio; (void)a; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  if (!strcmp(s, "Limpar")) clearInk = r < g ? r : g < b ? g : b;
  if (!strcmp(s, "Cancelar")) cancelInk = r < g ? r : g < b ? g : b;
  (void)e; (void)a; return (TxtLinha){0};
}
void txt_desenhar_alpha(TxtLinha t, float x, float y, float a) { (void)t; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y,
                float w, float leading, float a, int max) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y;
  (void)w; (void)leading; (void)a; (void)max; return 0;
}

// --- semeia o cache no disco pelo caminho de verdade -------------------------
static void semearFonteCacheTipo(int tipo, const char *base, const char *grupo, const char **nomes, int n) {
  int i, fl;
  stagingZerar();
  sTeto = 30000;
  fl = sFonte(tipo, base, base, tipo == LTV_ADDON ? base : "");
  assert(fl >= 0);
  for (i = 0; i < n; i++) {
    char id[32];
    GCru c;
    memset(&c, 0, sizeof c);
    snprintf(id, sizeof id, "%s-%d", base, i);
    c.id = id; c.nome = nomes[i]; c.logo = c.desc = "";
    c.epgId = c.tvgNome = c.url = c.ua = c.ref = "";
    c.grupo = grupo; c.numero = i + 1;
    sGrupoInfo(fl, grupo, 1);
    assert(sCanalAdd(fl, &c));
    sLtvF[fl].total++;
  }
  adotarStaging();          // staging -> publicado (canais/ltvF)
  cacheGravar();            // grava ltv-<chave>-p1.bin + indice
}

static void semearFonteCache(const char *base, const char *grupo, const char **nomes, int n) {
  semearFonteCacheTipo(LTV_ADDON, base, grupo, nomes, n);
}

static int temCanal(const char *nome) {
  int i;
  for (i = 0; i < nCanais; i++) if (!strcmp(canais[i].nome, nome)) return 1;
  return 0;
}

static void resetPublished(void) {
  fontesLtvSoltar(ltvF, nLtvF); nLtvF = 0;
  free(canais); canais = NULL; nCanais = capCanais = 0;
  free(cats); cats = NULL; nCats = capCats = 0;
  ltv_arena_soltar(&arena);
}

int main(int argc, char **argv) {
  static const char *a[] = { "Canal A1", "Canal A2" }, *b[] = { "Canal B1" };
  char chA[LTV_CHAVE], chB[LTV_CHAVE], nome[64], caminho[600];
  assert(argc == 2);
#ifndef GUIACACHE_BASELINE
  if (getenv("GUIACACHE_CONTRAST_ONLY")) {
    // White/gold accents return dark ink; both focused labels need light ink.
    limparConfirm = 1;
    for (int ink = 18; ink <= 240; ink += 222) {
      focusInk = ink;
      for (int f = 0; f < 2; f++) {
        limparFoco = f; desenharLimpar(1.0f);
        assert(clearInk >= 180 && cancelInk >= 180);
      }
    }
    puts("PASS: confirmation labels remain readable with light/dark accents");
    return 0;
  }
#endif
  dados_iniciar(argv[1]);
  bases[0] = "https://a.example"; bases[1] = "https://b.example";
  nAddons = 2;
  ltv_chave_fonte(LTV_ADDON, bases[0], chA, sizeof chA);
  ltv_chave_fonte(LTV_ADDON, bases[1], chB, sizeof chB);
  semearFonteCache(bases[0], "Grupo", a, 2);
  semearFonteCache(bases[1], "Grupo", b, 1);
  // Each seed publishes one source; index must retain both fixture sources.
  cacheNomeIndice(nome, sizeof nome);
  { char idx[128]; snprintf(idx, sizeof idx, "%s\n%s\n", chA, chB); dados_gravar(nome, idx); }
  resetPublished();
  assert(cacheLer() && nCanais == 3 && nLtvF == 2);
  assert(temCanal("Canal A1") && temCanal("Canal B1"));
#ifndef GUIACACHE_BASELINE
  // Disabling an installed source must preserve disk lists, EPG and favorites.
  snprintf(fav[0].chave, sizeof fav[0].chave, "%s", chA);
  snprintf(fav[0].id, sizeof fav[0].id, "%s", canais[0].id);
  nFav = favLido = 1; favGravar();
  snprintf(nome, sizeof nome, "epg-x-%s-p1.bin", chA); dados_gravar(nome, "saved EPG");
  addonDisabled = 1;
  resetPublished();
  cacheLer();
  assert(nFav == 1 && !strcmp(fav[0].chave, chA));
  assert(dados_caminho(caminho, sizeof caminho, nome) && access(caminho, F_OK) == 0);
  cacheNomeFonte(chA, nome, sizeof nome);
  assert(dados_caminho(caminho, sizeof caminho, nome) && access(caminho, F_OK) == 0);
  assert(!temCanal("Canal A1"));
  addonDisabled = 0;
  resetPublished();
  assert(cacheLer() && temCanal("Canal A1") && nFav == 1);
  stagingZerar(); sTeto = 30000;
  fonteVaziaRecupera(LTV_ADDON, bases[0], "A", bases[0]);
  assert(sNCanais == 2 && sLtvF[0].deCache);
  stagingZerar();
  // Removal must invalidate focus and options before source/channel compaction.
  for (int mode = FOP_FONTE; mode <= FOP_GRADE; mode++)
  for (int scenario = 0; scenario < 4; scenario++) {
    semearFonteCache(bases[0], "Grupo A", a, 2);
    semearFonteCache(bases[1], "Grupo B", b, 1);
    cacheNomeIndice(nome, sizeof nome);
    { char idx[128]; snprintf(idx, sizeof idx, "%s\n%s\n", chA, chB); dados_gravar(nome, idx); }
    resetPublished();
    assert(cacheLer() && nLtvF == 2);
    if (scenario == 3) guia_fonte_esquecer(chA);
    gpAbrir(scenario >= 2 ? 0 : 1);
    assert(gpAberto && gpN > 0);
    gpMarca[0] = 1;
    gpFoco = 0; // Apply is focused when the source disappears.
    focoFontes = 1; fontesCol = scenario >= 2 ? 2 : 3;
    fontesOkDesde = SDL_GetTicks(); fontesOkLongo = 1;
    fopCanalAbrir(&canais[0]);
    fopModo = mode;
    guia_fonte_esquecer(scenario == 0 || scenario == 3 ? chB : chA);
    assert(!gpAberto);
    assert(!focoFontes && fontesCol == 0);
    assert(!fontesOkDesde && !fontesOkLongo);
    assert(!fopAberto && fopCanal == NULL);
    if (scenario == 3) assert(nLtvF == 0 && nCanais == 0);
    else assert(nLtvF == 1 && ltvF[0].gi && ltvF[0].ngi == 1);
  }
  free(gpMarca); gpMarca = NULL;
  semearFonteCache(bases[1], "Grupo B", b, 1);
  // Removing the first source also moves B's channel and category indices.
  assert(nCanais == 1 && nLtvF == 1 && canais[0].fonte == 0 && cats[0].fonte == 0);
  assert(!strcmp(chaveDo(&canais[0]), chB));
  semearFonteCache(bases[0], "Grupo", a, 2);
  cacheNomeIndice(nome, sizeof nome);
  { char idx[128]; snprintf(idx, sizeof idx, "%s\n%s\n", chA, chB); dados_gravar(nome, idx); }
#endif
  resetPublished();
  // Only B remains configured. A must never return from disk.
  bases[0] = bases[1]; nAddons = 1;
  assert(cacheLer());
  assert(nCanais == 1 && nLtvF == 1);
  assert(!temCanal("Canal A1") && temCanal("Canal B1"));
  // Real failed-network recovery path preserves configured B's disk list.
  stagingZerar(); sTeto = 30000;
  assert(rede_baixar(bases[0], 1) == NULL);
  fonteVaziaRecupera(LTV_ADDON, bases[0], "B", bases[0]);
  assert(sNCanais == 1 && sLtvF[0].deCache && sLtvF[0].saude == LTV_SAUDE_DEGRADADA);
  stagingZerar();
#ifndef GUIACACHE_BASELINE
  // Pending network results must be cancelled; reset also wipes B's cache.
  fioVivo = 1;
  guia_limpar_lista();
  assert(cargaCancelada && recarregarPend && !nCanais && !nLtvF);
  cacheNomeFonte(chB, nome, sizeof nome);
  assert(dados_caminho(caminho, sizeof caminho, nome));
  assert(access(caminho, F_OK) != 0);
  fioVivo = 0;
#endif
  // Cold Spotlight must load this profile's URLs before validating disk caches.
  ltv_urls_carregar(1);
  assert(ltv_urls_adicionar(LTV_URL_M3U, "Saved playlist", "https://m.example/list.m3u") >= 0);
  semearFonteCacheTipo(LTV_M3U, "https://m.example/list.m3u", "Grupo", b, 1);
  ltv_chave_fonte(LTV_M3U, "https://m.example/list.m3u", chB, sizeof chB);
  snprintf(fav[0].chave, sizeof fav[0].chave, "%s", chB);
  snprintf(fav[0].id, sizeof fav[0].id, "%s", canais[0].id);
  nFav = 1; favGravar();
  ltv_urls_carregar(2); // Simulate URLs not loaded for the active profile.
  resetPublished(); cacheLido = favLido = nFav = 0; estado = G_PARADO;
  guia_preparar_busca();
  assert(nCanais == 1 && nLtvF == 1 && temCanal("Canal B1"));
  assert(nFav == 1 && !strcmp(fav[0].chave, chB));
  cacheNomeFonte(chB, nome, sizeof nome);
  assert(dados_caminho(caminho, sizeof caminho, nome) && access(caminho, F_OK) == 0);
  stagingZerar(); sTeto = 30000;
  fonteVaziaRecupera(LTV_M3U, "https://m.example/list.m3u", "M3U", "https://m.example/list.m3u");
  assert(sNCanais == 1 && sLtvF[0].deCache);
  stagingZerar();
#ifndef GUIACACHE_BASELINE
  // Enabled add-ons without live channels must not crowd out native sources.
  for (int i = 0; i < 128; i++) bases[i] = "https://addon.example";
  nAddons = 128; xtOk = stOk = 1;
  assert(chaveConfigurada(chB));
  assert(chaveConfigurada("xt") && chaveConfigurada("st"));
  resetPublished();
  assert(cacheLer() && nCanais == 1 && nFav == 1);
  pointerTargets = 3; limparConfirm = 0;
  desenharLimpar(1.0f);
  assert(pointerTargets == 3 && pointerLayers == 0);
  limparConfirm = 1; limparFoco = 0;
  desenharLimpar(1.0f);
  assert(pointerLayers == 1 && pointerTargets == 2);
#endif
  puts("PASS: source removal, Spotlight M3U/offline/favorites, 128 add-ons, confirmation pointer isolation");
  return 0;
}
