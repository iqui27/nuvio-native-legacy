// #439: accept 300% in persisted preferences and preserve the ASS scale.
#include "../src/player.c"
#include <assert.h>
int main(void) {
  player_dir(getenv("NUVIO_DADOS"));
  FILE *f=fopen(prefsArquivo(),"w"); assert(f); fputs("leg_tamanho 300\n",f); fclose(f);
  prefsLer();
  fprintf(stderr,"300%% preference: %d\n",legEstilo.tamanho);
  assert(legEstilo.tamanho==300); assert(escalaFonteAss()==2.5);
  return 0;
}
