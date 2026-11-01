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
  puts("PASS: desktop guide left edge >= sidebar right edge");
#else
  for(gv_side=0;gv_side<=1;gv_side++)
    assert(GV_X==48 && GV_MX==48+384*gv_side && GV_END==1872);
  puts("PASS: TV guide geometry unchanged");
#endif
  return 0;
}
