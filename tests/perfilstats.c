// Real rendering and key-handler regression coverage, with isolated app data.
#ifdef PF_BASELINE_FILE
#include PF_BASELINE_FILE
#else
#include "../src/perfil.c"
#endif
#define main legacy_shot_main
#include "perfil_shot.c"
#undef main

static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=k; perfil_evento(&e);
}
#ifndef PF_BASELINE_FILE
static int focusId(void) {
  return secao*100 + (secao==0?dia:secao==1?item:secao==2?amigo:0);
}
static void navigation(PerfilDados *d) {
  const SDL_Keycode keys[4]={SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_DOWN};
  PfAlvo targets[48];
  perfil_abrir(); perfil_definir_dados(d);
  dia=6-d->primeiroDiaSemana; int saturday=dia; key(SDLK_RIGHT);
  if(!pfRetrato()) assert(secao==1 && "Right from Saturday must reach Most watched, not wrap to Sunday");
  else assert(dia==saturday && "Portrait calendar must not wrap across rows");
  int n=alvos(targets), seen[48]={0}, paths[48][100], lengths[48]={0};
  seen[2]=1; // Opening focus is first day.
  for(int pass=0;pass<n;pass++) for(int from=0;from<n;from++) if(seen[from]==1) {
    seen[from]=2;
    for(int k=0;k<4;k++) {
      perfil_abrir(); perfil_definir_dados(d);
      for(int j=0;j<lengths[from];j++) key(keys[paths[from][j]]);
      assert(focusId()==targets[from].sec*100+targets[from].index);
      GfxRect r=targets[from].r;
      key(keys[k]); int id=focusId(), to=-1;
      for(int j=0;j<n;j++) if(id==targets[j].sec*100+targets[j].index) to=j;
      assert(to>=0);
      if(to==from) continue;
      GfxRect t=targets[to].r;
      float dx=t.x+t.w*.5f-r.x-r.w*.5f, dy=t.y+t.h*.5f-r.y-r.h*.5f;
      assert(k==0?dx<0:k==1?dx>0:k==2?dy<0:dy>0);
      if(!seen[to]) {
        assert(lengths[from]<99);
        memcpy(paths[to],paths[from],sizeof paths[to]);
        lengths[to]=lengths[from]+1; paths[to][lengths[from]]=k; seen[to]=1;
      }
    }
  }
  for(int i=0;i<n;i++) assert(seen[i] && "Every drawn target must be reachable through real key events");
  for(int i=0;i<n;i++) if(targets[i].sec>=PF_SEC_CONQ || targets[i].sec==1) {
    perfil_abrir(); perfil_definir_dados(d);
    for(int j=0;j<lengths[i];j++) key(keys[paths[i][j]]);
    key(SDLK_RETURN);
    if(secao==1) { PerfilDestaque selected; assert(perfil_item_selecionado(&selected)); assert(!strcmp(selected.id,d->destaques[targets[i].index].id)); }
    else if(secao==PF_SEC_CACA) assert(perfil_pediu_caca());
    else if(secao==PF_SEC_CONQ) assert(perfil_pediu_conquistas());
    else if(secao==PF_SEC_TRAVAS) { assert(pagina2); key(SDLK_ESCAPE); assert(!pagina2 && secao==PF_SEC_TRAVAS); }
    else if(targets[i].sec==PF_SEC_COMPARAR) { assert(modo==PF_DUELO); key(SDLK_ESCAPE); assert(modo==PF_RESUMO && secao==PF_SEC_COMPARAR); }
  }
  printf("PASS: %d targets reachable; every arrow follows visual direction; actions and Back retrace\n",n);
}
#endif
static void shot(const char *path,int w,int h) {
  SDL_SetWindowSize(win,w,h); SDL_PumpEvents();
  layout_modo_definir(h>w); layout_tela_definir(w,h);
  glViewport(0,0,w,h); gfx_tamanho_alvo(w,h);
  for(int i=0;i<100;i++) {
    txt_novo_quadro();tex_novo_quadro();tex_bombear(6);gfx_novo_quadro();
    perfil_atualizar(1.0f/60,SDL_GetTicks());
    glClearColor(.051f,.051f,.051f,1); glClear(GL_COLOR_BUFFER_BIT);
    perfil_desenhar(SDL_GetTicks());
    if(i==99) {
      unsigned char *pixels=malloc(w*h*4); assert(pixels);
      glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
      SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32); assert(surface);
      for(int y=0;y<h;y++) memcpy((char*)surface->pixels+y*surface->pitch,pixels+(h-1-y)*w*4,w*4);
      assert(IMG_SavePNG(surface,path)==0); SDL_FreeSurface(surface);free(pixels);
    }
    SDL_GL_SwapWindow(win);SDL_Delay(2);
  }
  printf("PNG %s\n",path);
}
int main(int argc,char **argv) {
  assert(argc==2 && getenv("NUVIO_DADOS"));
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  win=SDL_CreateWindow("Profile verification",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(win);
  SDL_GLContext gl=SDL_GL_CreateContext(win);assert(gl);SDL_GL_SetSwapInterval(0);
  gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(NULL); ajustes_dir(dados_dir()); ajustes_definir_vidro(0);
  PerfilDados d;montar(&d);d.diaHoje=29;
  strcpy(d.avatar,"deploy/app/art/elenco/00_0.jpg");
  strcpy(d.periodo,"SEPTEMBER 2026");
  const char *titles[]={"Fallout","Widow’s Bay","The Prestige","One Battle After Another"};
  for(int i=0;i<4;i++) {
    snprintf(d.destaques[i].poster,sizeof d.destaques[i].poster,"deploy/app/art/poster/%02d.jpg",i);
    snprintf(d.destaques[i].titulo,sizeof d.destaques[i].titulo,"%s",titles[i]);
  }
  socialvis_demo(4); perfil_iniciar();perfil_abrir();perfil_definir_dados(&d);
#ifndef PF_BASELINE_FILE
  navigation(&d);
  for(int offset=0;offset<7;offset++) {
    d.primeiroDiaSemana=offset;d.nDias=28+offset%4;navigation(&d);
  }
  d.primeiroDiaSemana=2;d.nDias=30;
#endif
  char path[1024];
  perfil_abrir();perfil_definir_dados(&d);
  snprintf(path,sizeof path,"%s-tv.png",argv[1]);shot(path,1920,1080);
  snprintf(path,sizeof path,"%s-phone.png",argv[1]);shot(path,540,960);
#ifndef PF_BASELINE_FILE
  navigation(&d);
  secao=PF_SEC_COMPARAR;pfScroll=0;
  snprintf(path,sizeof path,"%s-phone-friends.png",argv[1]);shot(path,540,960);
  layout_modo_definir(0);layout_tela_definir(1920,1080);
  ajustes_definir_vidro(1);perfil_abrir();perfil_definir_dados(&d);
  snprintf(path,sizeof path,"%s-tv-glass.png",argv[1]);shot(path,1920,1080);
  secao=PF_SEC_CONQ;
  snprintf(path,sizeof path,"%s-tv-actions.png",argv[1]);shot(path,1920,1080);
  secao=2;amigo=1;
  snprintf(path,sizeof path,"%s-tv-friends.png",argv[1]);shot(path,1920,1080);
#endif
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();
  return 0;
}
