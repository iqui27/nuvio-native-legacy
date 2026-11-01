/* Desktop TV Guide starts right of the native sidebar; TV keeps x=48. */
#include "../src/guia.c"
#include "../src/guiav5ui.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  g5ui_open("grade");
#ifdef NV_DESKTOP_UI
  int sizes[][2]={{1440,900},{1280,800},{1920,1080},{2560,1000}};
  for(int i=0;i<4;i++){
    layout_janela_definir(sizes[i][0],sizes[i][1]);
    assert(layout_desktop_ativo());
    for(gv_side=0;gv_side<=1;gv_side++){
      printf("%dx%d side=%d: GV_X=%.0f MX=%.0f END=%.0f rail=%.0f W=%.0f\n",sizes[i][0],sizes[i][1],(int)gv_side,GV_X,GV_MX,GV_END,(double)NV_DESKTOP_RAIL_W,(double)NV_TELA_W);
      assert(GV_X>=NV_DESKTOP_RAIL_W && GV_MX>=GV_X);
      assert(GV_END<=NV_TELA_W && GV_GRID_W>200);
    }
  }
  /* Source chips: scroll and drawing share one budget (metadata reserved). */
  for(int i=0;i<4;i++){
    layout_janela_definir(sizes[i][0],sizes[i][1]);
    for(gv_side=0;gv_side<=1;gv_side++){
      float meta=300,w[12];int n=12,off=0;
      for(int k=0;k<n;k++)w[k]=(GV_W-240+10)/n;   /* fits only without metadata */
      float avail=GV_W-meta;
      int end=gv_chips_fit(w,n,n-1,&off,avail,avail);
      assert(end==n && off>0);                      /* focused last chip is drawn */
      float x=GV_X;for(int k=off;k<end;k++)x+=w[k];
      assert(x-10<=GV_META_R-meta);                 /* chips stop before metadata */
      assert(GV_META_R==GV_X+GV_W && GV_META_R<=NV_TELA_W);
    }
  }
  /* Review cases: 1280x800 last chip drawn; 2560x1000 metadata at the row edge. */
  layout_janela_definir(1280,800);gv_side=1;
  {float w[2]={GV_W-200,10};int off=0;assert(gv_chips_fit(w,2,1,&off,GV_W-300,GV_W-300)==2&&off==1);}
  layout_janela_definir(2560,1000);
  assert(GV_META_R>1872.0f && GV_META_R==NV_TELA_W-48.0f);
  puts("PASS: desktop source chips visible, metadata at row right edge");
  puts("PASS: desktop guide left edge >= sidebar right edge");
#else
  for(gv_side=0;gv_side<=1;gv_side++)
    assert(GV_X==48 && GV_MX==48+384*gv_side && GV_END==1872);
  assert(GV_META_R==1872);
  puts("PASS: TV guide geometry unchanged");
#endif
  return 0;
}
