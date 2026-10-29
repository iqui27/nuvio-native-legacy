/* Grade navigation contract, exercised through the real guide handlers. */
#define main guiav5_existing_main
#include "guiav5_bugs.c"
#undef main

int main(void){
  assert(SDL_Init(SDL_INIT_TIMER)==0);dados_iniciar(getenv("NUVIO_DADOS"));context();
  stagingZerar();sTeto=30000;
  for(int i=0;i<2;i++){
    int f=sFonte(LTV_M3U,i?"second":"first",i?"Second list":"First list","");
    GCru c={0};c.id=i?"b":"a";c.nome=i?"Channel B":"Channel A";
    c.grupo="News";c.logo=c.desc=c.epgId=c.tvgNome=c.url=c.ua=c.ref="";
    sGrupoInfo(f,c.grupo,1);sCanalAdd(f,&c);sLtvF[f].total++;
  }
  favLido=1;adotarStaging();estado=G_PRONTO;aberta=1;
  g5ui_open("grade");gv_snap_force=1;gv_snapshot();gv_npins=1;gv_lin=0;gv_zone=0;
  const int up[]={1,5,5},up_focus[]={-1,1104,1100};
  for(int i=0;i<3;i++){gv_event(SDLK_UP,0);CHECK(gv_zone==up[i]&&(up_focus[i]<0||focus==up_focus[i]),"Grade Up: grid, pins, actions, tabs");}
  gv_event(SDLK_DOWN,0);CHECK(gv_zone==5&&focus==1104,"tabs Down reaches Search");
  gv_event(SDLK_RIGHT,0);CHECK(focus==1105,"actions Right reaches Options");
  gv_event(SDLK_RIGHT,0);CHECK(focus==1106,"actions Right reaches Multiview");
  gv_event(SDLK_RIGHT,0);CHECK(focus==1106,"action row does not wrap into tabs");
  gv_event(SDLK_DOWN,0);CHECK(gv_zone==1,"actions Down reaches pins");
  gv_event(SDLK_DOWN,0);CHECK(gv_zone==0&&gv_lin==0,"pins Down reaches grid");
  gv_lin=gv_match-1;gv_event(SDLK_DOWN,0);CHECK(gv_zone==4,"last channel Down reaches source footer");
  gv_event(SDLK_UP,0);CHECK(gv_zone==0&&gv_lin==gv_match-1,"footer Up restores last channel");
  gv_event(SDLK_DOWN,0);gv_event(SDLK_ESCAPE,0);CHECK(gv_zone==0&&gv_lin==gv_match-1,"footer Back restores grid");
  gv_zone=5;focus=1100;
  gv_event(SDLK_ESCAPE,0);CHECK(gv_zone==5&&focus==1104,"tabs Back retraces actions");
  gv_event(SDLK_ESCAPE,0);CHECK(gv_zone==1,"actions Back retraces pins");
  gv_event(SDLK_ESCAPE,0);CHECK(gv_zone==0,"pins Back retraces grid");
  gv_npins=0;gv_lin=0;gv_event(SDLK_UP,0);CHECK(gv_zone==5&&focus==1104,"Up skips absent pins");
  gv_event(SDLK_DOWN,0);CHECK(gv_zone==0,"Down skips absent pins");
  CHECK(!gv_chip_tem(1),"duplicate Sports chip absent");
  point_focus(1202,0);point_activate(1202,0);
  CHECK(gv_zone==4&&gv_match==1&&!strcmp(gv_focus()->id,"a"),"first source filters channels and retains footer focus");
  gv_event(SDLK_RIGHT,0);CHECK(gv_chipi==6,"footer reaches second source");gv_activate();
  CHECK(gv_match==1&&!strcmp(gv_focus()->id,"b"),"second source filters by stable key");
  gv_event(SDLK_LEFT,0);gv_event(SDLK_LEFT,0);gv_activate();
  CHECK(!gv_fonte[0]&&gv_match==2,"All live restores both sources");
  gv_chipi=3;gv_event(SDLK_LEFT,0);CHECK(gv_chipi==6,"Rewatch Left returns to last source");
  gv_chipi=5;gv_activate();CHECK(!strcmp(screen,"epg-saude"),"footer EPG opens health sheet");
  gv_voltar();CHECK(gv_zone==4&&gv_chipi==5,"Back from EPG restores footer");
  GfxRect pv=gv_preview_rect();
  CHECK(pv.x==GV_MX+GV_MW*.51f&&pv.y==96&&pv.w==GV_MW*.49f,"preview matches marked frame origin and width");
  CHECK(pv.y+pv.h==GV_BASE+gv_hh+40,"preview stops before pinned controls");
  snprintf(gv_pagina,sizeof gv_pagina,"cartoes");point_focus(1601,0);
  CHECK(gv_zone==6&&gv_card==1,"Cards pointer IDs remain distinct from grid IDs");
  snprintf(gv_pagina,sizeof gv_pagina,"grade");
  CHECK(failures==0,"layout contract complete");
  gv_fonte[0]=0;gv_grupo[0]=0;gv_source_off=0;
  failures+=guiav5_existing_main();
  return failures?1:0;
}
