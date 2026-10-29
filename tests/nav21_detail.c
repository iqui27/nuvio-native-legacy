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
#include "amigostitulo.h"
static int friendsFixture;
static int offlineFriends(const char *imdb,AmigosTitulo *out){(void)imdb;if(friendsFixture&&out){memset(out,0,sizeof *out);out->n=out->total=out->nViu=1;snprintf(out->a[0].nome,sizeof out->a[0].nome,"Offline friend");out->a[0].viu=1;}return friendsFixture;}
#define amigostitulo_obter offlineFriends
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
static int ownsArrow(SDL_Keycode k){
  if(!nivel)return focoServidor>=0 || focoAmigos || focoVersao || k==SDLK_UP || (k==SDLK_DOWN && servidoresDisponiveis) || (carro&&!carCheia);
  return k==SDLK_UP || k==SDLK_DOWN ||
    ((k==SDLK_LEFT||k==SDLK_RIGHT) && (foco.fileira==SEC_TEMPORADAS || foco.fileira==SEC_MUSICAS || foco.fileira==SEC_PROGTEMP));
}
static float transitionFixture(void){return t;}
static int pageFixture(void){return detail_aberto();}
static void serverRegression(void){
  fixture=-1;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);
  servidoresDisponiveis=2;drawFixture();
  int registered=0;for(int i=0;i<navigationCount;i++)if(navigation[i].row==-2&&navigation[i].col==1)registered=1;
  nav21_expect("Detail/server chips","Visible Jellyfin chip registered",registered,1);
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;detail_evento(&e);
  nav21_expect("Detail/server chips","Play, Down: Jellyfin chip",focoServidor,1);
  nav21_expect("Detail/server chips","Play, Down: stays in hero",nivel,0);
  drawFixture();e.key.keysym.sym=SDLK_UP;detail_evento(&e);
  nav21_expect("Detail/server chips","Play, Down, Up: leaves Jellyfin chip",focoServidor,-1);
  nav21_expect("Detail/server chips","Play, Down, Up: previous button",botao,0);
  drawFixture();e.key.keysym.sym=SDLK_DOWN;detail_evento(&e);
  drawFixture();detail_evento(&e);
  nav21_expect("Detail/server chips","Play, Down, Down: leaves Jellyfin chip",focoServidor,-1);
  nav21_expect("Detail/server chips","Play, Down, Down: enters sections",nivel,1);
  nav21_expect("Detail/server chips","Play, Down, Down: seasons or episodes",foco.fileira==SEC_TEMPORADAS||foco.fileira==SEC_EPISODIOS,1);
  // Friends card under the chips: Play -> Jellyfin -> friends -> sections, never back to the chip.
  fixture=-1;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);
  servidoresDisponiveis=2;friendsFixture=1;e.key.keysym.sym=SDLK_DOWN;
  drawFixture();detail_evento(&e);
  nav21_expect("Detail/friends","Play, Down: Jellyfin chip",focoServidor,1);
  drawFixture();detail_evento(&e);
  nav21_expect("Detail/friends","Play, Down, Down: friends card",focoAmigos,1);
  drawFixture();detail_evento(&e);
  nav21_expect("Detail/friends","Friends, Down: not back to Jellyfin chip",focoServidor,-1);
  nav21_expect("Detail/friends","Friends, Down: enters sections",nivel,1);
  nav21_expect("Detail/friends","Friends, Down: seasons or episodes",foco.fileira==SEC_TEMPORADAS||foco.fileira==SEC_EPISODIOS,1);
  friendsFixture=0;
}
static void carouselRegression(void){
  fixture=-1;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);
  carro=1;carCheia=0;carPos=carAplicado=0;carN=2;carIdx[0]=carIdx[1]=0;botao=nBotoes()-1;drawFixture();
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RIGHT;detail_evento(&e);
  nav21_expect("Detail/carousel","Last button, Right: next title",carPos,1);
  nav21_expect("Detail/carousel","Last button, Right: retain hero",nivel,0);
  carAplicado=carPos;botao=0;e.key.keysym.sym=SDLK_LEFT;detail_evento(&e);
  nav21_expect("Detail/carousel","First button, Left: previous title",carPos,0);
  carAplicado=carPos;e.key.keysym.sym=SDLK_DOWN;detail_evento(&e);
  nav21_expect("Detail/carousel","Carousel, Down: expand card",carCheia,1);
}
void nav21_detail(void){
  memset(&fixtureItem,0,sizeof fixtureItem);snprintf(fixtureItem.titulo,sizeof fixtureItem.titulo,"Offline series");snprintf(fixtureItem.imdb,sizeof fixtureItem.imdb,"tt0000001");snprintf(fixtureItem.tipo,sizeof fixtureItem.tipo,"series");snprintf(fixtureItem.meta,sizeof fixtureItem.meta,"2020");snprintf(fixtureItem.sinopse,sizeof fixtureItem.sinopse,"Offline fixture synopsis");fixtureItem.nElenco=6;fixtureItem.nTemporadas=2;fixtureItem.temporadas[0]=1;fixtureItem.temporadas[1]=2;for(int i=0;i<6;i++)snprintf(fixtureItem.elenco[i].nome,sizeof fixtureItem.elenco[i].nome,"Actor %d",i);
  cat_definir(&fixtureItem,1);CatEp episodes[24]={0};for(int i=0;i<24;i++){episodes[i].temporada=i/12+1;episodes[i].episodio=i%12+1;snprintf(episodes[i].nome,sizeof episodes[i].nome,"Episode %d",i);}cat_definir_episodios(0,episodes,24);
  serverRegression();carouselRegression();
  for(fixture=-1;fixture<N_SECOES;fixture++){
    openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);if(fixture>=0&&!secaoColunas(fixture))continue;
    char name[80];snprintf(name,sizeof name,"Detail/section-%d",fixture);
    Nav21Screen s={name,openFixture,detail_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,fixture<0?0:(fixture+1)*1000,transitionFixture,ownsArrow};nav21_check(s);
    openFixture();SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;detail_evento(&back);
    nav21_expect(name,"Open, Back: exit requested in one level",saindo,1);
  }
}
