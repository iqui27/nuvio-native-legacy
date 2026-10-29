#include "nav21.h"
#include "../src/busca.c"
static void openPage(void){busca_iniciar();}
static void stepPage(float dt){busca_atualizar(dt,SDL_GetTicks());}
static void drawPage(void){
  busca_desenhar(SDL_GetTicks());
  TXT_INI();
  for(int r=0;r<=kbFil;r++)for(int c=0;c<KB_COLUNAS[r];c++){
    GfxRect q=retanguloTecla(r,c);float scale=gfx_escala();
    nav21_target(r*10+c,(GfxRect){q.x*scale,q.y*scale,q.w*scale,q.h*scale});
  }
  TXT_FIM();
}
static int focusPage(void){return focoKb.fileira*10+focoKb.coluna;}
static void selectPage(int id){painel=0;campoFoco=0;focoKb.fileira=id/10;focoKb.coluna=id%10;}
static int page(void){return !sair;}
void telaspolir_search(void){
  Nav21Screen s={"Search/keyboard",openPage,busca_evento,stepPage,drawPage,focusPage,selectPage,page,0,NULL};
  nav21_check(s);
}
