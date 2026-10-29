/* Native guide-options proof: real rendering, persisted settings and remote input. */
#define main principal_shots_main
#include "guiav5_principal_shot.c"
#undef main

static int shot_w, shot_h;
static void options_frame(SDL_Window *window, const char *dir, const char *name) {
  txt_novo_quadro();tex_novo_quadro();tex_bombear(8);gfx_novo_quadro();
  glClearColor(.045f,.047f,.051f,1);glClear(GL_COLOR_BUFFER_BIT);
  g5ui_draw();
  if(name){
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,shot_w,shot_h,32,SDL_PIXELFORMAT_RGBA32);
    unsigned char *p=malloc(shot_w*shot_h*4);assert(s&&p);
    glReadPixels(0,0,shot_w,shot_h,GL_RGBA,GL_UNSIGNED_BYTE,p);
    for(int y=0;y<shot_h;y++)memcpy((char *)s->pixels+y*s->pitch,p+(shot_h-1-y)*shot_w*4,shot_w*4);
    char file[1024];snprintf(file,sizeof file,"%s/%s.png",dir,name);assert(!IMG_SavePNG(s,file));
    SDL_FreeSurface(s);free(p);
  }
  SDL_GL_SwapWindow(window);SDL_PumpEvents();
}
static void settle(SDL_Window *w){Uint32 until=SDL_GetTicks()+380;do{options_frame(w,NULL,NULL);SDL_Delay(8);}while(SDL_GetTicks()<until);}
static void key(SDL_Keycode k){SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=k;assert(g5ui_event(&e));e.type=SDL_KEYUP;g5ui_event(&e);}
int main(int argc,char **argv){
  assert(argc==4);const char *dir=argv[1];int phone=atoi(argv[2]),baseline=atoi(argv[3]);
  shot_w=phone?390:1920;shot_h=phone?844:1080;
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Guide options",0,0,shot_w,shot_h,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));
  SDL_GL_SetSwapInterval(0);layout_tela_definir(shot_w,shot_h);layout_modo_definir(phone);
  glViewport(0,0,shot_w,shot_h);gfx_tamanho_alvo(shot_w,shot_h);
  assert(gfx_iniciar()&&txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");
  ajustes_dir(getenv("NUVIO_DADOS"));layout_modo_definir(phone);gfx_tamanho_alvo(shot_w,shot_h);
  gfx_escala_txt_definir(1);ajustes_definir_vidro(1);context();semear();
  ajustes_definir_guia_previa_grande(1);ajustes_definir_guia_dias(3);
  g5ui_open("grade");gv_zone=5;focus=1105;settle(w);key(SDLK_RETURN);
  assert(!strcmp(screen,"ajustes"));
  Uint32 start=SDL_GetTicks();const unsigned times[]={0,80,160,320};
  for(int i=0;i<4;i++){
    while(SDL_GetTicks()-start<times[i]){options_frame(w,NULL,NULL);SDL_Delay(2);}
    char name[32];snprintf(name,sizeof name,"opening-%d",i);options_frame(w,dir,name);
  }
  settle(w);options_frame(w,dir,"open");
  key(SDLK_DOWN);settle(w);options_frame(w,dir,"picker-focused");
  if(!baseline){
    assert(focus==1942);key(SDLK_RETURN);settle(w);
    assert(!strcmp(screen,"ajustes")&&focus==1942);
    options_frame(w,dir,"submenu");key(SDLK_UP);assert(focus==1941);
    key(SDLK_LEFT);assert(focus==1941&&!gv_side_open);key(SDLK_RIGHT);assert(focus==1941);
    key(SDLK_RETURN);assert(!ajustes_guia_previa_grande());settle(w);
    key(SDLK_ESCAPE);assert(!strcmp(screen,"ajustes")&&focus==1942);settle(w);
    key(SDLK_DOWN);assert(focus==1944);key(SDLK_DOWN);assert(focus==1945);settle(w);
    int muted=ajustes_guia_previa_sem_som();key(SDLK_RETURN);settle(w);assert(ajustes_guia_previa_sem_som()!=muted);
    options_frame(w,dir,"toggle-focused");key(SDLK_RETURN);settle(w);
    key(SDLK_DOWN);assert(focus==1947);key(SDLK_RETURN);settle(w);key(SDLK_UP);key(SDLK_RETURN);settle(w);assert(ajustes_guia_dias()==1);
    key(SDLK_ESCAPE);settle(w);key(SDLK_DOWN);assert(focus==1948);
    key(SDLK_RETURN);settle(w);assert(!ajustes_vidro());options_frame(w,dir,"solid");
    key(SDLK_RETURN);settle(w);key(SDLK_DOWN);assert(focus==1949);key(SDLK_DOWN);assert(focus==1937);
    key(SDLK_RETURN);settle(w);options_frame(w,dir,"sources-submenu");assert(!strcmp(screen,"ajustes"));
    key(SDLK_ESCAPE);assert(focus==1937);settle(w);
    key(SDLK_ESCAPE);assert(!strcmp(screen,"grade")&&focus==1105);settle(w);
    key(SDLK_RETURN);settle(w);assert(focus==1940);
    for(int i=0;i<9;i++){key(SDLK_DOWN);settle(w);}assert(focus==1939);
    key(SDLK_DOWN);assert(focus==1939);key(SDLK_RIGHT);assert(focus==1939);key(SDLK_LEFT);assert(focus==1939&&!gv_side_open);
    for(int i=0;i<9;i++){key(SDLK_UP);settle(w);}assert(focus==1940);
    key(SDLK_UP);assert(focus==1936);key(SDLK_RETURN);settle(w);assert(!strcmp(screen,"grade")&&focus==1105);
    puts("PASS: options open, spatial movement, picker selection/Back, toggle, material, sources, close and reopen");
  }else{key(SDLK_DOWN);settle(w);options_frame(w,dir,"submenu");focus=1945;settle(w);options_frame(w,dir,"toggle-focused");}
  SDL_Quit();return 0;
}
