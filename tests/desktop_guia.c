/* Actual desktop sidebar routing and populated footer; TV keeps its geometry. */
#ifdef DESKTOP_GUIA_ROUTE
#include <assert.h>
#include "../src/app.c"
int desktop_guia_route(void) {
  int failures=0;
  uiPreview=1;perfilSessaoAplicada=perfis_sessao_geracao();
  tela=TELA_BIBLIOTECA;menu_definir_destino(MENU_BIBLIOTECA);
  menu_ir(MENU_GUIA);
  /* Sidebar selection dispatches through trocarTela; Back consumes guia_quer_sair. */
  trocarTela(TELA_GUIA);
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;
  guia_evento(&e);assert(guia_quer_sair());voltarTela();
  if(tela!=TELA_BIBLIOTECA||menu_destino()!=MENU_BIBLIOTECA){
    fprintf(stderr,"FAIL: Library -> sidebar Guide -> Back: page=%s sidebar=%d\n",app_tela_nome(),menu_destino());failures++;
  }else puts("PASS: Library -> sidebar Guide -> Back -> Library, sidebar restored");
  return failures;
}
#else
#define epg_iniciar desktop_guia_no_epg
#include "../src/guia.c"
#undef epg_iniciar
void epg_iniciar(void);
void desktop_guia_no_epg(void) {} /* No EPG worker/network in this local fixture. */
static PonteiroAlvo source_target;
static float metadata_x,metadata_w;
static void desktop_guia_target(float x,float y,float w,float h,PonteiroFn f,PonteiroFn a,int id,int b);
static void desktop_guia_text(TxtLinha l,float x,float y);
#define ponteiro_alvo desktop_guia_target
#define txt_desenhar desktop_guia_text
#include "../src/guiav5ui.c"
#undef ponteiro_alvo
#undef txt_desenhar
#include <assert.h>
#include <stdio.h>
#include <SDL2/SDL_image.h>
#include "menu.h"
#include "desktop_native.h"
static void desktop_guia_target(float x,float y,float w,float h,PonteiroFn f,PonteiroFn a,int id,int b){
  if(id==1202)source_target=(PonteiroAlvo){.x=x,.y=y,.w=w,.h=h,.focar=f,.ativar=a,.a=id,.b=b};
  ponteiro_alvo(x,y,w,h,f,a,id,b);
}
static void desktop_guia_text(TxtLinha l,float x,float y){
  float k=gfx_escala_txt();
  if(fabsf(y*k-1049)<.1f){metadata_x=x*k;metadata_w=l.w*k;}
  txt_desenhar(l,x,y);
}
static void desktop_guia_seed(void){
  stagingZerar();sTeto=100;
  int f=sFonteChave("synthetic-local",LTV_M3U,"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW","");
  GCru c={.id="synthetic-channel",.nome="Synthetic Channel FHD",.logo="",.desc="Local synthetic guide evidence",.epgId="",.tvgNome="",.url="",.ua="",.ref="",.grupo="Synthetic channels",.tipo="channel",.numero=1};
  assert(f>=0&&sCanalAdd(f,&c));adotarStaging();
  fontesOk=cacheLido=favLido=modoLido=previewLido=1;estado=G_PRONTO;previewLigado=0;
}
#ifdef NV_DESKTOP_UI
int desktop_guia_route(void);
static int desktop_guia_footer(void){
  gv_snap_force=1;gv_snapshot();gv_zone=4;gv_chipi=2;gv_source_off=0;
  assert(nCanais==1&&nLtvF==1&&guia_canal_campos(0,NULL,NULL,NULL,NULL,NULL)&&gv_n==1);
  source_target=(PonteiroAlvo){0};metadata_w=0;num_focus=0;gv_reg=1;
  gv_chips_row();
  assert(metadata_w>0);
  /* TxtLinha stores integer dimensions across text-scale transforms. */
  int ok=source_target.w>0&&source_target.x>=GV_X&&
    source_target.x+source_target.w<=metadata_x-22&&metadata_w>0&&
    fabsf(metadata_x+metadata_w-GV_META_R)<2;
  if(!ok){fprintf(stderr,"FAIL: populated focused provider target x=%.0f w=%.0f; metadata x=%.0f w=%.0f row right=%.0f\n",source_target.x,source_target.w,metadata_x,metadata_w,GV_META_R);return 1;}
  source_target.ativar(source_target.a,source_target.b);
  assert(!strcmp(gv_fonte,"synthetic-local"));gv_fonte[0]=0;
  puts("PASS: real footer draws focused overwide source, retains metadata, registers working activation");return 0;
}
#endif
#ifdef NV_DESKTOP_UI
static void desktop_guia_capture(SDL_Window *window,const char *path,int width,int height){
  SDL_SetWindowSize(window,width,height);layout_janela_definir(width,height);
  glViewport(0,0,width,height);gfx_tamanho_alvo(width,height);
  gv_zone=4;gv_chipi=2;gv_source_off=0;gv_pag_t=1;gv_hh=GV_HERO_H;
  int fresh=getenv("DESKTOP_GUIA_CAPTURE_FRESH")!=NULL;
  printf("SEQUENCE: %s layout=%.0fx%.0f\n",fresh?"fresh":"existing",(double)NV_TELA_W,(double)NV_TELA_H);
  for(int i=0;i<(fresh?1:3);i++){
    txt_novo_quadro();gfx_novo_quadro();
    if(fresh){gfx_sem_recorte();glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);}
    glClear(GL_COLOR_BUFFER_BIT);
    assert(g5ui_draw());menu_desenhar(SDL_GetTicks());glFinish();
  }
  unsigned char *pixels=malloc((size_t)width*height*4);assert(pixels);
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  for(int y=0;y<height;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(size_t)(height-1-y)*width*4,(size_t)width*4);
  assert(SDL_SaveBMP(surface,path)==0);SDL_FreeSurface(surface);free(pixels);
  printf("CAPTURE: %s %dx%d\n",path,width,height);
}
#endif
int main(int argc,char **argv) {
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  int width=1280,height=800;
#ifdef NV_DESKTOP_UI
  if(argc==5&&!strcmp(argv[1],"--capture")){width=atoi(argv[3]);height=atoi(argv[4]);}
#endif
  SDL_Window *window=SDL_CreateWindow("Synthetic Desktop Guide",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window);SDL_GLContext gl=SDL_GL_CreateContext(window);assert(gl);
  int fresh=getenv("DESKTOP_GUIA_CAPTURE_FRESH")!=NULL;
  if(fresh)layout_janela_definir(width,height);
  gfx_tamanho_alvo(width,height);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");
  desktop_guia_seed();
  g5ui_open("grade");
  int failures=0;
#ifdef NV_DESKTOP_UI
  if(argc==1)failures+=desktop_guia_route();
  g5ui_open("grade");layout_janela_definir(fresh?width:1280,fresh?height:800);gv_side=1;
  if(fresh&&width>1280)snprintf(ltvF[0].nome,sizeof ltvF[0].nome,"Synthetic Local Provider");
  failures+=desktop_guia_footer();
  if(argc==5&&!strcmp(argv[1],"--capture")){
    assert(getenv("NV_CAPTURA")&&getenv("NV_CAPTURA_GL"));
    desktop_native_init(window);desktop_native_appearance("DarkAqua");SDL_HideWindow(window);
    menu_iniciar();menu_definir_destino(MENU_GUIA);
    if(atoi(argv[3])>1280)snprintf(ltvF[0].nome,sizeof ltvF[0].nome,"Synthetic Local Provider");
    desktop_guia_capture(window,argv[2],atoi(argv[3]),atoi(argv[4]));
    goto finish;
  }
#endif
#ifdef NV_DESKTOP_UI
  int sizes[][2]={{1440,900},{1280,800},{1920,1080},{2560,1000}};
  for(int i=0;i<4;i++){
    layout_janela_definir(sizes[i][0],sizes[i][1]);
    assert(layout_desktop_ativo());
    for(gv_side=0;gv_side<=1;gv_side++){
      printf("%dx%d side=%d: GV_X=%.0f MX=%.0f END=%.0f rail=%.0f W=%.0f\n",sizes[i][0],sizes[i][1],(int)gv_side,GV_X,GV_MX,GV_END,(double)NV_DESKTOP_RAIL_W,(double)NV_TELA_W);
      assert(GV_X>=NV_DESKTOP_RAIL_W && GV_MX>=GV_X);
      assert(GV_END<=NV_TELA_W && GV_GRID_W>200);
    }
  }
  /* Source chips: scroll and drawing share one budget (metadata reserved). */
  for(int i=0;i<4;i++){
    layout_janela_definir(sizes[i][0],sizes[i][1]);
    for(gv_side=0;gv_side<=1;gv_side++){
      float meta=300,w[12];int n=12,off=0;
      for(int k=0;k<n;k++)w[k]=(GV_W-240+10)/n;   /* fits only without metadata */
      float avail=GV_W-meta;
      int end=gv_chips_fit(w,n,n-1,&off,avail,avail);
      assert(end==n && off>0);                      /* focused last chip is drawn */
      float x=GV_X;for(int k=off;k<end;k++)x+=w[k];
      assert(x-10<=GV_META_R-meta);                 /* chips stop before metadata */
      assert(GV_META_R==GV_X+GV_W && GV_META_R<=NV_TELA_W);
    }
  }
  /* Review cases: 1280x800 last chip drawn; 2560x1000 metadata at the row edge. */
  layout_janela_definir(1280,800);gv_side=1;
  {float w[2]={GV_W-200,10};int off=0;assert(gv_chips_fit(w,2,1,&off,GV_W-300,GV_W-300)==2&&off==1);}
  layout_janela_definir(2560,1000);
  assert(GV_META_R>1872.0f && GV_META_R==NV_TELA_W-48.0f);
  puts("PASS: desktop source chips visible, metadata at row right edge");
  puts("PASS: desktop guide left edge >= sidebar right edge");
#else
  for(gv_side=0;gv_side<=1;gv_side++)
    assert(GV_X==48 && GV_MX==48+384*gv_side && GV_END==1872);
  assert(GV_META_R==1872);
  puts("PASS: TV guide geometry unchanged");
#endif
#ifdef NV_DESKTOP_UI
finish:
  if(argc==5)desktop_native_finish();
#endif
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);IMG_Quit();SDL_Quit();
  return failures?1:0;
}
#endif
