/* Offline production guide focus/layout gate and PNG evidence. */
#define xtream_consultar guide_test_consultar
#define main original_guide_shots
#include "guiav5_principal_shot.c"
#undef main
#undef xtream_consultar
static int movie_requests,series_requests;
char *guide_test_consultar(const char *server,const char *user,const char *password,const char *action,int seconds,int *status){
  (void)server;(void)user;(void)password;(void)seconds;*status=200;
  if(!strcmp(action,"get_vod_streams")){movie_requests++;return strdup("[{\"stream_id\":1,\"name\":\"Provider movie\"}]");}
  if(!strcmp(action,"get_series")){series_requests++;return strdup("[{\"series_id\":2,\"name\":\"Provider series\"}]");}
  return strdup("[]");
}
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
  dados_iniciar(getenv("NUVIO_DADOS"));
  const char *source_labels[]={"Adicionar fonte","Add source","Adăugați sursă","Додати джерело","Добавить источник","Ajouter une source","Quelle hinzufügen","Agregar fuente","Aggiungi fonte","Bron toevoegen","Dodaj źródło","Kaynak ekle","Adicionar fonte","Lägg till källa","Tilføj kilde","Legg til kilde","Přidat zdroj","Pridať zdroj","Dodaj vir","Forrás hozzáadása","Pridėti šaltinį","Dodaj izvor","Dodaj izvor","Добавяне на източник","Προσθήκη πηγής","Tambahkan sumber","Thêm nguồn","ソースを追加","添加源","新增來源","إضافة مصدر"};
  for(int language=0;language<IDIOMA_N;language++){
    char path[1024];snprintf(path,sizeof path,"%s/ajustes.txt",getenv("NUVIO_DADOS"));
    FILE *settings=fopen(path,"w");assert(settings);fprintf(settings,"idioma %d\n",language);fclose(settings);
    ajustes_dir(getenv("NUVIO_DADOS"));ajustes_iniciar();
    CHECK(!strcmp(i18n("Adicionar fonte"),source_labels[language]),"Add source names a provider in every language");
  }
  dados_gravar("ajustes.txt","idioma 1\n");ajustes_dir(getenv("NUVIO_DADOS"));ajustes_iniciar();
  ajustes_definir_vidro(0);context();semear();
  nLtvF=1;ltvF[0].tipo=LTV_M3U;ltvF[0].base="";strcpy(ltvF[0].chave,"fixture");
  for(int i=0;i<nCanais;i++)canais[i].base="";
  /* A mixed entry must neither suppress nor disappear after native loading. */
  mixedVod=calloc(1,sizeof *mixedVod);assert(mixedVod);nMixedVod=1;
  mixedVod[0].kind=G5_MOVIE;mixedVod[0].age=-1;
  strcpy(mixedVod[0].source,"fixture");strcpy(mixedVod[0].id,"mixed-movie");strcpy(mixedVod[0].name,"Mixed movie");
  xtream_definir_servidor("https://provider.invalid");xtream_definir_usuario("fixture");xtream_definir_senha("fixture");
  draft.live=0;g5ui_open("vod-filmes");
  CHECK(atomic_load(&busy)&&!source_loaded,"mixed VOD does not mark provider inventory loaded");
  Uint32 wait_started=SDL_GetTicks();
  while(atomic_load(&busy)&&SDL_GetTicks()-wait_started<2000){g5ui_update();SDL_Delay(1);}
  CHECK(movie_requests==1&&series_requests==1,"mixed VOD still requests provider movies and series");
  int mixed=0,movies=0,shows=0;
  for(int i=0;i<count;i++){mixed+=!strcmp(inventory[i].id,"mixed-movie");movies+=!strcmp(inventory[i].name,"Provider movie");shows+=!strcmp(inventory[i].name,"Provider series");}
  CHECK(count==3&&mixed==1&&movies==1&&shows==1,"provider completion merges mixed movie with native movies and series");
  g5ui_open("vod-filmes");CHECK(count==3&&movie_requests==1&&series_requests==1,"reopening VOD neither duplicates entries nor reloads provider");
  free(mixedVod);mixedVod=NULL;nMixedVod=0;clear_catalog();xtream_esquecer();
  source_loaded=1;epg_so_provedores(1);
  g5ui_open("grade");capture(w,out,"sidebar-hidden");
  unsigned char pixel[4];
  int helper_pixels=0;
  for(int y=156;y<166;y++)for(int x=48;x<1100;x++){
    glReadPixels(x,1079-y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    helper_pixels+=pixel[0]>70||pixel[1]>70||pixel[2]>70;
  }
  CHECK(helper_pixels==0,"guide has no helper text below tabs");
  glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
  GVCh preview_channel={0};strcpy(preview_channel.nome,"SporTV");
  gv_broadcast((GfxRect){100,200,600,300},&preview_channel,NULL,0);
  glReadPixels(130,1080-230,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  CHECK(pixel[0]<20&&pixel[1]<20&&pixel[2]<20,"preview background stays near-black");
  glReadPixels(350,1080-350,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  CHECK(abs(pixel[0]-pixel[1])<=2&&abs(pixel[1]-pixel[2])<=2,"preview fallback tile stays neutral");
  char logo_path[PATH_MAX];assert(realpath("tests/fixtures/logos/comfundo.png",logo_path));
  snprintf(preview_channel.logo,sizeof preview_channel.logo,"%s",logo_path);
  Uint32 logo_started=SDL_GetTicks();
  while(!tex_obter_larg(logo_path,132)&&SDL_GetTicks()-logo_started<2000){tex_bombear(8);SDL_Delay(1);}
  gv_broadcast((GfxRect){100,200,600,300},&preview_channel,NULL,0);
  glReadPixels(360,1080-320,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  CHECK(pixel[0]>100&&pixel[1]>100&&pixel[2]>100,"neutral preview still renders channel logo");
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
  inventory=calloc(G5_CAP,sizeof *inventory);count=G5_CAP;
  for(int i=0;i<count;i++){inventory[i].kind=G5_MOVIE;sprintf(inventory[i].name,"Movie %d",i);}
  /* Use registered item targets: both pointer and remote activate these IDs. */
  const int positions[]={0,799,800,801,802,803,804,805,806,807,808,820,823,915,1100,1101,1102,1103,1104,1105,4095};
  for(unsigned i=0;i<sizeof positions/sizeof *positions;i++){
    g5ui_open("grade");g5ui_open("vod-filmes");filter(G5_MOVIE);
    int item_base=focus;focus=item_base+positions[i];quadro();
    int target=-1;for(int j=0;j<num_focus;j++)if(next_focus[j]==focus)target=next_focus[j];
    CHECK(target>=0,"large catalog item has a visible registered target");
    if(target>=0){if(i%2)key(SDLK_RETURN);else point_activate(target,0);CHECK(!strcmp(screen,"vod-titulo")&&selected==positions[i],"large catalog title opens without activating navigation");}
  }
  count=12;
  g5ui_open("grade");g5ui_open("vod-filmes");int item_base=focus;focus=item_base+6;quadro();key(SDLK_LEFT);
  CHECK(focus==800&&nav<0,"VOD first-column Left reaches sidebar");
  key(SDLK_RIGHT);CHECK(focus==item_base+6,"VOD sidebar Right restores exact item");
  focus=item_base;key(SDLK_UP);CHECK(nav==3,"VOD Up reaches active tab");
  key(SDLK_DOWN);CHECK(nav<0&&focus==item_base,"VOD tab Down restores content");
  focus=item_base+5;key(SDLK_RIGHT);CHECK(focus==item_base+5&&!vod_sidebar,"VOD right edge never opens hidden controls");
  focus=item_base+6;key(SDLK_LEFT);key(SDLK_DOWN);CHECK(vod_sidebar&&focus==801,"VOD sidebar stays in visible controls");
  key(SDLK_RIGHT);CHECK(!vod_sidebar&&focus==item_base+6,"VOD sidebar preserves item after navigation");
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
  g5ui_open("grade");g5ui_open("vod-filmes");quadro();key(SDLK_LEFT);key(SDLK_ESCAPE);
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
  focus=item_base+6;key(SDLK_LEFT);capture(w,out,"vod-open");
  g5ui_close();return failures?1:0;
}
