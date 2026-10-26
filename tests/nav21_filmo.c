#include "nav21.h"
#include "../src/filmo.c"
#undef ponteiro_alvo
static int fixture;
static void openFixture(void){folhaAtiva=0;pessoaLembrada=-1;filmo_abrir();aba=fixture;zona=0;col=0;}
static void stepFixture(float dt){filmo_atualizar(dt);}
static void drawFixture(void){
  filmo_desenhar(1);ilha_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target(navigation[i].zone*10000+navigation[i].col,navigation[i].rect);
  if(aba==1) {
    TXT_INI();
    for(int i=0;i<ni;i++)if(i<topo*gradeCols()||i>=(topo+gradeLinhas())*gradeCols()) {
      float y=compacta(CONTEUDO_Y,230);
      nav21_target(40000+i,area((GfxRect){72+(i%gradeCols())*(220*gfx_poster_k()+24)/colunaK(),60+(py(y)+100+(i/gradeCols())*gradePasso()-scroll-60)/linhaK(),220*gfx_poster_k()/colunaK(),330*gfx_poster_k()/linhaK()}));
    }
    TXT_FIM();
  }
}
static int focusFixture(void){return camada?50000+op:zona*10000+(zona==3?ano:col);}
static void selectFixture(int id){TXT_INI();toqueFoco(id/10000,id%10000);if(zona==4&&aba==1)topo=col/gradeCols();TXT_FIM();}
static void keyFixture(const SDL_Event *e){filmo_evento(e);}
static float transitionFixture(void){return entrada;}
static int pageFixture(void){return saindo?-1:aba;}
static float sheetTransition(void){return ilha_corpo_t();}
static void internalNext(void){folha(indices[0]);}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;filmo_evento(&e);}
void nav21_filmo(void){
  char json[18000];size_t used=snprintf(json,sizeof json,"{\"name\":\"Fixture\",\"biography\":\"Offline biography\",\"combined_credits\":{\"cast\":[");
  for(int i=0;i<12;i++)used+=snprintf(json+used,sizeof json-used,"%s{\"id\":%d,\"title\":\"Film %d\",\"media_type\":\"movie\",\"release_date\":\"2000-01-01\",\"vote_count\":500}",i?",":"",i+1,i+1);
  snprintf(json+used,sizeof json-used,"]}}");pessoa_ler(123,json,"2026-10-13");
  const char *names[]={"History","Filmography","Filmography/about","Filmography/achievements"};
  for(fixture=0;fixture<4;fixture++){Nav21Screen s={names[fixture],openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};nav21_check(s);}
  fixture=1;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);drawFixture();zona=4;col=0;
  Nav21Screen s={"Filmography/title-options",openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,sheetTransition};
  nav21_motion(s,internalNext,"Title, OK: sheet transition >=150ms");
  nav21_motion(s,internalBack,"Title options, Back: sheet transition >=150ms");
  nav21_expect(s.name,"Title options, Back: previous page",aba,1);nav21_expect(s.name,"Title options, Back: previous focus",focusFixture(),40000);
}
