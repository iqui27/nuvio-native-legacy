/* Deterministic guide proof. Host video uses a declared still behind the GL hole. */
#define main guide_principal_main
#include "guiav5_principal_shot.c"
#undef main

int main(int argc,char **argv){
  assert(argc==2);
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *window=SDL_CreateWindow("Guide card proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  assert(window&&SDL_GL_CreateContext(window));SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));tex_cache_dir(getenv("NUVIO_DADOS"));
  dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");
  ajustes_dir(getenv("NUVIO_DADOS"));gfx_escala_txt_definir(ajustes_escala_texto());context();semear();
  canais[0].logo=getenv("GUIACARD_LOGO_URL");assert(canais[0].logo&&strstr(canais[0].logo,"http://127.0.0.1:"));
  canais[0].nome="SporTV HD";canais[1].nome="News without logo";
  g5ui_open("grade");aberta=1;gv_lin=0;gv_zone=0;gv_snap_force=1;gv_snapshot();
  gv_assistir("c1");CatItem it;assert(guia_pediu_preview(&it));
  float x,y,w,h;guia_preview_rect(&x,&y,&w,&h);player_mini_no_guia(x,y,w,h);player_manter_mini();
  cat_definir(&it,1);player_abrir(0,NULL);player_marcar_canal(&it);
  static const VideoSimulacao sim={.aceita=1,.pronto=1,.tocando=1,.largura=1920,.altura=1080};
  video_simular(&sim);player_definir_fonte("https://example.invalid/live.m3u8");
  gv_lin=1;gv_snap_force=1;gv_snapshot();capture(window,argv[1],"preview-playing");
  assert(tex_obter_larg(canais[0].logo,52)&&tex_aspecto(canais[0].logo)>0);
  puts("PASS: channel logo URL downloaded, decoded and uploaded through texture cache");
  gv_zone=1;capture(window,argv[1],"pins-focused");
  player_encerrar();tex_encerrar();SDL_Quit();return 0;
}
