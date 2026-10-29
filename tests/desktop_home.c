// Real Home geometry at the approved Mac size; no GL, catalog, or network.
#include "../src/layout.c"
#include "../src/home.c"
#include <assert.h>
float gfx_escala_tela_txt(void) { return 1; }
float gfx_escala(void) { return 1; }
float gfx_poster_k(void) { return 1; }
int ajustes_hero_ligado(void) { return 1; }
float ajustes_conteudo_x(void) { return NV_DESKTOP_SIDEBAR_W + 60; }
int main(void) {
  layout_janela_definir(1280, 800);
  layout_tela_definir(2560, 1600);
  assert(layoutHome() == HOME_LAYOUT_DINAMICA);
  focoHero = 1;
  scrollY = -empurraHero();
  GfxRect art = heroArtworkRect(HOME_LAYOUT_DINAMICA, 0);
  assert(art.x == 0 && art.y == 0 && art.w == 1920 && art.h == 1200);
  assert(topoFileiras() - scrollY == art.h * .94f);
  scrollY+=600; art=heroArtworkRect(HOME_LAYOUT_DINAMICA,0);
  assert(art.y==-600 && topoFileiras()-scrollY==528);
  scrollY+=600; art=heroArtworkRect(HOME_LAYOUT_DINAMICA,0);
  assert(art.y+art.h==0 && topoFileiras()-scrollY==-72);
  assert(homeConteudoX() == 396 && homeConteudoDireita() == 1880);
  assert(homeLarguraUtil() == 1484);
  assert(larguraDe(FILEIRA_CONTINUE) == 300 && alturaDe(FILEIRA_CONTINUE) == 169);
  assert(heroBaseCopia(HOME_LAYOUT_DINAMICA, art, 0, 0) < 1200);
  layout_janela_definir(1440, 900);
  art = heroArtworkRect(HOME_LAYOUT_DINAMICA, 0);
  assert(art.x == 0 && art.w == 2160 && nv_layout_h == 1350);
  assert(fabsf(art.h-1350)<.001f);
  puts("PASS: real desktop Home artwork, row rest position, content bounds, continue cards");
  return 0;
}
