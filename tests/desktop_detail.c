// Desktop rows stay readable while TV retains section focus dimming.
#include <assert.h>
#include <string.h>
#include "../src/layout.c"
#include "../src/detail.c"
int ajustes_home_layout(void) { return HOME_LAYOUT_PADRAO; }
int extras_temporadas(void) { return 0; }
int extras_progresso_serie(int *watched, int *aired) { return 0; }
int main(void) {
  layout_janela_definir(1920,1080);
  nivel=1; foco.fileira=SEC_NOTAS;
  for(int r=0;r<N_SECOES;r++) {
#ifdef NV_DESKTOP_UI
    assert(secaoAlvo(r)==1.f);
#else
    assert(secaoAlvo(r)==(r==SEC_NOTAS?1.f:.42f));
#endif
  }
#ifdef NV_DESKTOP_UI
  assert(dicaSecao(SEC_TRAILERS)==NULL);
#else
  assert(!strcmp(dicaSecao(SEC_TRAILERS),"OK abre no player, com som"));
#endif
  return 0;
}
