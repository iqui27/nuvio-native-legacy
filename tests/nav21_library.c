#include "nav21.h"
#include <assert.h>
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
  if(temAberta && id<1000)acaoSel=id;
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
static void keyFixture(SDL_Keycode code){
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;biblioteca_evento(&e);
  if(code==SDLK_RETURN){e.type=SDL_KEYUP;biblioteca_evento(&e);}
  stepFixture(.1f);nav21_targets_reset();drawFixture();
}
// Follow the rendered layout so these behavior checks survive wrapped chips.
static void reachFixture(int id){
  int parent[512],via[512],queue[512],head=0,tail=0,start=-1,target=-1;
  GfxRect rects[512];
  for(int i=0;i<navigationCount;i++){
    parent[i]=-1;rects[i]=navigation[i].rect;
    if(navigation[i].id==focusFixture())start=i;
    if(navigation[i].id==id)target=i;
  }
  assert(start>=0 && target>=0);
  queue[tail++]=start;parent[start]=start;
  const SDL_Keycode keys[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT};
  while(head<tail && parent[target]<0){
    int from=queue[head++];
    for(int d=0;d<4;d++){
      int next=focus_rect_nearest(rects,navigationCount,from,(d==3)-(d==2),(d==1)-(d==0));
      if(next>=0 && parent[next]<0){parent[next]=from;via[next]=d;queue[tail++]=next;}
    }
  }
  assert(parent[target]>=0);
  int path[512],n=0;
  for(int i=target;i!=start;i=parent[i])path[n++]=via[i];
  while(n)keyFixture(keys[path[--n]]);
  nav21_expect("Library/routes","D-pad reaches rendered control",focusFixture(),id);
}
static void openListFixture(const CatItem *items){
  fixture=1;openFixture();fonte=FONTE_TRAKT;
  lst_teste_itens(NULL,0);
  selectFixture(BIB_FIL_GRADE*1000);
  keyFixture(SDLK_RETURN);
  DescItemLista extras[12]={0};
  for(int i=0;i<12;i++){extras[i].rank=i+1;extras[i].itemId=7000+i;}
  lst_teste_itens(items,12);lst_teste_extras(extras,12);
  stepFixture(.1f);nav21_targets_reset();drawFixture();
  nav21_expect("Lists/open","Lists, OK: owned list opened",temAberta && abertaPropria,1);
  nav21_expect("Lists/open","Lists, OK: initial action",focusFixture(),0);
}
static float transitionFixture(void){return animTela;}
static int pageFixture(void){return sair?-1:modo;}
static int ownsArrow(SDL_Keycode k){return (k==SDLK_UP || k==SDLK_DOWN) &&
  !focoOrigem && !focoPainel && !focoCont && !estadoFoco && foco.fileira>=gradeIni();}
void nav21_route_library_open(void){fixture=0;openFixture();selectFixture(1001);}
int nav21_route_library_focus(void){return focusFixture();}
void nav21_library(void){
  CatItem items[12]={0};for(int i=0;i<12;i++){snprintf(items[i].titulo,sizeof items[i].titulo,"Fixture %d",i);snprintf(items[i].imdb,sizeof items[i].imdb,"tt%07d",i+1);snprintf(items[i].tipo,sizeof items[i].tipo,"movie");items[i].naLista=1;items[i].listaFontes=1u<<AJ_SALVOS_LOCAL;items[i].fontes=CAT_FONTE_NUVIO;snprintf(items[i].meta,sizeof items[i].meta,"2000");}cat_definir(items,12);
  for(fixture=0;fixture<2;fixture++){Nav21Screen s={fixture?"Lists":"Library + Origins",openFixture,biblioteca_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture,ownsArrow};nav21_check(s);}
  fixture=0;openFixture();drawFixture();
  keyFixture(SDLK_UP);
  nav21_expect("Library/edges","To watch, Up: request clock",biblioteca_pediu_relogio(),1);
  openFixture();drawFixture();reachFixture(10000);
  nav21_expect("Library/edges","To watch, D-pad to Origins",focoOrigem,1);
  keyFixture(SDLK_LEFT);
  nav21_expect("Library/edges","Origins, Left: exit requested",sair,1);
  for(int i=0;i<90;i++)stepFixture(1.f/60);
  nav21_expect("Library/edges","Origins, Left: request sidebar",biblioteca_quer_sair(),1);

  openListFixture(items);
  reachFixture(4);
  nav21_expect("Lists/reorder","Open list, D-pad to Order",focusFixture(),4);
  keyFixture(SDLK_RETURN);keyFixture(SDLK_RETURN);
  nav21_expect("Lists/reorder","Order, OK, title OK: picked first title",ordemPega,0);
  keyFixture(SDLK_DOWN);
  CatItem moved={0};lst_item(1,&moved);
  nav21_expect("Lists/reorder","Picked title, Down: title moved",strcmp(moved.imdb,items[0].imdb),0);
  nav21_expect("Lists/reorder","Picked title, Down: picked position",ordemPega,1);
  nav21_expect("Lists/reorder","Picked title, Down: focus follows title",celulaEmFoco(),1);
  keyFixture(SDLK_UP);
  nav21_expect("Lists/reorder","Picked title, Up: moved back",ordemPega,0);
  keyFixture(SDLK_UP);keyFixture(SDLK_LEFT);keyFixture(SDLK_RIGHT);
  nav21_expect("Lists/reorder","Picked first title, Up/Left/Right: focus stays on title",celulaEmFoco(),0);
  keyFixture(SDLK_ESCAPE);
  nav21_expect("Lists/reorder","Picked title, Back: release only",ordemPega,-1);
  nav21_expect("Lists/reorder","Picked title, Back: editing remains",edModo,ED_ORDEM);
  keyFixture(SDLK_ESCAPE);
  nav21_expect("Lists/reorder","Order, Back: editing ends",edModo,ED_NADA);
  nav21_expect("Lists/reorder","Order, Back: list stays open",temAberta,1);

  // Check every rendered open-list directional control against its rectangles.
  openListFixture(items);
  Nav21Target base[512];int n=navigationCount;
  for(int i=0;i<n;i++)base[i]=(Nav21Target){navigation[i].id,navigation[i].rect};
  const SDL_Keycode arrows[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT};
  for(int i=0;i<n;i++)for(int d=0;d<4;d++){
    openListFixture(items);selectFixture(base[i].id);drawFixture();
    int current=-1;GfxRect rects[512];
    for(int j=0;j<navigationCount;j++){rects[j]=navigation[j].rect;if(navigation[j].id==base[i].id)current=j;}
    int next=focus_rect_nearest(rects,navigationCount,current,(d==3)-(d==2),(d==1)-(d==0));
    if(next<0 || next==current)continue;
    int expected=navigation[next].id;keyFixture(arrows[d]);
    char path[160];snprintf(path,sizeof path,"Open list, from %d, %s",base[i].id,SDL_GetKeyName(arrows[d]));
    nav21_expect("Lists/directions",path,focusFixture(),expected);
  }
  openListFixture(items);reachFixture(5);keyFixture(SDLK_RETURN);
  nav21_expect("Lists/notes","Notes, OK: note mode",edModo,ED_NOTAS);
  keyFixture(SDLK_DOWN);keyFixture(SDLK_RETURN);
  nav21_expect("Lists/notes","Notes, Down, title OK: keyboard opens",teclado_aberto(),1);
  nav21_expect("Lists/notes","Notes, Down, title OK: correct entry",notaItemId,7001);
  keyFixture(SDLK_ESCAPE);
  nav21_expect("Lists/notes","Note keyboard, Back: keyboard closes",teclado_aberto(),0);
  nav21_expect("Lists/notes","Note keyboard, Back: editing remains",edModo,ED_NOTAS);
  keyFixture(SDLK_ESCAPE);keyFixture(SDLK_ESCAPE);
  nav21_expect("Lists/back","Lists, open list, Back: previous list grid",temAberta,0);
  nav21_expect("Lists/back","Lists, open list, Back: previous focus",focusFixture(),BIB_FIL_GRADE*1000);
  lst_teste_itens(NULL,0);
}
