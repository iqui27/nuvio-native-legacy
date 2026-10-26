#include "nav21.h"
#include "../src/social.c"
static void openFixture(void){
  sair=selecionado=0;entrada=tempo=rolagem=velocidade=0;memset(focoAnim,0,sizeof focoAnim);
  memset(&dados,0,sizeof dados);dados.estado=SOCIAL_PRONTO;dados.n=4;
  snprintf(dados.pessoa.socialNome,sizeof dados.pessoa.socialNome,"Offline friend");
  for(int i=0;i<dados.n;i++)snprintf(dados.atividades[i].titulo,sizeof dados.atividades[i].titulo,"Fixture title %d",i);
}
static void stepFixture(float dt){social_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){social_desenhar(SDL_GetTicks());TXT_INI();float lx,w;t21_inicio(&lx,&w);for(int i=0;i<dados.n;i++)nav21_target(i,(GfxRect){lx+544,ANC(184)+i*156-rolagem,w-544,156});TXT_FIM();}
static int focusFixture(void){return selecionado;}
static void selectFixture(int i){selecionado=i;}
static float transitionFixture(void){return entrada;}
static int pageFixture(void){return !sair;}
void nav21_social(void){Nav21Screen s={"Social/Friends activity",openFixture,social_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};nav21_check(s);}
