#include "nav21.h"
#include "ponteiro.h"
#include "extras.h"
#include "musica.h"
#include <string.h>
#include <stdio.h>
static MusEstado offlineMusic(const char *id,int season,MusAlbum *out){(void)id;(void)season;if(out){memset(out,0,sizeof *out);snprintf(out->titulo,sizeof out->titulo,"Offline soundtrack");out->n=3;}return MUS_PRONTO;}
#define mus_estado offlineMusic
static Uint32 clockFixture;
#define SDL_GetTicks() clockFixture
static void offlineExtras(const char *id,int series,long tmdb){(void)id;(void)series;(void)tmdb;}
#define extras_pedir offlineExtras
#include "../src/detail.c"
void nav21_extras(void);
#undef ponteiro_alvo
static int fixture;
static CatItem fixtureItem;
static void openFixture(void){clockFixture+=1000;reacao_fechar();sinopse_fechar();trocaarte_fechar();detail_fechar_seco();musicaui_fechar();HomeItem hi={0};hi.indice=0;hi.rect=(GfxRect){96,160,420,260};hi.titulo=fixtureItem.titulo;hi.meta=fixtureItem.meta;detail_abrir(&hi);nav21_extras();if(fixture>=0){nivel=1;foco.fileira=fixture;foco.coluna=0;}}
static void stepFixture(float dt){clockFixture+=(Uint32)(dt*1000);detail_atualizar(dt,SDL_GetTicks());
  // Settling advances the fixture clock past delayed text/section reveals.
  if(t>.999f){gateHero.inicio=gateHero.pronto=SDL_GetTicks()-500;for(int i=0;i<N_SECOES;i++)secVisto[i]=SDL_GetTicks()-500;}
}
static void drawFixture(void){detail_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target((navigation[i].row+1)*1000+navigation[i].col,navigation[i].rect);}
static int focusFixture(void){return nivel?(foco.fileira+1)*1000+foco.coluna:botao;}
static void selectFixture(int id){ponteiroDetalhe(id/1000-1,id%1000);}
static int pageFixture(void){return detail_aberto();}
void nav21_detail(void){
  memset(&fixtureItem,0,sizeof fixtureItem);snprintf(fixtureItem.titulo,sizeof fixtureItem.titulo,"Offline series");snprintf(fixtureItem.imdb,sizeof fixtureItem.imdb,"tt0000001");snprintf(fixtureItem.tipo,sizeof fixtureItem.tipo,"series");snprintf(fixtureItem.meta,sizeof fixtureItem.meta,"2020");snprintf(fixtureItem.sinopse,sizeof fixtureItem.sinopse,"Offline fixture synopsis");fixtureItem.nElenco=6;fixtureItem.nTemporadas=2;fixtureItem.temporadas[0]=1;fixtureItem.temporadas[1]=2;for(int i=0;i<6;i++)snprintf(fixtureItem.elenco[i].nome,sizeof fixtureItem.elenco[i].nome,"Actor %d",i);
  cat_definir(&fixtureItem,1);CatEp episodes[24]={0};for(int i=0;i<24;i++){episodes[i].temporada=i/12+1;episodes[i].episodio=i%12+1;snprintf(episodes[i].nome,sizeof episodes[i].nome,"Episode %d",i);}cat_definir_episodios(0,episodes,24);
  for(fixture=-1;fixture<N_SECOES;fixture++){
    openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);if(fixture>=0&&!secaoColunas(fixture))continue;
    char name[80];snprintf(name,sizeof name,"Detail/section-%d",fixture);
    Nav21Screen s={name,openFixture,detail_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,fixture<0?0:(fixture+1)*1000};nav21_check(s);
    openFixture();SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;detail_evento(&back);
    nav21_expect(name,"Open, Back: exit requested in one level",saindo,1);
  }
}
