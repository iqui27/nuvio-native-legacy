// LG DTS row: with Off appended as a third value it keeps the in-row segmented
// choice wherever the two-value row had it (Panel and List layouts).
#include "ajustes.h"
#define NV_WEBOS 1
#include "../src/ajustes.c"
#include <assert.h>
void avisos_tv_linha(char *line,size_t cap,char *model,size_t model_cap) {
  if(cap)line[0]=0;if(model_cap)model[0]=0;
}
static int fitsWithTwo(void) {
  float label=(float)txt_largura(TXT_V2_ROT,rotuloOpcao(AJ_DTS_AC3));
  return aj2SegLargura(AJ_DTS_AC3,2)+label+80.0f<=aj2ListaWRef()-44.0f;
}
int main(void) {
  char dir[]="/tmp/nuvio-dts-seg-XXXXXX";assert(mkdtemp(dir));
  assert(SDL_Init(SDL_INIT_TIMER)==0);setenv("NUVIO_DADOS",dir,1);
  dados_iniciar(dir);snprintf(dirAjustes,sizeof dirAjustes,"%s",dir);
  assert(txt_iniciar("deploy/app",1));
  ajustes_iniciar();
  assert(nValores(AJ_DTS_AC3)==3);
  int polish=-1;
  for(int l=0;l<nValores(AJ_IDIOMA);l++){
    valor[AJ_IDIOMA]=l;idiomaEscolhido();
    if(!strcmp(textoValor(AJ_IDIOMA),"Polski"))polish=l;
    valor[AJ_LAYOUT_AJUSTES]=0;
    assert(ajSegmentada(AJ_DTS_AC3)==fitsWithTwo());
  }
  assert(polish>=0);
  valor[AJ_IDIOMA]=polish;idiomaEscolhido();
  for(int lista=0;lista<2;lista++){
    valor[AJ_LAYOUT_AJUSTES]=lista;
    assert(ajSegmentada(AJ_DTS_AC3) && ajEmLinha(AJ_DTS_AC3)==(uxIndice>=2));
    assert(!strcmp(ajRotSeg(AJ_DTS_AC3,0),"Stereo") && !strcmp(ajRotSeg(AJ_DTS_AC3,1),"5.1"));
  }
  puts("PASS ajustes_dts_segmentado");
  return 0;
}
