/* 1080p proof; host decoder is replaced by an explicitly declared still. */
#define main principal_shot_main
#include "guiav5_principal_shot.c"
#undef main

static void frame_cost(void){
  double samples[180];
  for(int i=0;i<210;i++){
    Uint64 start=SDL_GetPerformanceCounter();
    quadro();glFinish();
    if(i>=30)samples[i-30]=1000.0*(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
  }
  for(int i=0;i<180;i++)for(int j=i+1;j<180;j++)if(samples[j]<samples[i]){double v=samples[i];samples[i]=samples[j];samples[j]=v;}
  printf("HOST FRAME COST: median %.3f ms, p95 %.3f ms (draw + glFinish, still fixture, no decoder)\n",samples[90],samples[171]);
}
int main(int argc,char **argv){
  assert(argc==2);mkdir(argv[1],0755);
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *window=SDL_CreateWindow("Guide layout proof",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  assert(window&&SDL_GL_CreateContext(window));SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));tex_cache_dir(getenv("NUVIO_DADOS"));
  dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");
  ajustes_dir(getenv("NUVIO_DADOS"));gfx_escala_txt_definir(ajustes_escala_texto());context();semear();
  snprintf(ltvF[0].nome,sizeof ltvF[0].nome,"Iptb");ltvF[0].nCanais=nCanais;
  char logo[1024];snprintf(logo,sizeof logo,"%s/tests/fixtures/aovivo/logo.png",getenv("GUIALAYOUT_ROOT"));
  canais[0].logo=logo;canais[0].nome="SporTV HD";
  ajustes_definir_guia_previa_grande(1);
  g5ui_open("grade");aberta=1;gv_lin=0;gv_zone=0;gv_snap_force=1;gv_snapshot();
  gv_assistir("c1");CatItem it;assert(guia_pediu_preview(&it));
  float x,y,w,h;guia_preview_rect(&x,&y,&w,&h);player_mini_no_guia(x,y,w,h);player_manter_mini();
  cat_definir(&it,1);player_abrir(0,NULL);player_marcar_canal(&it);
  static const VideoSimulacao sim={.aceita=1,.pronto=1,.tocando=1,.largura=1920,.altura=1080};
  video_simular(&sim);player_definir_fonte("https://example.invalid/live.m3u8");
  capture(window,argv[1],"preview-playing");frame_cost();
#ifndef GUIALAYOUT_BASELINE
  char all[96];snprintf(all,sizeof all,"%s  %d",i18n("Todos ao vivo"),gv_total_n);
  TxtLinha full=txt_linha_bruta(TXT_ILHA_APOIO,all,243,242,239,255);
  float available=gv_source_chip(0,0,0)-10-16;
  TxtLinha fitted=txt_linha_corta_bruta(TXT_ILHA_APOIO,all,243,242,239,255,available);
  assert(full.tex&&fitted.tex==full.tex&&available>=full.w);
#endif
  printf("VISIBLE ROWS: %d; first y %.0f; last bottom %.0f\n",gv_vis(GV_BASE),gv_rows_y(GV_BASE),
         gv_rows_y(GV_BASE)+(gv_vis(GV_BASE)-1)*(GV_ROW_H+GV_ROW_GAP)+GV_ROW_H);
#ifndef GUIALAYOUT_BASELINE
  assert(gv_vis(GV_BASE)>=4);
#endif
#ifndef GUIALAYOUT_BASELINE
  gv_zone=4;gv_chipi=2;gv_activate();
  capture(window,argv[1],"pins-focused");
  guia_preview_rect(&x,&y,&w,&h);
  printf("PREVIEW RECT: %.0f %.0f %.0f %.0f\n",x,y,w,h);
#endif
  player_encerrar();tex_encerrar();SDL_Quit();return 0;
}
