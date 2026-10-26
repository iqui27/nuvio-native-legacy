#include "nav21.h"
#include "../src/streams.c"
static void openFixture(void){anim=0;stream_folha_nome("Offline title");stream_folha_abrir();folhaRelogio=0;}
static void stepFixture(float dt){stream_folha_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){stream_folha_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target((navigation[i].group+2)*10000+navigation[i].id+100,navigation[i].rect);}
static int focusFixture(void){return (grupo+2)*10000+(grupo==0?filtro:foco)+100;}
static void selectFixture(int id){grupo=id/10000-2;if(grupo==0)filtro=id%10000-100;else foco=id%10000-100;}
static int pageFixture(void){return aberta;}
void nav21_sources(void){Stream v[4]={0};for(int i=0;i<4;i++){snprintf(v[i].url,sizeof v[i].url,"file:///fixture-%d.mp4",i);snprintf(v[i].rotulo,sizeof v[i].rotulo,"Fixture %d",i);snprintf(v[i].provedor,sizeof v[i].provedor,"Offline");v[i].altura=1080;}stream_definir_lista(v,4);openFixture();int primary=focusFixture();Nav21Screen s={"Sources sheet",openFixture,stream_folha_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary};nav21_check(s);}
