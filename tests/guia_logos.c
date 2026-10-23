// Offline player_api -> Xtream -> guide grid -> texture request regression.
// Rendering uses the real grid row, gfx and text; fixture URLs map to local PNGs.
#define NV_PERFIL_SESSAO_FORTE
#ifndef GUIA_LOGOS_SOURCE
#define GUIA_LOGOS_SOURCE "../src/guia.c"
#endif
#include GUIA_LOGOS_SOURCE
#include <SDL2/SDL_image.h>
#include <assert.h>

int perfis_ativo(void) { return 1; }
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { return g == 0; }
int perfis_sessao_travar(uint64_t g) { return g == 0; }
void perfis_sessao_destravar(void) {}
int addons_n(void) { return 0; }
const char *addons_base(int i) { (void)i; return ""; }
int addons_ativo(int i) { (void)i; return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }
int addons_ocupado(void) { return 1; }
int ajustes_livetv_formato(void) { return 0; }
int perftv_enabled, perftv_uploads, perftv_blur, perftv_glass;
int ajustes_vidro(void) { return 0; }
int ajustes_profundidade(void) { return 0; }
float ajustes_profundidade_brilho(void) { return 0; }
int ajustes_vidro_fosco(void) { return 0; }
int ajustes_vidro_qualidade(void) { return 1; }
int ajustes_relogio_12h(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *ajustes_epg_pais(void) { return ""; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
void ajustes_acento(float *r, float *g, float *b) { *r = .3f; *g = .6f; *b = .9f; }
const char *i18n(const char *s) { return s; }
// No short-EPG worker or network thread in this fixture.
void xtepg_querer(const char *id) { (void)id; }
int xtepg_carregando(const char *id) { (void)id; return 0; }
int xtepg_tem(const char *id) { (void)id; return 0; }
int xtepg_faixa(const char *id, time_t a, time_t b, EpgProg *p, int n) {
  (void)id; (void)a; (void)b; (void)p; (void)n; return 0;
}

static const char streams[] =
  "[{\"num\":1,\"name\":\"ESPN FHD\",\"stream_id\":1,\"stream_icon\":\"http:\\/\\/logos.test\\/espn.png\",\"category_id\":\"1\"},"
  "{\"num\":2,\"name\":\"ESPN SD\",\"stream_id\":2,\"stream_icon\":\"\",\"category_id\":1},"
  "{\"num\":3,\"name\":\"Globo RJ\",\"stream_id\":3,\"stream_icon\":null,\"epg_channel_id\":\"globo.rj\",\"category_id\":1},"
  "{\"num\":4,\"name\":\"Canal M3U HD\",\"stream_id\":4,\"stream_icon\":\"\",\"epg_channel_id\":\"m3u.br\",\"category_id\":1},"
  "{\"num\":5,\"name\":\"No Logo\",\"stream_id\":5,\"category_id\":1},"
  "{\"num\":6,\"name\":\"Whitespace\",\"stream_id\":6,\"stream_icon\":\" \\nhttp://logos.test/espn.png\\t \",\"category_id\":1},"
  "{\"num\":7,\"name\":\"Invalid\",\"stream_id\":7,\"stream_icon\":\"null\",\"epg_channel_id\":\"globo.rj\",\"category_id\":1},"
  "{\"num\":8,\"name\":\"ESPN HD\",\"stream_id\":8,\"stream_icon\":\"http://logos.test/espn-hd.png\",\"category_id\":1},"
  "{\"num\":9,\"name\":\"By ID\",\"stream_id\":9,\"epg_channel_id\":\"m3u.br\",\"category_id\":1},"
  "{\"num\":10,\"name\":\"Canal M3U FHD\",\"stream_id\":10,\"category_id\":1}]";
char *rede_baixar_st_retry(const char *url, int s, const char *const *c, int *st, int *retry) {
  (void)s; (void)c; *st = 200; if (retry) *retry = 0;
  if (strstr(url, "action=get_live_categories")) return strdup("[{\"category_id\":1,\"category_name\":\"Fixture\"}]");
  if (strstr(url, "action=get_live_streams")) return strdup(streams);
  return strdup("{\"user_info\":{\"auth\":1,\"status\":\"Active\"}}");
}
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *rede_baixar_medido_controle(const char *u, int s, const char *const *c,
                                 const RedeControle *ct, RedeMedida *m) {
  (void)u; (void)s; (void)c; (void)ct; (void)m; return NULL;
}
int rede_baixar_fluxo(const char *u, int s, const char *const *c,
                     int (*cb)(const char *, size_t, void *), void *x,
                     long long max, int *st, long long *bytes) {
  (void)u; (void)s; (void)c; (void)cb; (void)x; (void)max; (void)st; (void)bytes; return 0;
}

static const char *failed, *failed2;
static char requested[600];
static GLuint texture;
#ifdef GUIA_LOGOS_RETRY
int guia_logos_retry_check(const char *name, const char *bad, const char *fallback,
                           GLuint texture, const char *(*draw)(void *), void *channel);
#else
int tex_falhou(const char *s) {
  return s && ((failed && !strcmp(failed, s)) || (failed2 && !strcmp(failed2, s)));
}
GLuint tex_obter_larg(const char *s, float w) {
  (void)w;
  if (tex_falhou(s)) return 0;
  snprintf(requested, sizeof requested, "%s", s ? s : ""); return texture;
}
float tex_aspecto(const char *s) { (void)s; return 2.0f; }
int tex_cor_fundo(const char *s, float *r, float *g, float *b) { (void)s; (void)r; (void)g; (void)b; return 0; }
int tex_logo_tom_unico(const char *s) { (void)s; return 0; }
#endif

static GCanal *channel(const char *name) {
  int i;
  for (i = 0; i < nCanais; i++) if (!strcmp(canais[i].nome, name)) return &canais[i];
  assert(0); return NULL;
}
static const time_t fixtureTime = 1800000000;
static void row(GCanal *c, float y) {
  requested[0] = 0;
  desenharLinhaLista(c, y, 0, 1, fixtureTime, fixtureTime - 600, fixtureTime, 0, G_L_FAIXA_X + 100, NULL);
  desenharLinhaLista(c, y, 0, 1, fixtureTime, fixtureTime - 600, fixtureTime, 1, G_L_FAIXA_X + 100, NULL);
}
#ifdef GUIA_LOGOS_RETRY
static const char *retryRow(void *c) {
  row(c, 80);
  return logoDe(c);
}
#endif
static int failures;
static void check(const char *name, const char *want) {
  row(channel(name), 80);
  int ok = !strcmp(requested, want);
  printf("%s grid: %s (failed=%s; want=%s; got=%s)\n", ok ? "PASS" : "FAIL",
         name, failed ? failed : "none", want, requested);
  if (!ok) failures++;
}
static void capture(const char *path) {
  const char *names[] = { "ESPN FHD", "ESPN SD", "Globo RJ", "Canal M3U HD", "No Logo" };
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int i, y; assert(pixels && s);
  txt_novo_quadro(); gfx_novo_quadro(); glClearColor(.04f, .04f, .045f, 1); glClear(GL_COLOR_BUFFER_BIT);
  txt_desenhar(txt_linha(TXT_HEADLINE, "Live TV - offline logo fixture", 240, 240, 240, 255), 64, 40);
  for (i = 0; i < 5; i++) row(channel(names[i]), 160 + i * 100);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pixels + (1079-y) * 1920*4, 1920*4);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels);
}
int main(int argc, char **argv) {
  char xml[] = "<tv><channel id='globo.rj'><display-name>Globo RJ</display-name><icon src='http://xmltv.test/globo.png?a=1&amp;b=2'/></channel>"
    "<channel id='m3u.br'><display-name>Canal M3U</display-name><icon src='http://xmltv.test/lower-priority.png'/></channel></tv>";
  const char m3u[] = "#EXTM3U\n#EXTINF:-1 tvg-id=\"m3u.br\" tvg-logo=\"http://m3u.test/canal.png\",Canal M3U\nhttp://fixture.invalid/live\n"
    "#EXTINF:-1 tvg-id=\"m3u.br\" tvg-logo=\"http://m3u.test/second.png\",Canal M3U SD\nhttp://fixture.invalid/live2\n";
  LtvM3u parser; M3uCtx ctx = {0};
  SDL_Window *win; SDL_GLContext gl; SDL_Surface *image;
  assert(argc == 3); dados_iniciar(argv[1]);
  xtream_definir_servidor("http://fixture.invalid"); xtream_definir_usuario("fixture"); xtream_definir_senha("fixture");
  stagingZerar(); sTeto = 30000;
  ctx.fl = sFonte(LTV_M3U, "fixture-list", "Fixture M3U", "");
  ltv_m3u_iniciar(&parser, m3uCanal, &ctx);
  assert(ltv_m3u_alimentar(&parser, m3u, strlen(m3u))); ltv_m3u_fim(&parser);
  assert(xtreamCarregar() == 10); adotarStaging();
  assert(epg_xml_processar(xml) == 0); // Icons work even without programmes.
  assert(SDL_Init(SDL_INIT_VIDEO) == 0); IMG_Init(IMG_INIT_PNG);
  win = SDL_CreateWindow("Offline Live TV", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl); glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1));
  image = IMG_Load("tests/fixtures/logos/recortado.png"); assert(image);
  SDL_Surface *rgba = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA32, 0); assert(rgba);
  // Make the synthetic mark readable on the guide's dark background.
  for (int y = 0; y < rgba->h; y++) for (int x = 0; x < rgba->w; x++) {
    unsigned char *p = (unsigned char *)rgba->pixels + y * rgba->pitch + x * 4;
    p[0] = p[1] = p[2] = 240;
  }
  char fixturePath[700]; snprintf(fixturePath, sizeof fixturePath, "%s", argv[2]);
  char *slash = strrchr(fixturePath, '/'); assert(slash); strcpy(slash + 1, "fixture-logo.png");
  assert(IMG_SavePNG(rgba, fixturePath) == 0);
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  SDL_FreeSurface(rgba); SDL_FreeSurface(image); gfx_tex_esquecer(0);
#ifdef GUIA_LOGOS_RETRY
  failures += guia_logos_retry_check("own logo", "http://logos.test/espn.png",
      "http://logos.test/espn-hd.png", texture, retryRow, channel("ESPN FHD"));
  failures += guia_logos_retry_check("variant donor", "http://logos.test/espn.png",
      "http://logos.test/espn-hd.png", texture, retryRow, channel("ESPN SD"));
  failures += guia_logos_retry_check("M3U donor", "http://m3u.test/canal.png",
      "http://m3u.test/second.png", texture, retryRow, channel("By ID"));
  failures += guia_logos_retry_check("XMLTV initials", "http://xmltv.test/globo.png?a=1&b=2",
      "", texture, retryRow, channel("Globo RJ"));
#else
  check("ESPN FHD", "http://logos.test/espn.png");
  check("ESPN SD", "http://logos.test/espn.png");
  check("Globo RJ", "http://xmltv.test/globo.png?a=1&b=2");
  check("Canal M3U HD", "http://m3u.test/canal.png");
  check("No Logo", "");
  check("Whitespace", "http://logos.test/espn.png");
  check("Invalid", "http://xmltv.test/globo.png?a=1&b=2");
  // Donors are selected before downloads; confirmed failures must advance
  // without republishing, while pending first donors retain priority.
  failed = "http://logos.test/espn.png";
  check("ESPN SD", "http://logos.test/espn-hd.png");
  check("ESPN FHD", "http://logos.test/espn-hd.png");
  failed2 = "http://m3u.test/canal.png";
  capture(argv[2]);
  failed2 = "http://logos.test/espn-hd.png";
  check("ESPN SD", ""); failed = failed2 = NULL;
  failed = "http://m3u.test/canal.png";
  check("Canal M3U HD", "http://m3u.test/second.png");
  check("By ID", "http://m3u.test/second.png");
  check("Canal M3U FHD", "http://m3u.test/second.png");
  failed2 = "http://m3u.test/second.png";
  check("Canal M3U HD", "http://xmltv.test/lower-priority.png");
  failed = failed2 = NULL;
  // Confirmed decode/HTTP failures advance priority; pending images do not.
  GCanal *c = channel("Canal M3U HD"); const char *original = c->logo;
  c->logo = "http://logos.test/broken.png";
  check("Canal M3U HD", c->logo);
  failed = c->logo; check("Canal M3U HD", "http://m3u.test/canal.png");
  c->logo = original; failed = "http://m3u.test/canal.png";
  check("Canal M3U HD", "http://m3u.test/second.png"); failed = NULL;
  // XMLTV outranks another variant; failed XMLTV falls through to it.
  GCanal *variant = channel("ESPN SD"); int previous = variant->epg;
  variant->epg = channel("Globo RJ")->epg;
  check("ESPN SD", "http://xmltv.test/globo.png?a=1&b=2");
  failed = "http://xmltv.test/globo.png?a=1&b=2";
  check("ESPN SD", "http://logos.test/espn.png"); failed = NULL; variant->epg = previous;
  // Repack after donor removal must not retain/cache borrowed metadata.
  assert(!c->logo[0]); channel("Canal M3U")->logo = ""; empacotar();
  check("Canal M3U HD", "http://m3u.test/second.png");
  channel("Canal M3U SD")->logo = ""; empacotar();
  check("Canal M3U HD", "http://xmltv.test/lower-priority.png");
#endif
  glDeleteTextures(1, &texture); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  return failures ? 1 : 0;
}
