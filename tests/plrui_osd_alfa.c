// Exercise real plrui helpers; record draw calls without a GPU/video plane.
#include "plrui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static float op = 0.78f, px[3], alfa, fill[3], glass_density;
static int glass, rims, panels, materials;
static GfxRect bounds;
void gfx_vidro_folha(GfxRect r, float raio, float a) { panels++; alfa = a; }
int ajustes_vidro(void) { return glass; }
float ajustes_vidro_opacidade(void) { return op; }
void ajustes_acento(float *r, float *g, float *b) { *r = .8f; *g = .3f; *b = .1f; }
int ajustes_tinta_foco(void) { return 0; }
const char *i18n(const char *s) { return s; }
int txt_largura(TxtEstilo estilo, const char *s) { return 80; }
TxtLinha txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  return (TxtLinha){ .w = 80, .h = 20 };
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) {}
void gfx_icone(GfxRect r, const char *nome, float cr, float cg, float cb, float ca) {}
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco,
              float parx, float pary, float raio, float cr, float cg, float cb, float ca) {
  if (modo != GFX_SOMBRA &&
      (!bounds.w || (r.x >= bounds.x && r.y >= bounds.y && r.x + r.w <= bounds.x + bounds.w &&
                     r.y + r.h <= bounds.y + bounds.h))) {
    fill[0] = cr; fill[1] = cg; fill[2] = cb; alfa = ca;
    px[0] += (cr - px[0]) * ca; px[1] += (cg - px[1]) * ca; px[2] += (cb - px[2]) * ca;
  }
}
void gfx_vidro_painel(GfxRect r, float raio, float fundo, float a) {
  panels++; glass_density = fundo; alfa = a;
}
void gfx_vidro_material(GfxRect r, float raio, float dens, float a) {
  materials++; glass_density = dens; alfa = a;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  if (bounds.w > 0.0f && (r.x < bounds.x || r.y < bounds.y || r.x + r.w > bounds.x + bounds.w ||
      r.y + r.h > bounds.y + bounds.h)) return;
  px[0] += (cr - px[0]) * ca; px[1] += (cg - px[1]) * ca; px[2] += (cb - px[2]) * ca;
  fill[0] = cr; fill[1] = cg; fill[2] = cb; alfa = ca;
}
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  rims++; // Rim does not cover the centre pixel.
}
void gfx_sombra_sob(GfxRect s, float foco, float parx, float raio, float cr, float cg, float cb,
                    float ca, GfxRect clip, float raioClip, float a) {}
static void check(int ok, const char *message) {
  if (!ok) { printf("FAIL: %s (alfa=%.4f fill %.3f %.3f %.3f)\n", message, alfa, fill[0], fill[1], fill[2]); exit(1); }
}
static int near(float a, float b) { return fabsf(a - b) < .0001f; }
static void reset(void) {
  px[0] = .62f; px[1] = .66f; px[2] = .74f;
  alfa = -1; rims = panels = materials = 0;
  bounds = (GfxRect){0};
}
static void solid(float a) {
  check(near(alfa, a) && near(fill[0], .141f) && near(fill[1], .149f) && near(fill[2], .173f),
        "shared idle button/hint lost solid #24262C surface");
  check(!rims, "shared surface gained a rim");
}
static void tint(float a) {
  check(near(alfa, (.30f + .45f * op) * a), "video tint ignores opacity/fade");
  check(px[1] > .10f, "video surface opaque over bright frame");
}
// A video button is a translucent island surface even settled on focus: the
// accent survives (same colour as the shared pill), the fill does not.
static void accentVideo(float a) {
  check(near(alfa, (.30f + .45f * op) * a), "focused video button ignores glass opacity");
  check(near(fill[0], .8f) && near(fill[1], .3f) && near(fill[2], .1f),
        "focused video button lost accent");
  check(px[1] > .10f, "focused video button opaque over bright frame");
}
int main(void) {
  GfxRect pill = { 100, 100, 120, 60 };
  const char *keys[] = { "OK" }, *labels[] = { "Play" };
  const float opacity[] = { .60f, .78f, .92f };
  const float fade[] = { 1, .4f };
  for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) {
    op = opacity[i]; float a = fade[j];
    reset(); plrui_botao_repouso(pill, a); solid(a);
    reset(); plrui_botao(100, 100, "Send", NULL, 0, a); solid(a);
    reset(); plrui_dicas(keys, labels, 1, 100, 100, 0, a); solid(a);
    reset(); plrui_disco_osd(pill, a); tint(a); check(rims == 1, "OSD rim missing");
    reset(); plrui_botao_repouso_video(pill, a); tint(a); check(rims == 1, "video pill rim missing");
    reset(); plrui_botao_video(100, 100, "Back", NULL, 0, a); tint(a);
    reset(); plrui_botao_video(100, 100, "Back", NULL, .25f, a); tint(a * .75f);
    reset(); plrui_dicas_video(keys, labels, 1, 100, 100, 0, a); tint(a);
    check(!rims, "hint chip gained a rim");
    reset(); bounds = (GfxRect){ 100, 100, plrui_botao_largura("Play", NULL), 60 };
    plrui_botao_video(100, 100, "Play", NULL, 1, a); accentVideo(a);
    reset(); bounds = (GfxRect){ 10, 10, 1180, 308 };
    plrui_material_video(bounds, 36.0f, a);
    { GfxRect idle = { 480, 232, 136, 60 };
      bounds = idle; plrui_botao_repouso_video(idle, a); tint(a);
      check(rims == 1, "idle button lost rim inside translucent island"); }
    bounds = (GfxRect){ 10, 10, 1180, 308 }; reset();
    plrui_material_video(bounds, 36.0f, a);
    { GfxRect foco = { 480, 232, 136, 60 };
      bounds = foco; plrui_botao_repouso_video(foco, a); tint(a);
      reset(); bounds = foco; plrui_pilula_foco_video(foco, a); accentVideo(a); }
    bounds = (GfxRect){0};
  }
  glass = 1;
  reset(); plrui_botao_repouso(pill, .4f);
  check(panels == 1 && near(glass_density, .55f) && near(alfa, .4f), "shared glass panel changed");
  reset(); plrui_botao_repouso_video(pill, .4f);
  check(panels == 1 && near(glass_density, .55f) && near(alfa, .4f), "video glass panel changed");
  reset(); plrui_botao_video(100, 100, "Back", NULL, 0, .4f);
  check(panels == 1 && near(alfa, .4f), "video button glass dispatch changed");
  reset(); plrui_disco_osd(pill, .4f);
  check(materials == 1 && near(glass_density, .52f) && near(alfa, .4f), "OSD glass material changed");
  for (int video = 0; video < 2; video++) {
    reset();
    if (video) plrui_dicas_video(keys, labels, 1, 100, 100, 0, .4f);
    else plrui_dicas(keys, labels, 1, 100, 100, 0, .4f);
    check(near(alfa, .09f * .4f) && near(fill[0], 1), "glass hint changed");
    reset();
    if (video) plrui_botao_video(100, 100, "Play", NULL, 1, .4f);
    else plrui_botao(100, 100, "Play", NULL, 1, .4f);
    check(!panels && near(alfa, .4f) && near(fill[0], .8f), "glass focus lost accent");
  }
  puts("plrui_osd_alfa: PASS (shared/video, opacity/fade, glass, accent)");
  return 0;
}
