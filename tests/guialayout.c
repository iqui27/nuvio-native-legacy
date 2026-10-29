/* Grade navigation contract, exercised through the real guide handlers. */
#include "plrui.h"
#define plrui_linha_foco(...) ((void)0)
#define gfx_recorte_dentro(...) ((GfxRecorteAnt){0})
#define gfx_recorte_voltar(...) ((void)0)
#define main guiav5_existing_main
#include "guiav5_bugs.c"
#undef main

static void key(SDL_Keycode code){
  SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=code;
  guia_evento(&e);e.type=SDL_KEYUP;guia_evento(&e);
}

int main(void){
  assert(SDL_Init(SDL_INIT_TIMER)==0);dados_iniciar(getenv("NUVIO_DADOS"));context();
  stagingZerar();sTeto=30000;
  for(int i=0;i<2;i++){
    int f=sFonte(LTV_M3U,i?"second":"first",i?"Second list":"First list","");
    GCru c={0};c.id=i?"b":"a";c.nome=i?"Channel B":"Channel A";
    c.grupo="News";c.logo=c.desc=c.epgId=c.tvgNome=c.url=c.ua=c.ref="";
    sGrupoInfo(f,c.grupo,1);sCanalAdd(f,&c);sLtvF[f].total++;
    if(!i)for(int j=0;j<10;j++){
      char id[16];snprintf(id,sizeof id,"extra-%d",j);c.id=id;
      sCanalAdd(f,&c);sLtvF[f].total++;
    }
  }
  favLido=1;adotarStaging();estado=G_PRONTO;aberta=1;
  g5ui_open("grade");gv_snap_force=1;gv_snapshot();gv_npins=1;gv_lin=0;gv_zone=0;
  CHECK(gv_vis(GV_BASE)>=4,"1080p Grade retains at least four complete channel rows");
  CHECK(GV_SOURCE_Y>=gv_rows_y(GV_BASE)+3*(GV_ROW_H+GV_ROW_GAP)+GV_ROW_H,
        "source footer uses bottom margin below four complete rows");
  /* D-pad traverses every channel; only list endpoints leave the grid. */
  gv_top=5;gv_lin=5;gv_rol=5;gv_rol_v=0;gv_dt=1.0f/60;
  key(SDLK_UP);CHECK(gv_zone==0&&gv_lin==4,"visible grid top Up reaches previous channel");
  gv_grid(GV_BASE);
  CHECK(gv_top==4&&gv_rol>4&&gv_rol<5,"Up scrolls smoothly above visible window");
  for(int row=3;row>=0;row--){
    key(SDLK_UP);CHECK(gv_zone==0&&gv_lin==row,"Up reaches every earlier channel");gv_grid(GV_BASE);
  }
  key(SDLK_UP);CHECK(gv_zone==1&&gv_lin==0,"channel one Up reaches pinned row");
  key(SDLK_UP);CHECK(gv_zone==5&&focus==1104,"pinned Up reaches button row");
  key(SDLK_UP);CHECK(gv_zone==5&&focus==1100,"button row Up reaches top tabs");
  key(SDLK_DOWN);CHECK(gv_zone==5&&focus==1104,"top tabs Down reaches button row");
  key(SDLK_DOWN);CHECK(gv_zone==1,"button row Down reaches pinned row");
  key(SDLK_DOWN);CHECK(gv_zone==0&&gv_lin==0,"pinned Down reaches channel one");
  gv_top=0;gv_lin=gv_vis(GV_BASE)-1;gv_rol=0;gv_rol_v=0;
  key(SDLK_DOWN);CHECK(gv_zone==0&&gv_lin==gv_vis(GV_BASE),"visible grid bottom Down reaches next channel");
  gv_grid(GV_BASE);
  CHECK(gv_top==1&&gv_rol>0&&gv_rol<1,"Down scrolls smoothly beyond visible window");
  for(int row=gv_lin+1;row<gv_match;row++){
    key(SDLK_DOWN);CHECK(gv_zone==0&&gv_lin==row,"Down reaches every later channel");gv_grid(GV_BASE);
  }
  key(SDLK_DOWN);CHECK(gv_zone==4,"only last channel Down reaches source footer");
  key(SDLK_UP);CHECK(gv_zone==0&&gv_lin==gv_match-1,"footer Up reaches last channel");
  gv_lin=5;point_focus(1200,0);key(SDLK_UP);
  CHECK(gv_zone==0&&gv_lin==gv_top+gv_vis(GV_BASE)-1,"footer Up reaches visible grid last row after pointer focus");
  for(int action=1104;action<=1106;action++){
    gv_zone=5;focus=action;
    key(SDLK_UP);CHECK(gv_zone==5&&focus==1100,"each action Up reaches tabs");
    key(SDLK_DOWN);CHECK(gv_zone==5&&focus==1104,"tabs Down stays in button row");
    focus=action;key(SDLK_DOWN);CHECK(gv_zone==1,"each action Down reaches pins");
  }
  gv_top=gv_lin=0;gv_snap_force=1;gv_snapshot();gv_npins=1;gv_zone=0;
  const int up[]={1,5,5},up_focus[]={-1,1104,1100};
  for(int i=0;i<3;i++){key(SDLK_UP);CHECK(gv_zone==up[i]&&(up_focus[i]<0||focus==up_focus[i]),"Grade Up: grid, pins, actions, tabs");}
  key(SDLK_DOWN);CHECK(gv_zone==5&&focus==1104,"tabs Down reaches Search");
  key(SDLK_RIGHT);CHECK(focus==1105,"actions Right reaches Options");
  key(SDLK_RIGHT);CHECK(focus==1106,"actions Right reaches Multiview");
  key(SDLK_RIGHT);CHECK(focus==1106,"action row does not wrap into tabs");
  key(SDLK_DOWN);CHECK(gv_zone==1,"actions Down reaches pins");
  key(SDLK_DOWN);CHECK(gv_zone==0&&gv_lin==0,"pins Down reaches grid");
  gv_top=gv_match-gv_vis(GV_BASE);gv_lin=gv_match-1;key(SDLK_DOWN);CHECK(gv_zone==4,"last channel Down reaches source footer");
  key(SDLK_UP);CHECK(gv_zone==0&&gv_lin==gv_match-1,"footer Up restores last channel");
  key(SDLK_DOWN);key(SDLK_ESCAPE);CHECK(gv_zone==0&&gv_lin==gv_match-1,"footer Back restores grid");
  gv_zone=5;focus=1100;
  key(SDLK_ESCAPE);CHECK(gv_zone==5&&focus==1104,"tabs Back retraces actions");
  key(SDLK_ESCAPE);CHECK(gv_zone==1,"actions Back retraces pins");
  key(SDLK_ESCAPE);CHECK(gv_zone==0,"pins Back retraces grid");
  gv_npins=0;gv_lin=0;key(SDLK_UP);CHECK(gv_zone==5&&focus==1104,"Up skips absent pins");
  key(SDLK_DOWN);CHECK(gv_zone==0,"Down skips absent pins");
  CHECK(!gv_chip_tem(1),"duplicate Sports chip absent");
  point_focus(1202,0);point_activate(1202,0);
  CHECK(gv_zone==4&&gv_match==11&&!strcmp(gv_focus()->id,"a"),"first source filters channels and retains footer focus");
  key(SDLK_RIGHT);CHECK(gv_chipi==6,"footer reaches second source");gv_activate();
  CHECK(gv_match==1&&!strcmp(gv_focus()->id,"b"),"second source filters by stable key");
  key(SDLK_LEFT);key(SDLK_LEFT);gv_activate();
  CHECK(!gv_fonte[0]&&gv_match==12,"All live restores both sources");
  gv_chipi=3;key(SDLK_LEFT);CHECK(gv_chipi==6,"Rewatch Left returns to last source");
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
