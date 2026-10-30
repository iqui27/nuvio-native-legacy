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
int main(void) {
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
