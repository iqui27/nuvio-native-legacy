/* Verify the v5 video-plane rectangle against the card/large setting. */
#include "../src/guia.c"
#include "../src/guiav5ui.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  g5ui_open("grade");
  float x,y,w,h;
  assert(!ajustes_guia_previa_grande());
  guia_preview_rect(&x,&y,&w,&h);
  assert(w==626 && h==GV_HERO_H && y==GV_BASE);
  assert(x+w==GV_MX+GV_MW);
  ajustes_definir_guia_previa_grande(1);
  guia_preview_rect(&x,&y,&w,&h);
  assert(w==1040 && h==GV_HERO_H && x-GV_MX>=640);
  assert(x+w==GV_MX+GV_MW);
  gv_side=1;
  guia_preview_rect(&x,&y,&w,&h);
  assert(w>626 && x-GV_MX>=640);
  ajustes_definir_guia_previa_grande(0);
  guia_preview_rect(&x,&y,&w,&h);
  assert(w==626 && h==GV_HERO_H);
  puts("PASS: v5 card/large geometry and readable information beside sidebar");
  return 0;
}
