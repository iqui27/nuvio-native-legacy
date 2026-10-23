// Apple TV (Dinamica) sidebar: x1.2 at UI scale <= 90%, unchanged from 100%;
// Streaming opens past Settings with either D-pad focus or a mobile swipe.
#include "../src/menu.c"
#include <assert.h>
#include <stdio.h>

static int layoutTeste = HOME_LAYOUT_DINAMICA;
int gfx_vidro_camada;
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
int ajustes_home_layout(void) { return layoutTeste; }
int ajustes_menu_explorar(void) { return 1; }
int ajustes_menu_guia(void) { return 1; }
int ajustes_menu_agenda(void) { return 1; }
int ajustes_menu_perfil(void) { return 1; }
int ajustes_rail_recolhida(void) { return 0; }
int ajustes_menu_topo(void) { return 0; }
static float escalaTeste = 1.0f;
float gfx_escala_ui(void) { return escalaTeste; }
float gfx_escala_txt(void) { return 1.0f; }
float gfx_escala_ui_base(void) { return escalaTeste; }
static int pastasTeste = 3;
int home_streaming_barra(const int **p) { static const int v[3] = {4, 5, 6}; if (p) *p = v; return pastasTeste; }
#ifdef NV_TOUCH_UI
static int mobileTeste;
float nv_layout_w = 1920.0f, nv_layout_h = 1080.0f;
int layout_modo_mobile(void) { return mobileTeste; }
#endif

static float painelReal(float ui) { escalaTeste = ui; return tvPainel().w * menuEscala(); }
static void tecla(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; menu_evento(&e);
}
static int visivel(void) { float ys[NV_MENU_FOCOS_TV]; tvLayout(ys, NULL); return ys[MENU_ST0] >= 0.0f; }

int main(void) {
  float base = painelReal(1.0f);
  assert(fabsf(base - 355.0f * 0.9f) < 0.01f);
  assert(fabsf(painelReal(1.1f) - base) < 0.01f);
  assert(fabsf(painelReal(0.9f) - base * 0.9f * 1.2f) < 0.01f);
  assert(fabsf(painelReal(0.8f) - base * 0.8f * 1.2f) < 0.01f);
  layoutTeste = HOME_LAYOUT_PADRAO;
  assert(fabsf(menuEscala() - 0.9f * 0.8f) < 0.001f);
  layoutTeste = HOME_LAYOUT_DINAMICA;
  escalaTeste = 1.0f;

  // "Scroll down for streaming" ends the sidebar, right under Settings, only
  // while Streaming is collapsed and there is something to open.
  { float ys[NV_MENU_FOCOS_TV];
    aberto = 1; linha = MENU_AJUSTES; tvLayout(ys, NULL);
    assert(tvDicaY >= 0.0f && fabsf(tvDicaY - (ys[MENU_AJUSTES] + TV_LINHA_H)) < 0.01f);
    linha = MENU_ST0; tvLayout(ys, NULL);
    assert(tvDicaY < 0.0f);
    pastasTeste = 0; linha = MENU_AJUSTES; tvLayout(ys, NULL);
    assert(tvDicaY < 0.0f);
    pastasTeste = 3; }
  aberto = 1; linha = MENU_AJUSTES;
  assert(!visivel());
  tecla(SDLK_DOWN);
  assert(linha == MENU_ST0 && visivel());
  tecla(SDLK_DOWN);
  assert(linha == MENU_ST0 + 1 && visivel());
  tecla(SDLK_UP); tecla(SDLK_UP);
  assert(linha == MENU_AJUSTES && !visivel());
  tecla(SDLK_DOWN); menu_fechar();
  assert(!visivel());
#ifdef NV_TOUCH_UI
  // Swipe from an ordinary destination; touch scrolling must not need focus
  // on Settings. Cover both a fitting panel and one that already scrolls.
  for (int small = 0; small < 2; small++) {
    float ys[NV_MENU_FOCOS_TV];
    nv_layout_h = small ? 500.0f : 1080.0f;
    menu_abrir();
    GfxRect panel = tvPainel();
    tvToqueEscala = menuEscala();
    tvToqueRegiao = (GfxRect){panel.x * tvToqueEscala, panel.y * tvToqueEscala,
                             panel.w * tvToqueEscala, panel.h * tvToqueEscala};
    tvToqueMaximo = fmaxf(0.0f, tvLayout(ys, NULL) - panel.h);
    tvRolar = 0.0f;
    PonteiroRolagem e = {PONT_ROL_INICIO, 1, 0, 0,
                        tvToqueRegiao.x + 10, tvToqueRegiao.y + 10};
    mobileTeste = 0;
    assert(!tvToqueRolar(&e) && !visivel());
    mobileTeste = 1;
    e.eixoY = 0;
    assert(!tvToqueRolar(&e) && !visivel());
    e.eixoY = 1;
    e.x = tvToqueRegiao.x - 1;
    assert(!tvToqueRolar(&e) && !visivel());
    e.x = tvToqueRegiao.x + 10;
    assert(tvToqueRolar(&e));
    e.fase = PONT_ROL_MOVER;
    e.delta = 20;
    tvToqueRolar(&e);
    assert(!visivel());
    if (small) {
      e.delta = -20;
      assert(tvToqueRolar(&e) && !visivel());
    }
    e.delta = -1000;
    assert(tvToqueRolar(&e));
    assert(linha == MENU_ST0 && visivel());
    tvLayout(ys, NULL);
    assert(ys[MENU_ST0] - tvRolar < tvPainel().h);
    assert(ys[MENU_ST0] + TV_LINHA_H - tvRolar > 0);
    // Use the same focus callback and confirmation as a service tap.
    ponteiroLinha(MENU_ST0 + 1, 0);
    tecla(SDLK_RETURN);
    assert(menu_pediu_colecao() == 5 && !aberto && !visivel());
    menu_abrir();
    assert(!visivel());
    pastasTeste = 0;
    e.fase = PONT_ROL_INICIO;
    assert(tvToqueRolar(&e));
    e.fase = PONT_ROL_MOVER;
    tvToqueRolar(&e);
    assert(linha < MENU_ST0 && !visivel());
    pastasTeste = 3;
    menu_fechar();
  }
#endif
  puts("PASS: Apple TV scale, D-pad collapse/expand, mobile swipe and service selection");
  return 0;
}
