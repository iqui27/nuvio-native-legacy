#include "nav21.h"
#include "../src/social.c"
static void openFixture(void){
  sair=selecionado=filtro=0;zona=1;entrada=tempo=rolagem=velocidade=segSel=segFoco=revT=0;memset(focoAnim,0,sizeof focoAnim);
  memset(&dados,0,sizeof dados);dados.estado=SOCIAL_PRONTO;dados.n=4;
  snprintf(dados.pessoa.socialNome,sizeof dados.pessoa.socialNome,"Offline friend");
  for(int i=0;i<dados.n;i++)snprintf(dados.atividades[i].titulo,sizeof dados.atividades[i].titulo,"Fixture title %d",i);
}
static void stepFixture(float dt){social_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){social_desenhar(SDL_GetTicks());TXT_INI();GfxRect il=ilhaRect();int v[20],sec[20],nv=ativVisiveis(v);float ry[20],sy[20];montarLista(v,nv,time(NULL),ry,sec,sy);for(int i=0;i<nv;i++)nav21_target(i,(GfxRect){il.x+26,il.y+SO_LISTA+ry[i]-rolagem,il.w-52,SO_LINHA});TXT_FIM();}
static int focusFixture(void){return selecionado;}
static void selectFixture(int i){selecionado=i;}
static float transitionFixture(void){return entrada;}
static int pageFixture(void){return !sair;}
void nav21_social(void){Nav21Screen s={"Social/Friends activity",openFixture,social_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};nav21_check(s);}
