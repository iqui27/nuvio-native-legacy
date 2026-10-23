// Shared TV renderer, offline: actual stage, safe source line, honest buffer.
#include "abrecine.h"
#include "catalogo.h"
#include "streams.h"
#include "video.h"
#include "gfx.h"
#include "text.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define NV_TELA_W 1920.0f
#define NV_TELA_H 1080.0f
static AbreCine cine = { .viva = 1, .veu = 1, .foco = 1, .desde = 1000 };
static int esperandoFonte, tentativaN, tentativaM, reduz, addonLigado;
static int fundoOp = 2;   // #508: 0 off, 1 sharp + veil (the default, A3), 2 blurred
static char linhaEp[192] = "S01E03 · The Head";
static Stream fonte;
static VideoStats stats;
static char linhas[16][300];
static int n, blur, logo, fills, arte, logoFills, veus, aneis, brancos;
static float fill;
static GfxModo arteModo;
static GfxRect rLogo, rVeu, rAnel, rArco, rBranco;
// A3: what the title page leaves for the morph (player.c, player_morph_de).
static GfxRect morphLogo, morphPlay;
static Uint32 morphEm;
float gfx_tex_aspect_atual;
int ajustes_animacoes_reduzidas(void) { return reduz; }
int player_carregando(void) { return 1; }
int ajustes_abertura_addon(void) { return addonLigado; }
int ajustes_abertura_fundo(void) { return fundoOp; }
static int estiloOp;   // 0 Centro (A3), 1 Canto (B3)
int ajustes_abertura_estilo(void) { return estiloOp; }
static int logoPronto;
static void corFocoPlayer(float *r, float *g, float *b) { *r = .8f; *g = .5f; *b = .2f; }
int corviva_logo_acento(float *r, float *g, float *b) {
  if (!logoPronto) return 0;
  *r = .1f; *g = .6f; *b = .9f; return 1;
}
const char *i18n(const char *s) { return s; }
const Stream *stream_item(int idx) { (void)idx; return &fonte; }
int stream_atual(void) { return 0; }
void video_estatisticas(VideoStats *s) { *s = stats; }
const char *artehero_url(const CatItem *c) { return c->backdrop; }
// This renderer fixture supplies local artwork; ownership/copying uses the
// real detail and player modules in player_loading_artwork.sh.
static int cinemaPoster;
static const char *cinemaArteObter(const CatItem *c) { return artehero_url(c); }
const char *artehero_logo_sessao(const CatItem *c) { return c->logo; }
GLuint tex_obter_hero(const char *s) { return *s ? 1 : 0; }
GLuint tex_obter_larg(const char *s, float w) { (void)w; return *s ? 1 : 0; }
GLuint tex_obter_larg_qualquer(const char *s, float w) { (void)w; return *s ? 2 : 0; }
float tex_aspecto(const char *s) { (void)s; return 2; }
int tex_marca_escura(const char *s) { (void)s; return 0; }
GLuint gfx_desfocado(GLuint t, const char *s) { (void)s; blur++; return t; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float px, float py, float rad,
              float cr, float cg, float cb, float ca) {
  if (t == 2) { logo++; rLogo = r; }
  if (t == 1 && r.w > 1900.0f) { arte++; arteModo = m; }
  if (!t && m == GFX_VEU_BAIXO) { veus++; rVeu = r; }
}
void gfx_cor(GfxRect r, float rad, float cr, float cg, float cb, float ca) {
  if (rad == .5f && cr == 1 && cg == 1 && cb == 1 && ca > 0) { brancos++; rBranco = r; }
}
void gfx_anel(GfxRect r, float rad, float esp, float cr, float cg, float cb, float ca) { aneis++; rAnel = r; }
void gfx_arco(GfxRect r, float esp, float ini, float frac, float cr, float cg, float cb, float ca) {
  rArco = r;
  if (cr == .8f) { fills++; fill = frac; }
  if (cr == .1f && cg == .6f && cb == .9f) { logoFills++; fill = frac; }
}
TxtLinha txt_linha(TxtEstilo es, const char *s, int r, int g, int b, int a) {
  assert(n < 16); snprintf(linhas[n++], sizeof linhas[0], "%s", s);
  return (TxtLinha){ .w = (int)strlen(s) * 12, .h = 24 };
}
TxtLinha txt_linha_corta(TxtEstilo es, const char *s, int r, int g, int b, int a, float w) {
  return txt_linha(es, s, r, g, b, a);
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) {}
#include "render.c"
static int tem(const char *s) {
  for (int i = 0; i < n; i++) if (strstr(linhas[i], s)) return 1;
  return 0;
}
static void quadro(CatItem *c) {
  n = blur = logo = fills = logoFills = arte = 0; fill = 0;
  desenharCinema(2600, c, 0);
  assert(blur == 1);
  assert(tem("S01E03"));
  // The bar says "loading": no stage text, no percent, with the toggle on or off.
  assert(!tem("Abrindo fonte") && !tem("Carregando") && !tem("%"));
  assert(addonLigado ? tem("AIOStreams") : !tem("AIOStreams"));
  assert(!tem("RAW") && !tem("secret") && !tem("http"));
}
int main(void) {
  CatItem c = {0};
  snprintf(c.backdrop, sizeof c.backdrop, "local-backdrop");
  snprintf(c.logo, sizeof c.logo, "local-logo");
  snprintf(c.titulo, sizeof c.titulo, "Fallout");
  snprintf(fonte.provedor, sizeof fonte.provedor, "AIOStreams");
  snprintf(fonte.rotulo, sizeof fonte.rotulo, "RAW.FILE\nhttps://example.invalid/?secret=1");
  snprintf(fonte.url, sizeof fonte.url, "https://example.invalid/?secret=2");
  video_stats_vazio(&stats);
  for (addonLigado = 0; addonLigado <= 1; addonLigado++) {
    reduz = 0; stats = (VideoStats){0}; video_stats_vazio(&stats);
    tentativaN = tentativaM = 0;
    esperandoFonte = 1;
    quadro(&c); assert(logo == 1 && fills == 1 && fabsf(fill - .25f) < .01f);
    esperandoFonte = 0;
    quadro(&c);
    stats.bufferPct = 0; reduz = 1;
    quadro(&c); assert(fills == 0);
    stats.bufferPct = 100;
    quadro(&c); assert(fills == 1 && fabsf(fill - 1) < .01f);
    stats.bufferPct = 37;
    quadro(&c); assert(fills == 1 && fabsf(fill - .37f) < .01f);
    esperandoFonte = 1; reduz = 1;
    quadro(&c); assert(fills == 0);
    reduz = 0;
    esperandoFonte = 0; stats.bufferPct = -1;
    stats.bufferSeg = 5; stats.bufferAlvoSeg = 20;
    quadro(&c);
    stats.bufferPct = 101;
    quadro(&c);
    tentativaN = 2; tentativaM = 3;
    c.logo[0] = 0;
    n = logo = 0; desenharCinema(2600, &c, 0);
    assert(logo == 0 && tem("Fallout") && tem("Fonte 2 de 3") == addonLigado);
    snprintf(fonte.provedor, sizeof fonte.provedor, "AIOStreams\n4K\r\t");
    n = 0; desenharCinema(2600, &c, 0);
    assert(!tem("\n") && !tem("\r") && !tem("\t"));
    snprintf(fonte.provedor, sizeof fonte.provedor, "AIOStreams");
    snprintf(c.logo, sizeof c.logo, "local-logo");
  }
  // The bar takes the logo colour (as the Immersive style), not the focus accent.
  logoPronto = 1; esperandoFonte = 0; stats.bufferPct = 50; fills = logoFills = 0;
  desenharCinema(2600, &c, 0);
  assert(logoFills == 1 && fills == 0 && fabsf(fill - .5f) < .01f);
  stats.bufferPct = -1; reduz = 0; fills = logoFills = 0;
  desenharCinema(2000, &c, 0);   // indeterminate sweep is logo-coloured too
  assert(logoFills == 1 && fills == 0);
  logoPronto = 0; stats.bufferPct = 50; fills = logoFills = 0;
  desenharCinema(2600, &c, 0);   // no palette yet: the accent
  assert(fills == 1 && logoFills == 0);
  addonLigado = 1;
  fonte.provedor[0] = fonte.rotulo[0] = linhaEp[0] = 0;
  n = 0; desenharCinema(2600, &c, 0);
  assert(!tem("S01E03") && !tem("secret"));
  // #508: the backdrop on the opening screen is a choice — off, sharp, blurred.
  // Off draws no art at all; sharp draws the backdrop without the blur copy;
  // blurred (the default) is the receded art of always, blur copy and all.
  fundoOp = 0; n = blur = arte = 0; desenharCinema(2600, &c, 0);
  assert(blur == 0 && arte == 0);
  fundoOp = 1; n = blur = arte = 0; desenharCinema(2600, &c, 0);
  assert(blur == 0 && arte == 1);
  fundoOp = 2; n = blur = arte = 0; desenharCinema(2600, &c, 0);
  assert(blur == 1 && arte == 1);
  // A3 CONTRACT (dono, 16/10): with Arte, the backdrop is SHARP (no blur copy,
  // plain GFX_CARD), a dark veil rises from the bottom to HALF the screen, and
  // the Play becomes a SMALL ring BELOW the logo, centred, not around it.
  fundoOp = 1; n = blur = arte = veus = aneis = logo = 0; desenharCinema(2600, &c, 0);
  assert(blur == 0 && arte == 1 && arteModo == GFX_CARD);
  assert(veus == 1 && rVeu.y == 540.0f && rVeu.h == 540.0f && rVeu.w == 1920.0f);
  assert(logo == 1 && aneis == 1 && rAnel.w == rAnel.h && rAnel.w <= 64.0f);
  assert(rAnel.y >= rLogo.y + rLogo.h + 32.0f);
  assert(fabsf(rAnel.x + rAnel.w * .5f - 960.0f) < .5f && fabsf(rLogo.x + rLogo.w * .5f - 960.0f) < .5f);
  assert(rArco.x == rAnel.x && rArco.y == rAnel.y && rArco.w == rAnel.w);
  // Off: no art and no veil. Blurred: no veil (the blur copy is already dark).
  fundoOp = 0; veus = 0; desenharCinema(2600, &c, 0); assert(veus == 0);
  fundoOp = 2; veus = 0; desenharCinema(2600, &c, 0); assert(veus == 0);
  // MORPH from the title page: logo and Play pill seen just before the opening.
  fundoOp = 1; reduz = 0; stats.bufferPct = -1; esperandoFonte = 1;
  morphLogo = (GfxRect){ 96, 368, 500, 200 }; morphPlay = (GfxRect){ 96, 600, 321, 94 };
  cine.desde = 20000; morphEm = 19950;
  brancos = 0; desenharCinema(20000 + 150, &c, 0);
  assert(brancos == 1 && rBranco.x == rAnel.x && rBranco.w == rAnel.w);   // the white pill is still there
  assert(rAnel.w < 321.0f && rAnel.w > 56.0f && rAnel.x > 96.0f && rAnel.x < 932.0f);
  assert(rLogo.x > 96.0f && rLogo.x + rLogo.w * .5f < 960.0f);
  brancos = logoFills = fills = 0; desenharCinema(20000 + 1500, &c, 0);
  assert(brancos == 0 && rAnel.x == 932.0f && rAnel.y == 756.0f && rAnel.w == 56.0f);
  assert(fabsf(rLogo.x + rLogo.w * .5f - 960.0f) < .5f && fills + logoFills == 1);
  // An opening with no title page on screen just before it fades in place.
  cine.desde = 40000; brancos = 0; desenharCinema(40000 + 150, &c, 0);
  assert(brancos == 0 && rAnel.x == 932.0f);
  // B3 CONTRACT (Canto): same sharp art + half veil, but the logo sits at the
  // BOTTOM LEFT and the small ring is directly BELOW it, left-aligned. No glass
  // island: the only extra draws are the ring parts and the logo. Same code path.
  estiloOp = 1; fundoOp = 1; reduz = 0; stats.bufferPct = -1; esperandoFonte = 1;
  cine.desde = 60000; morphEm = 0;
  n = blur = arte = veus = aneis = logo = 0; desenharCinema(60000 + 2000, &c, 0);
  assert(blur == 0 && arte == 1 && arteModo == GFX_CARD && veus == 1 && rVeu.y == 540.0f && rVeu.h == 540.0f);
  assert(logo == 1 && aneis == 1 && rAnel.w == rAnel.h && rAnel.w == 44.0f && rAnel.x == 96.0f && rAnel.y == 940.0f);
  assert(rLogo.x == 96.0f && rLogo.x + rLogo.w * .5f < 480.0f && rLogo.y > 540.0f);   // bottom-left quadrant
  assert(rAnel.y >= rLogo.y + rLogo.h + 32.0f && rAnel.x == rLogo.x);                  // ring under the logo
  assert(rArco.x == rAnel.x && rArco.y == rAnel.y && rArco.w == rAnel.w);
  // MORPH lands on the corner targets, from the same title-page rects as A3.
  morphLogo = (GfxRect){ 96, 368, 500, 200 }; morphPlay = (GfxRect){ 96, 600, 321, 94 };
  cine.desde = 70000; morphEm = 69950;
  brancos = 0; desenharCinema(70000 + 150, &c, 0);
  assert(brancos == 1 && rAnel.w < 321.0f && rAnel.w > 44.0f);   // pill still shrinking
  brancos = 0; desenharCinema(70000 + 1500, &c, 0);
  assert(brancos == 0 && rAnel.x == 96.0f && rAnel.y == 940.0f && rAnel.w == 44.0f && rLogo.x == 96.0f);
  estiloOp = 0;
  fundoOp = 2;
  puts("PASS: S5 shared renderer: backdrop, logo/title, episode, no stage text, addon line only with toggle, real buffer only, backdrop option off/sharp/blurred, A3 ring below logo + B3 corner layout + sharp art + half veil + morph");
}
