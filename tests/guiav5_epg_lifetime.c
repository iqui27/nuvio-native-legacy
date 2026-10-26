// Real EPG replacement between cached snapshot and text rendering; no GL or network.
#include "stub_perfisessao.h"
#include "../src/guia.c"
#include "../src/guiav5ui.c"
#include <assert.h>
/* guiav5ui.c routes text through its scale wrappers; stub the real calls. */
#undef txt_linha
#undef txt_linha_bruta
#undef txt_linha_corta
#undef txt_linha_corta_bruta
#undef txt_desenhar
#undef txt_bloco
#undef txt_bloco_bruto

// Read text at the renderer boundary, without a GL context.
static Uint32 ticks = 100;
static char rendered[256];
static int translations;
Uint32 SDL_GetTicks(void) { return ticks; }
int ajustes_idioma(void) { return IDIOMA_EN; }
int ajustes_epg_memoria(void) { return 0; }
const char *i18n(const char *s) {
  translations++;
  // strlen also exposes freed EPG storage in the original translated path.
  volatile size_t n = strlen(s); (void)n;
  return !strcmp(s, "Favoritos") ? "Favorites" : s;
}
TxtLinha txt_linha_corta_bruta(TxtEstilo st, const char *s, int r, int g, int b, int a, float w) {
  (void)st; (void)r; (void)g; (void)b; (void)a; (void)w;
  snprintf(rendered, sizeof rendered, "%s", s);
  return (TxtLinha){0};
}
TxtLinha txt_linha_corta(TxtEstilo st, const char *s, int r, int g, int b, int a, float w) {
  return txt_linha_corta_bruta(st, i18n(s), r, g, b, a, w);
}
void txt_desenhar(TxtLinha l, float x, float y) { (void)l; (void)x; (void)y; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float a) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)a;
}
int tex_falhou(const char *url) { (void)url; return 0; }
float gfx_escala_txt(void) { return 1; }
float gfx_escala_txt_entrar(void) { return 1; }
void gfx_escala_sair(float a) { (void)a; }
GLuint tex_obter_larg(const char *url, float w) { (void)url; (void)w; return 0; }
int perfis_ativo(void) { return 1; }
const char *login_uid(void) { return ""; }
char *dados_ler(const char *name) { (void)name; return NULL; }
int dados_gravar(const char *name, const char *value) { (void)name; (void)value; return 1; }
int xtream_e_id(const char *id) { (void)id; return 0; }
void xtepg_querer(const char *id) { (void)id; }
int xtepg_agora(const char *id,time_t t,EpgProg *p) { (void)id;(void)t;(void)p;return 0; }
int xtepg_proximo(const char *id,time_t t,int k,EpgProg *p) { (void)id;(void)t;(void)k;(void)p;return 0; }
int xtepg_faixa(const char *id,time_t de,time_t ate,EpgProg *o,int cap) { (void)id;(void)de;(void)ate;(void)o;(void)cap;return 0; }

static void schedule(const char *title) {
  char xml[4096], ini[32], fim[32];
  time_t start = time(NULL) - 600;
  struct tm tm;
  size_t used = (size_t)snprintf(xml, sizeof xml,
    "<tv><channel id=\"fixture\"><display-name>Fixture</display-name></channel>");
  for (int i = 0; i < 6; i++) {
    time_t stop = start + 3600;
    gmtime_r(&start, &tm); strftime(ini, sizeof ini, "%Y%m%d%H%M%S +0000", &tm);
    gmtime_r(&stop, &tm); strftime(fim, sizeof fim, "%Y%m%d%H%M%S +0000", &tm);
    used += (size_t)snprintf(xml + used, sizeof xml - used,
      "<programme channel=\"fixture\" start=\"%s\" stop=\"%s\"><title>%s</title></programme>",
      ini, fim, title);
    start = stop;
  }
  snprintf(xml + used, sizeof xml - used, "</tv>");
  assert(epg_xml_processar(xml) == 6);
}

int main(void) {
  schedule("Favoritos"); // A real key used as provider text must stay verbatim.
  GCanal channel = {.id="fixture", .nome="Fixture", .logo="", .base="", .epgId="fixture",
    .tvgNome="", .cat=-1, .fonte=-1, .epg=epg_match_id("fixture")};
  assert(channel.epg >= 0);
  canais = &channel; nCanais = 1; favLido = 1;
  snprintf(screen, sizeof screen, "grade");
  gv_snapshot();
  assert(gv_n == 1 && gv_ch[0].np == 6);
  EpgProg *cached = gv_prog(&gv_ch[0]);
  assert(cached);
  schedule("Replacement programme"); // Frees the original EPG arena.
  gv_snapshot(); // Still inside the two-second cache window.
  gv_label(TXT_ILHA_TITULO, cached->titulo, 0, 0, 500, 0);
  assert(!strcmp(rendered, "Favoritos"));
  assert(translations == 0);
  assert(!strcmp(cached->titulo, "Favoritos"));
  for (int i = 0; i < 6; i++) {
    gv_label(TXT_ILHA_CORPO, gv_ch[0].p[i].titulo, 0, 0, 500, 0);
    assert(!strcmp(rendered, "Favoritos"));
  }
  label(TXT_ILHA_NOME, "Favoritos", 0, 0, 500, 0);
  assert(!strcmp(rendered, "Favoritos") && translations == 0);
  guia_etiqueta("Favoritos", 0, 0, 500, 1);
  assert(!strcmp(rendered, "Favoritos") && translations == 0);
  gv_sheet_head(0, 0, "Favoritos", "Favoritos");
  assert(!strcmp(rendered, "Favoritos") && translations == 0);
  GCat category = {.nome="Favoritos", .fonte=-1};
  int visible = 0;
  cats = &category; vis = &visible; nVis = 1;
  assert(!strcmp(linhaNome(0), "Favoritos") && translations == 0);
  cats = NULL; vis = NULL; nVis = 0;
  gv_label(TXT_ILHA_NOME, i18n("Favoritos"), 0, 0, 500, 0);
  assert(!strcmp(rendered, "Favorites") && translations == 1);
  ticks += 2000; // Expire the cache without leaving the guide.
  channel.epg = epg_match_id("fixture");
  gv_snapshot();
  for (int i = 0; i < 6; i++)
    assert(!strcmp(gv_ch[0].p[i].titulo, "Replacement programme"));
  // XMLTV accepts 239-byte titles; snapshot ownership must preserve them.
  char long_title[240]; memset(long_title, 'x', sizeof long_title - 1); long_title[239] = 0;
  schedule(long_title);
  ticks += 2000;
  channel.epg = epg_match_id("fixture");
  gv_snapshot();
  assert(!strcmp(gv_prog(&gv_ch[0])->titulo, long_title));
  epg_teste_limpar();
  gv_label(TXT_ILHA_TITULO, gv_prog(&gv_ch[0])->titulo, 0, 0, 500, 0);
  assert(!strcmp(rendered, long_title));
  canais = NULL; nCanais = 0;
  puts("PASS: cached guide titles survive EPG replacement and refresh");
  return 0;
}
