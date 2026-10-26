#include "nav21.h"
#include "../src/biblioteca.c"
#undef ponteiro_alvo
static int fixture;
static int focusFixture(void){
  if(focoOrigem)return 10000+origemSel;
  if(estadoFoco)return 20000;
  if(focoPainel)return 30000+painelSel;
  if(focoCont)return 40000;
  return foco.fileira*1000+foco.coluna;
}
static void openFixture(void){
  biblioteca_iniciar();modo=fixture?MODO_LISTAS:MODO_PARAVER;focoOrigem=focoPainel=focoCont=estadoFoco=0;temAberta=0;
  if(fixture){listaTab=0;lst_ler_trakt("[{\"name\":\"Fixture list\",\"ids\":{\"trakt\":1},\"item_count\":12}]",0);}
  for(int i=0;i<cat_n();i++){CatItem item=*cat_item(i);item.naLista=1;item.listaFontes=~0u;cat_atualizar_item(i,&item);}
  reconstruirFonte(ajustes_fonte_biblioteca());
}
static void selectFixture(int id){
  focoOrigem=focoPainel=focoCont=estadoFoco=0;
  if(id>=10000&&id<20000){focoOrigem=1;origemSel=id-10000;}
  else if(id==20000)estadoFoco=1;
  else if(id>=30000&&id<40000){focoPainel=1;painelSel=id-30000;}
  else if(id==40000)focoCont=1;
  else {foco.fileira=id/1000;foco.coluna=id%1000;}
}
static void stepFixture(float dt){biblioteca_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){
  biblioteca_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target(navigation[i].id,navigation[i].rect);
  if(!fixture)for(int i=0;i<nCelulas;i++) {
    int id=(gradeIni()+i/colunas())*1000+i%colunas(),exists=0;
    for(int j=0;j<navigationCount;j++)if(navigation[j].id==id)exists=1;
    if(!exists)nav21_target(id,(GfxRect){bibX()+i%colunas()*passoColuna(),gradeY()+(i/colunas())*passoLinha()-scrollY,BIB_CARD_W,BIB_POSTER_H});
  }
}
static float transitionFixture(void){return animTela;}
static int pageFixture(void){return sair?-1:modo;}
void nav21_route_library_open(void){fixture=0;openFixture();selectFixture(1001);}
int nav21_route_library_focus(void){return focusFixture();}
void nav21_library(void){
  CatItem items[12]={0};for(int i=0;i<12;i++){snprintf(items[i].titulo,sizeof items[i].titulo,"Fixture %d",i);snprintf(items[i].imdb,sizeof items[i].imdb,"tt%07d",i+1);snprintf(items[i].tipo,sizeof items[i].tipo,"movie");items[i].naLista=1;items[i].listaFontes=1u<<AJ_SALVOS_LOCAL;items[i].fontes=CAT_FONTE_NUVIO;snprintf(items[i].meta,sizeof items[i].meta,"2000");}cat_definir(items,12);
  for(fixture=0;fixture<2;fixture++){Nav21Screen s={fixture?"Lists":"Library + Origins",openFixture,biblioteca_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};nav21_check(s);}
  fixture=1;openFixture();foco.fileira=BIB_FIL_GRADE;foco.coluna=0;Foco saved=foco;
  focoListas=foco;temFocoListas=1;temAberta=1;
  SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;biblioteca_evento(&back);
  nav21_expect("Lists/back","Lists, open list, Back: previous list grid",temAberta,0);nav21_expect("Lists/back","Lists, open list, Back: previous row",foco.fileira,saved.fileira);nav21_expect("Lists/back","Lists, open list, Back: previous column",foco.coluna,saved.coluna);
}
