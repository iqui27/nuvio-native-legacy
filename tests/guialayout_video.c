/* Verify production mini geometry and native cover crop without audio changes. */
#define video_janela_fonte capture_crop
#include "../src/player.c"
#undef video_janela_fonte
#include <assert.h>
static int crop[8],calls;
void capture_crop(int sx,int sy,int sw,int sh,int dx,int dy,int dw,int dh){
  int values[]={sx,sy,sw,sh,dx,dy,dw,dh};memcpy(crop,values,sizeof crop);calls++;
}
int main(void){
  mini=miniGuia=1;miniGuiaCaixa=(PlrRect){978,96,894,472};player_mini_guia_preencher(1);
  VideoSimulacao sim={.largura=1920,.altura=1080};video_simular(&sim);
  PlrRect r=miniDestino();assert(r.x==978&&r.y==96&&r.w==894&&r.h==472);
  janelaMandar(r);assert(calls==1&&crop[0]==0&&crop[1]>0&&crop[2]==1920&&crop[3]<1080);
  assert(crop[4]==978&&crop[5]==96&&crop[6]==894&&crop[7]==472);
  sim.largura=720;sim.altura=576;video_simular(&sim);janelaMandar(r);
  assert(calls==2&&crop[0]==0&&crop[2]==720&&crop[3]<576);
  assert(fabsf((float)crop[2]/crop[3]-894.0f/472)<.006f);
  player_mini_guia_preencher(0);r=miniDestino();assert(r.w<894&&r.h==472);
  player_mini_guia_preencher(1);miniGuia=0;r=miniDestino();assert(r.w<=PLR_PIP_W&&r.h<=PLR_PIP_H);
  miniGuia=1;comVideo=1;janelaAnimar((PlrRect){0,0,1920,1080},miniDestino());
  assert(janAtiva);for(int i=0;i<PLR_ENC_PASSOS+1;i++)janelaPasso(janEm);
  assert(!janAtiva&&crop[4]==978&&crop[5]==96&&crop[6]==894&&crop[7]==472);
  puts("PASS: native preview cover keeps aspect, fills frame, preserves PiP and completes transition");
  return 0;
}
