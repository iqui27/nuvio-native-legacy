#include <assert.h>
#include <stdio.h>
#include "../src/ctxmenu.c"
const char *i18n(const char *s) { return s; }
int ajustes_fonte_biblioteca(void) { return AJ_SALVOS_LOCAL; }
int cat_imdb_na_lista_na_fonte(const char *id,int source) { (void)id;(void)source;return 0; }
int salvos_tem(const char *id) { (void)id;return 1; }
int simkl_ativo(void) { return 0; }
int simkl_na_plantowatch(const char *id) { (void)id;return 0; }
int cat_historico_estado_id(const char *id,const char *type) { (void)id;(void)type;return 1; }
static int listsLoading=1,listsCount;
int lst_minhas_carregando(void) { return listsLoading; }
int lst_minhas_n(void) { return listsCount; }
int lst_minha(int i,LstLista *out) {
  if(i<0 || i>=listsCount) return 0;
  memset(out,0,sizeof *out);snprintf(out->titulo,sizeof out->titulo,"List %d",i);return 1;
}
float nv_layout_w=1920,nv_layout_h=1080;
int layout_modo_mobile(void) { return 0; }
int main(void) {
  aberto=1;pagina=3;
  ctx_desktop_menu();assert(aberto && pagina==3); // Must return before native tracking while cold.
  DesktopMenuItem lists[LST_MAX];char names[LST_MAX][96];
  assert(desktopLists(lists,names)==-1); // Cold cache must not track a disabled menu.
  listsLoading=0;listsCount=12;
  assert(desktopLists(lists,names)==12);
  assert(lists[11].action==4011 && !strcmp(lists[11].title,"List 11"));
  listsCount=0;assert(desktopLists(lists,names)==1 && lists[0].action==0);
  const int actions[]={OP_DESKTOP_PLAY,OP_DETALHES,OP_DESKTOP_TRAILER,OP_DESKTOP_COPY,OP_LISTA,OP_ASSISTIDO,OP_RECOMENDAR,OP_ESTILO,OP_F_MENU};
  const int legacy[]={21,8,22,23,1,2,4,6,12};
  const char *symbols[]={"play","info.circle","film","link","plus","eye","person.2","rectangle.grid.1x2","slider.horizontal.3"};
  CatItem ci={0};snprintf(ci.imdb,sizeof ci.imdb,"tt0000001");
  nOps=0;
  for(int i=0;i<9;i++) juntar("Menu action",actions[i]);
  for(int i=0;i<9;i++) {
    DesktopMenuItem item=desktopItem(i,&ci);
    assert(item.action==legacy[i]);
    assert(desktopActionIndex(item.action)==i);
    assert(!strcmp(item.symbol,symbols[i]));
    assert(item.checked==(i==4||i==5));
    assert(item.separator==(i==4||i==6||i==7));
  }
  assert(desktopActionIndex(-1)==-1);
  puts("PASS: all nine native tags preserve legacy action IDs, order, SF Symbols, separators and toggle states");
}
