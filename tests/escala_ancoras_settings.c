#define AJUSTES_TESTE
#include "../src/ajustes.c"
#include "escala_ancoras.h"
void fixture_settings(float scale,int category) {
  ajustes_teste_ux_captura(category?1:0);
  valor[AJ_TAMANHO_AJUSTES]=2; valor[AJ_RAIL]=1; valor[AJ_HOME_LAYOUT]=0;
  gfx_escala_txt_definir(scale);
  for(int frame=0;frame<120;frame++) ajustes_atualizar(1.f/60,10000+frame*16);
  for(int frame=0;frame<18;frame++) { txt_novo_quadro(); ajustes_desenhar(10000); }
  shot_begin(category?"settings-category":"settings-index",scale);
  ajustes_desenhar(10000); shot_end();
  float old=gfx_escala(); gfx_escala_sair(ajustes_tamanho_ajustes());
  if(category) {
    shot_reset(); aj2DesenharPagina(1);
    GfxRect header=shot_text_bounds();
    GfxRect headerBox=shot_box(aj2PaginaR()); shot_anchor("category header",headerBox,0);
    shot_contains("header text inside",headerBox,header); shot_no_text_overlap();
    // Production list is drawn inside aj2ListaR; its first real line proves the origin.
    shot_reset(); ajDesenharFolha();
    GfxRect list=shot_text_bounds();
    GfxRect listBox=shot_box(aj2ListaR()); shot_anchor("category list",listBox,0);
    shot_contains("list text inside",listBox,list); shot_no_text_overlap();
    shot_disjoint("header / category list",header,list);
  } else {
    GfxRect index=shot_text_bounds();
    GfxRect box=shot_box(aj2GradeR()); shot_anchor("index panel",box,0);
    shot_contains("index text inside",box,index); shot_no_text_overlap();
  }
  gfx_escala_sair(old);
}
