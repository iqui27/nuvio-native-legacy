/* Offline production guide focus/layout gate and PNG evidence. */
#define main original_guide_shots
#include "guiav5_principal_shot.c"
#undef main
static int failures;
static void capture_now(SDL_Window *window,const char *dir,const char *name){
  char path[1024];snprintf(path,sizeof path,"%s/%s.png",dir,name);
  unsigned char *pixels=malloc(1920*1080*4);assert(pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++)memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  assert(!IMG_SavePNG(s,path));SDL_FreeSurface(s);free(pixels);SDL_GL_SwapWindow(window);
}
#define CHECK(c, s) do { if(!(c)){fprintf(stderr,"FAIL: %s\n",s);failures++;}else puts("PASS: " s); } while(0)
static void key(SDL_Keycode k){SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=k;g5ui_event(&e);quadro();}
int main(int argc,char **argv){
  const char *out=argc>1?argv[1]:"/Volumes/ExternalSSD/tmp/rev/guialib";mkdir(out,0755);
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Guide focus proof",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));ajustes_dir(getenv("NUVIO_DADOS"));ajustes_definir_vidro(0);context();semear();
  nLtvF=1;ltvF[0].tipo=LTV_M3U;ltvF[0].base="";strcpy(ltvF[0].chave,"fixture");
  for(int i=0;i<nCanais;i++)canais[i].base="";
  source_loaded=1;epg_so_provedores(1);
  g5ui_open("grade");capture(w,out,"sidebar-hidden");
  CHECK(GV_MX<60&&GV_MW>1800,"hidden sidebar gives content full width");
  CHECK(gv_vis(GV_BASE)>=4,"grid uses space freed by hero and footer");
  key(SDLK_LEFT);CHECK(gv_zone==3,"grid Left opens sidebar");
  Uint32 slide_started=SDL_GetTicks();
  while(SDL_GetTicks()-slide_started<90){quadro();SDL_Delay(8);}
  CHECK(gv_side>0&&gv_side<1,"sidebar captured during real spring transition");
  capture_now(w,out,"sidebar-mid-animation");
  capture(w,out,"sidebar-open");
  CHECK(GV_MX>420,"open sidebar reserves content width");
  key(SDLK_RIGHT);CHECK(gv_zone==0&&gv_lin==0,"sidebar Right restores grid item");
  key(SDLK_LEFT);key(SDLK_ESCAPE);CHECK(g5ui_active()&&!gv_side_open,"Back closes sidebar before guide");
  key(SDLK_ESCAPE);CHECK(!g5ui_active(),"next Back exits guide after closing sidebar");
  g5ui_open("grade");
  inventory=calloc(G5_CAP,sizeof *inventory);count=12;
  for(int i=0;i<count;i++){inventory[i].kind=G5_MOVIE;sprintf(inventory[i].name,"Movie %d",i);}
  g5ui_open("vod-filmes");focus=6;quadro();key(SDLK_LEFT);
  CHECK(focus==800&&nav<0,"VOD first-column Left reaches sidebar");
  key(SDLK_RIGHT);CHECK(focus==6,"VOD sidebar Right restores exact item");
  focus=0;key(SDLK_UP);CHECK(nav==3,"VOD Up reaches active tab");
  key(SDLK_DOWN);CHECK(nav<0&&focus==0,"VOD tab Down restores content");
  focus=5;key(SDLK_RIGHT);CHECK(focus==5&&!vod_sidebar,"VOD right edge never opens hidden controls");
  focus=6;key(SDLK_LEFT);key(SDLK_DOWN);CHECK(vod_sidebar&&focus==801,"VOD sidebar stays in visible controls");
  key(SDLK_RIGHT);CHECK(!vod_sidebar&&focus==6,"VOD sidebar preserves item after navigation");
  CHECK(canalPorId(canais[0].id)!=NULL,"enabled source resolves channel");
  ltv_pref("fixture",1)->desligada=1;
  CHECK(canalPorId(canais[0].id)==NULL,"disabled source cannot resolve channel fallback");
  ltv_pref("fixture",1)->desligada=0;
  char addon_key[LTV_CHAVE];ltv_chave_fonte(LTV_ADDON,"https://disabled.test",addon_key,sizeof addon_key);
  ltv_pref(addon_key,1)->desligada=1;CHECK(!guia_fonte_playback_permitida("https://disabled.test"),"disabled addon excluded from playback fallback");
  ltv_pref(addon_key,1)->desligada=0;
  const char *views[]={"grade","cartoes","esportes-ligas","fontes-gerenciar","busca","ajustes"};
  for(unsigned i=0;i<sizeof views/sizeof *views;i++){
    g5ui_open(views[i]);quadro();
    if(!strcmp(views[i],"esportes-ligas"))focus=2070;
    if(!strcmp(views[i],"busca"))focus=1799;
    if(!strcmp(views[i],"ajustes"))focus=1949;
    int saved=focus;int zone=gv_zone;
    if(!strcmp(views[i],"grade"))gv_tfoc=0;
    key(SDLK_LEFT);if(!gv_side_open)fprintf(stderr,"view=%s focus=%d zone=%d sidebar=%d\n",views[i],focus,gv_zone,gv_side_open);CHECK(gv_side_open,"view Left reaches sidebar");
    key(SDLK_RIGHT);CHECK(!gv_side_open&&focus==saved&&gv_zone==zone,"view Right restores exact focus");
    key(SDLK_LEFT);key(SDLK_ESCAPE);CHECK(!gv_side_open&&!strcmp(screen,views[i]),"Back closes sidebar only");
    if(!strcmp(views[i],"fontes-gerenciar")){focus=100;key(SDLK_UP);CHECK(nav==0,"Sources Up reaches tabs");key(SDLK_DOWN);}
    else{if(!strcmp(views[i],"grade"))gv_lin=0;if(!strcmp(views[i],"cartoes"))gv_card=gv_lin=0;
      key(SDLK_UP);CHECK(gv_zone==5&&focus>=1100&&focus<=1105,"view Up reaches top tabs");
      key(SDLK_DOWN);CHECK(gv_zone!=5,"tabs Down returns to view");}
  }
  g5ui_open("grade");g5ui_open("vod-filmes");focus=0;quadro();key(SDLK_LEFT);key(SDLK_ESCAPE);
  CHECK(!strcmp(screen,"vod-filmes")&&!vod_sidebar,"VOD Back closes sidebar only");
  key(SDLK_ESCAPE);CHECK(!strcmp(screen,"grade"),"VOD Back returns one view level");
  gv_zone=0;key(SDLK_ESCAPE);CHECK(gv_side_open,"guide Back opens sidebar");
  key(SDLK_ESCAPE);CHECK(!gv_side_open&&g5ui_active(),"guide Back closes sidebar only");
  key(SDLK_ESCAPE);CHECK(!g5ui_active(),"guide Back exits after sidebar layer");
  g5ui_open("grade");gv_zone=0;gv_lin=0;key(SDLK_UP);CHECK(gv_zone==5,"Up from first content row reaches tabs");
  key(SDLK_DOWN);key(SDLK_DOWN);CHECK(gv_zone==2,"preview remains reachable without buttons");
  g5ui_open("cartoes");gv_card=gv_lin=0;
  key(SDLK_DOWN);key(SDLK_DOWN);CHECK(gv_card==10,"Cards reaches channels beyond first ten");
  key(SDLK_LEFT);CHECK(gv_side_open,"Cards later first column reaches sidebar");
  key(SDLK_RIGHT);CHECK(gv_card==10,"Cards restores later item");
  gv_card=gv_lin=0;capture(w,out,"cards-large-logos");
  g5ui_open("vod-filmes");capture(w,out,"vod-hidden");
  focus=6;key(SDLK_LEFT);capture(w,out,"vod-open");
  g5ui_close();return failures?1:0;
}
