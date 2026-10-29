// #388: a foto do perfil no rodape do menu mantem a proporcao dela (cover),
// em vez de ser esticada num quadrado.
#include "../src/menu.c"
#include <assert.h>
#include <stdio.h>

float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
int ajustes_home_layout(void) { return HOME_LAYOUT_MODERNA; }
int ajustes_menu_explorar(void) { return 1; }
int ajustes_menu_guia(void) { return 1; }
int ajustes_menu_agenda(void) { return 1; }
int ajustes_menu_perfil(void) { return 1; }
int ajustes_rail_recolhida(void) { return 0; }
float gfx_escala_ui(void) { return 1.0f; }

static ContaPerfil conta;
static float aspectoNoDesenho = -1.0f;
static float aspectoFoto;
static GfxModo modoNoDesenho;
const ContaPerfil *perfis_item_ativo(void) { return &conta; }
GLuint tex_obter_larg(const char *c, float l) { (void)c; (void)l; return 7; }
float tex_aspecto(const char *c) { (void)c; return aspectoFoto; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo m, float foco, float px, float py,
              float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)foco; (void)px; (void)py; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
  if (tex == 7) {
    aspectoNoDesenho = gfx_tex_aspect_atual;
    modoNoDesenho = m;
  }
}

int main(void) {
  MenuGeo g = geoEm(0, 1);
  const float aspectos[] = { 16.0f / 9.0f, 9.0f / 16.0f };
  snprintf(conta.avatarUrl, sizeof conta.avatarUrl, "foto-retangular");
  for (unsigned i = 0; i < sizeof aspectos / sizeof aspectos[0]; i++) {
    aspectoFoto = aspectos[i];
    desenhaRodape(&g, 0, 1.0f, 0, 0);
    assert(aspectoNoDesenho == aspectoFoto);
    assert(modoNoDesenho == GFX_AVATAR);
    assert(gfx_tex_aspect_atual == 0.0f);
  }
  puts("PASS #388: avatar largo e vertical usa cover com mascara circular");
  return 0;
}
