#include "../src/home.c"
#include "escala_ancoras.h"
void fixture_home(float scale, int focused) {
  static const char *names[3][2]={{"home-modern-hero","home-modern-row"},{"home-standard-hero","home-standard-row"},{"home-dynamic-hero","home-dynamic-row"}};
  int lay=ajustes_home_layout();
  float old=gfx_escala_txt_entrar();
  sincronizarFileiras(); heroSetGarantir();
  for(int r=0;r<nFileiras;r++) animFocoFil[r]=0;
  focoHero=!focused; foco.fileira=0; foco.coluna=0;
  heroAtual=0; heroAnterior=-1; heroEntra=1; heroSai=0; heroDesliza=1;
  scrollY=focused?0:-empurraHero();
  fileirasVistasEm=10000; memset(filEntraEm,0,sizeof filEntraEm);
  for(int r=0;r<nFileiras;r++) filNAntes[r]=fileiras[r].n;
  gfx_escala_sair(old);
  // Warm the actual rasterizer; captures must contain all expected lines.
  for(int frame=0;frame<12;frame++) { txt_novo_quadro(); tex_bombear(8); SDL_Delay(1); home_desenhar(10000); }
  shot_begin(names[lay][focused],scale); home_desenhar(10000); shot_no_text_overlap(); shot_end();
  old=gfx_escala_txt_entrar();
  char meta[288]; hero_meta_linha(cat_item(0),meta,sizeof meta);
  GfxRect info={0};
  if(!(lay==2 && focused)) info=shot_text(txt_linha_corta(TXT_HERO_META,meta,179,179,179,255,lay==2?760:NV_HERO_SIN_W));
  // The first row is rendered by desenhaTituloFil, not a helper coordinate.
  GfxRect row=shot_text(txt_linha_corta(TXT_ROW_TITULO,fileiras[0].titulo,168,173,181,255,NV_TELA_W-homeConteudoX()-180));
  shot_anchor("first row title",row,0);
  if(!(lay==2 && focused)) {
    shot_reset(); desenhaHero(10000,0);
    // Exclude the full-width artwork and right-side pagination, retaining
    // every rendered logo, text, badge and action surface in the copy column.
    GfxRect hero=shot_component_bounds((GfxRect){homeConteudoX(),0,
      homeTextoLargura(lay==2?760:NV_HERO_SIN_W),NV_TELA_H});
    shot_anchor("hero block left/bottom",hero,1);
    shot_disjoint("complete hero / first row",hero,row);
  }
  if(!(lay==2 && focused)) shot_check("text height follows scale",info.h>12*scale && info.h<70*scale);
  gfx_escala_sair(old);
}
