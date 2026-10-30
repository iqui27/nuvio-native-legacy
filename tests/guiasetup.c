/* Real guide state, persisted source switches and remote navigation at TV 1080p. */
#include "epg.h"
/* The transport request is outside this UI regression; do not start network threads. */
static void setup_epg_refresh(void){}
#define epg_recarregar setup_epg_refresh
#define main principal_proof_main
#include "guiav5_principal_shot.c"
#undef main
static int failures;
static void check(int ok,const char *message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);failures++;}}
static void key(SDL_Keycode k){SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=k;g5ui_event(&e);e.type=SDL_KEYUP;g5ui_event(&e);}
static void settle(SDL_Window *w){Uint32 end=SDL_GetTicks()+380;do{quadro();SDL_GL_SwapWindow(w);SDL_Delay(8);}while(SDL_GetTicks()<end);}
static void options_frame(SDL_Window *w,const char *dir,const char *name){
  capture(w,dir,name);char a[1024],b[1024];snprintf(a,sizeof a,"%s/%s-app.png",dir,name);snprintf(b,sizeof b,"%s/%s.png",dir,name);assert(!rename(a,b));
}
static int shot_w,shot_h;

int main(int argc,char **argv){
  setbuf(stdout,NULL);setbuf(stderr,NULL);
  assert(argc==3);int before=atoi(argv[2]);shot_w=1920;shot_h=1080;
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Live TV setup proof",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  assert(w&&SDL_GL_CreateContext(w));SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar()&&txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  dados_gravar("ajustes.txt","idioma 1\nidiomaAutoLocal 0\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");
  ajustes_dir(getenv("NUVIO_DADOS"));gfx_escala_txt_definir(1);ponteiro_iniciar();context();ltv_pref_carregar(perfis_ativo());semear();
  snprintf(ltvF[0].nome,sizeof ltvF[0].nome,"Living room TV");ltvF[0].nCanais=nCanais;ltvF[0].total=nCanais;
  ltvF[0].atualizada=time(NULL)-3600;ltvF[0].epgCom=8;
  static GGrupoInfo groups_fixture[]={{"Sports",4,4,1},{"News",2,2,1},{"Films",3,3,1},{"Kids",3,3,1}};
  ltvF[0].gi=groups_fixture;ltvF[0].ngi=4;
  ltvF[0].tipo=LTV_XTREAM;ltvF[0].base="";
  for(int i=0;i<nCanais;i++)canais[i].base="";
  /* Prevent network activity; switches still run their production reload path. */
  fioVivo=1;
  g5ui_open("fontes-gerenciar");settle(w);options_frame(w,argv[1],"setup");
  focus=100;nav=-1;settle(w);key(SDLK_RETURN);settle(w);
  options_frame(w,argv[1],"disabled");
  check(!strcmp(screen,"fontes-gerenciar")&&focus==100,"disable stays on source row");
  ltv_pref_carregar(-99);ltv_pref_carregar(perfis_ativo());
  check(!guia_fonte_ligada(0),"disabled switch persists after reload");
  key(SDLK_LEFT);settle(w);
  check(!gv_side_open,"Left on a source must not jump into sidebar");
  focus=100;nav=-1;key(SDLK_RETURN);settle(w);options_frame(w,argv[1],"reenabled");
  check(guia_fonte_ligada(0),"same row re-enables disabled source");
  check(guia_canal_campos(0,NULL,NULL,NULL,NULL,NULL),"re-enable immediately restores cached source channels");
  ltv_pref_carregar(-99);ltv_pref_carregar(perfis_ativo());check(guia_fonte_ligada(0),"re-enabled switch persists");
#ifndef GUIASETUP_BASELINE
  puts("CHECK: Add/Back preserves active channels and EPG");
  draft.type=1;draft.live=draft.epg=1;committed=1;use_saved=1;
  snprintf(draft.playlist,sizeof draft.playlist,"https://example.test/live.m3u");
  snprintf(draft.xmltv,sizeof draft.xmltv,"https://example.test/epg.xml");
  inventory=calloc(G5_CAP,sizeof *inventory);count=total=1;source_loaded=1;
  inventory[0].kind=G5_LIVE;snprintf(inventory[0].id,sizeof inventory[0].id,"retained");
  G5Item channel;check(g5ui_m3u_channels(&channel,1)==1,"active M3U fixture exposes channels");
  focus=0;key(SDLK_RETURN);settle(w);
  check(g5ui_m3u_channels(&channel,1)==1&&!strcmp(g5ui_xmltv(),"https://example.test/epg.xml"),"Add keeps active channels and EPG during setup");
  key(SDLK_ESCAPE);settle(w);
  check(!strcmp(screen,"fontes-gerenciar")&&draft.type==1&&draft.live&&use_saved&&
    !strcmp(draft.playlist,"https://example.test/live.m3u")&&g5ui_m3u_channels(&channel,1)==1&&
    !strcmp(g5ui_xmltv(),"https://example.test/epg.xml"),"Add then Back keeps channels and EPG for next refresh");
  /* Return to the original fixture for the approved screenshots. */
  clear_catalog();erase(&draft,sizeof draft);committed=0;use_saved=0;
  focus=204;settle(w);key(SDLK_RETURN);settle(w);
  check(!strcmp(screen,"ajustes"),"Options remains reachable from source list");
  key(SDLK_ESCAPE);settle(w);check(!strcmp(screen,"fontes-gerenciar")&&focus==204,"Options Back returns to setup opener");
  focus=100;settle(w);key(SDLK_RIGHT);settle(w);check(focus==101,"Right reaches Refresh on same source");
  key(SDLK_LEFT);settle(w);check(focus==100,"Left returns to source toggle");
  focus=200;settle(w);int days=ajustes_guia_dias();key(SDLK_RETURN);settle(w);check(ajustes_guia_dias()!=days&&focus==200,"EPG range changes inline");
  puts("CHECK: inline grouping");focus=202;settle(w);int grouped=ajustes_livetv_agrupar();key(SDLK_RETURN);settle(w);
  check(ajustes_livetv_agrupar()!=grouped&&focus==202,"grouping changes inline and retains focus");
  key(SDLK_RETURN);settle(w);
  puts("CHECK: inline order");focus=203;settle(w);int order=ajustes_guia_ordem();key(SDLK_RETURN);settle(w);
  check(ajustes_guia_ordem()!=order&&focus==203,"sort changes inline and persists");
  puts("CHECK: categories");focus=300;settle(w);key(SDLK_RETURN);settle(w);
  GuiaCategoria categories[8];int nc=guia_categorias(categories,8);
  check(nc==4&&categories[0].oculta,"hidden category can be changed on setup screen");
  for(int i=0;i<nCanais;i++)if(!strcmp(cats[canais[i].cat].nome,"Sports"))check(!guia_canal_campos(i,NULL,NULL,NULL,NULL,NULL),"hidden category stays absent from v5 guide");
  key(SDLK_RETURN);settle(w);guia_categorias(categories,8);check(!categories[0].oculta,"same category row restores visibility");
  puts("CHECK: only Xtream source off, empty guide and one-press recovery");
  focus=100;key(SDLK_RETURN);settle(w);g5ui_open("grade");settle(w);
  check(!gv_total_n&&!strcmp(screen,"grade"),"disabled Xtream leaves zero visible channels");
  int turn_on=focus;
  check(turn_on==4000,"off empty state focuses first Turn on action, never first-source wizard");
  int first_source=0;for(int i=0;i<num_focus;i++)if(next_focus[i]>=1700&&next_focus[i]<=1702)first_source=1;
  check(!first_source,"configured source never renders first-source wizard actions");
  options_frame(w,argv[1],"sources-turned-off");
  key(SDLK_RETURN);settle(w);
  printf("CHECK: recovery on=%d channels=%d screen=%s focus=%d zone=%d\n",guia_fonte_ligada(0),gv_total_n,screen,focus,gv_zone);
  check(guia_fonte_ligada(0)&&gv_total_n>0&&!strcmp(screen,"grade"),"one-press Turn on restores cached channels and exits empty state");
  check(recarregarPend,"Turn on schedules source refresh after cached channels");
  if(guia_fonte_ligada(0))guia_fonte_alternar(0);
  gv_snap_force=1;g5ui_open("grade");settle(w);
  key(SDLK_UP);settle(w);gv_zone=5;focus=1105;key(SDLK_RETURN);settle(w);
  printf("CHECK: Options screen=%s focus=%d zone=%d on=%d\n",screen,focus,gv_zone,guia_fonte_ligada(0));
  check(!strcmp(screen,"fontes-gerenciar"),"Options reachable after last source disabled");
  focus=100;key(SDLK_RETURN);settle(w);key(SDLK_ESCAPE);settle(w);
  check(!strcmp(screen,"grade")&&gv_total_n>0,"Sources Turn on restores channels on return to guide");
  /* An enabled source with no channels is unavailable, not unconfigured. */
  int retained=nCanais;nCanais=0;ltvF[0].saude=LTV_SAUDE_FORA;gv_snap_force=1;gv_zone=0;focus=-1;settle(w);
  check(focus==1799,"all-failed state focuses Manage sources");
  key(SDLK_RETURN);settle(w);check(!strcmp(screen,"fontes-gerenciar"),"failed source opens Sources without setup");
  nCanais=retained;ltvF[0].saude=LTV_SAUDE_OK;gv_snap_force=1;key(SDLK_ESCAPE);settle(w);
  int retained_sources=nLtvF;nLtvF=0;nCanais=0;gv_snap_force=1;settle(w);
  first_source=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==1700)first_source=1;
  check(first_source,"unconfigured guide still offers first-source setup");
  nLtvF=retained_sources;nCanais=retained;gv_snap_force=1;
#endif
  /* Same declared fixtures in before/after: no account or stream credentials. */
  puts("CHECK: Live footer");free(cats);cats=NULL;free(canais);canais=NULL;nCanais=0;semear();snprintf(ltvF[0].nome,sizeof ltvF[0].nome,"Living room TV");ltvF[0].nCanais=nCanais;
  canais[0].nome="SporTV FHD";
  static const VideoSimulacao sim={.aceita=1,.pronto=1,.tocando=1,.largura=1920,.altura=1080,.info={.codec="H.264"}};video_simular(&sim);
  aberta=1;player_mini_no_guia(978,96,894,472);previewId[0]=0;
  g5ui_open("grade");gv_zone=0;gv_lin=0;gv_snap_force=1;settle(w);options_frame(w,argv[1],"live-footer");
  puts("CHECK: VOD");clear_catalog();inventory=calloc(G5_CAP,sizeof *inventory);count=total=8;source_loaded=1;
  for(int i=0;i<count;i++){
    G5Item *it=&inventory[i];it->kind=G5_MOVIE;it->age=-1;
    snprintf(it->source,sizeof it->source,"fixture");snprintf(it->id,sizeof it->id,"movie%d",i);
    snprintf(it->name,sizeof it->name,"%s",i%2?"The last journey":"Beyond the horizon");
    snprintf(it->group,sizeof it->group,"Cinema");snprintf(it->ext,sizeof it->ext,"mkv");
    snprintf(it->poster,sizeof it->poster,"design/glass-ilha/arte.jpg");
    snprintf(it->backdrop,sizeof it->backdrop,"design/glass-ilha/arte.jpg");
  }
  build_groups();vod_index_inventory(&vod_data);vod_rebuild();
  g5ui_open("vod-filmes");focus=VOD_ITEM;nav=-1;settle(w);options_frame(w,argv[1],"vod-selected");
#ifndef GUIASETUP_BASELINE
  puts("CHECK: source identity after publication");g5ui_open("fontes-gerenciar");focus=100;nav=-1;settle(w);
  nLtvF=2;ltvF[1]=ltvF[0];snprintf(ltvF[1].chave,sizeof ltvF[1].chave,"another");
  focus=102;key(SDLK_RETURN);settle(w);
  check(teclado_aberto(),"Edit opens rename keyboard");
  GFonteLtv swap=ltvF[0];ltvF[0]=ltvF[1];ltvF[1]=swap;settle(w);
  SDL_Event keyboard={.type=SDL_KEYDOWN};keyboard.key.keysym.sym=SDLK_RETURN;teclado_evento(&keyboard);
  char expected_name[96];snprintf(expected_name,sizeof expected_name,"%s",teclado_texto());
  check(strcmp(expected_name,ltvF[1].nome)!=0,"rename changes keyboard text");
  keyboard.key.keysym.sym=SDLK_DOWN;
  for(int i=0;i<12;i++)teclado_evento(&keyboard);
  keyboard.key.keysym.sym=SDLK_RIGHT;for(int i=0;i<24;i++)teclado_evento(&keyboard);
  keyboard.key.keysym.sym=SDLK_RETURN;teclado_evento(&keyboard);g5ui_update();settle(w);
  check(!teclado_aberto(),"rename keyboard completes");
  GuiaFonteSaude renamed,untouched;guia_fonte_saude(1,&renamed);guia_fonte_saude(0,&untouched);
  check(!strcmp(renamed.nome,expected_name)&&strcmp(untouched.nome,expected_name),"rename follows original source key across publication");
  check(focus>=104&&focus<108&&!strcmp(sources_key,"fixture"),"background publication follows source key rather than row index");
  puts("CHECK: disabled source cache survives refresh");
  char cache_file[64];cacheNomeFonte("fixture",cache_file,sizeof cache_file);dados_gravar(cache_file,"retained-cache");
  ltv_pref("fixture",1)->desligada=1;ltvF[1].deCache=0;ltvF[1].nCanais=12;cacheGravar();
  char *saved_cache=dados_ler(cache_file);check(saved_cache&&!strcmp(saved_cache,"retained-cache"),"refresh cannot overwrite disabled source cache");free(saved_cache);ltv_pref("fixture",1)->desligada=0;
  puts("CHECK: name and add-on removal persistence");
  LtvPref *pref=ltv_pref("fixture",1);snprintf(pref->nome,sizeof pref->nome,"Renamed TV");ltv_pref_gravar();ltv_pref_carregar(perfis_ativo());
  GuiaFonteSaude health;guia_fonte_saude(1,&health);check(!strcmp(health.nome,"Renamed TV"),"edited display name survives preference reload");
  ltvF[0].gi=NULL;ltvF[0].ngi=0;ltvF[0].tipo=LTV_ADDON;check(guia_fonte_remover(0),"Remove handles Live TV add-on subscription");
  ltv_pref_carregar(perfis_ativo());check(ltv_pref("another",0)->removida,"removed subscription persists");
  stagingZerar();check(sFonteChave("another",LTV_ADDON,"Removed","")==-1,"removed source cannot reappear on refresh");
  puts("CHECK: re-enable loads disk cache after disabled inventory publication");
  semear();ltvF[0].gi=NULL;ltvF[0].ngi=0;ltvF[0].base="";
  for(int i=0;i<nCanais;i++)canais[i].base="";
  snprintf(ltvF[0].chave,sizeof ltvF[0].chave,"g5m3u");
  char config_file[64];snprintf(config_file,sizeof config_file,"guiav5-source-p%d.json",perfis_ativo());dados_gravar(config_file,"{}");
  ltvF[0].deCache=0;ltvF[0].nCanais=nCanais;cacheGravar();
  check(guia_fonte_alternar(0)&&!guia_fonte_ligada(0),"cache fixture disabled");
  nCanais=0;fioVivo=0;
  check(guia_fonte_alternar(0)&&guia_canal_campos(0,NULL,NULL,NULL,NULL,NULL),"Turn on loads disk cache before network refresh when memory inventory is empty");
  check(recarregarPend,"disk-cache restoration still schedules refresh");fioVivo=1;
#endif
  if(!before)check(!failures,"all setup regression checks");
  printf("%s: source focus, persistence, re-enable, Options and spatial navigation\n",failures?"FAIL":"PASS");
  /* Texture/network workers are process-owned; avoid SDL teardown beneath them. */
  fflush(NULL);_Exit(failures?1:0);
}
