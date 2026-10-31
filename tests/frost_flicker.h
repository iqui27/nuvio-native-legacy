// Live GL reproduction; included by the existing offline Detail fixture.
#ifndef NV_TESTS_FROST_FLICKER_H
#define NV_TESTS_FROST_FLICKER_H
#include "biblioteca.h"
#include "home.h"
#include "menu.h"
#include "addons.h"
#include "fundo.h"
#include <SDL2/SDL_image.h>

#define FF_FRAMES 60
#define FF_BYTES ((size_t)1920 * 1080 * 4)
static unsigned char *ffPixels;
static CatItem ffItems[16];
static CatFileira ffRow;
static int ffDetail;

static void ff_key(SDL_Keycode key) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = key;
  assert(SDL_PushEvent(&e) == 1);
  e.type = SDL_KEYUP; assert(SDL_PushEvent(&e) == 1);
}
static void ff_frame(void) {
  SDL_Event e;
  Uint32 now = SDL_GetTicks();
  while (SDL_PollEvent(&e)) {
    if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) continue;
    if (ffDetail) detail_evento(&e);
    else if (menu_aberto()) menu_evento(&e);
    else home_evento(&e);
  }
  tex_upl_n = 0; tex_upl_bytes = 0;
  tex_bombear(3);
  home_atualizar(1.f/60, now); menu_atualizar(1.f/60, now);
  if (ffDetail) detail_atualizar(1.f/60, now);
  corviva_quadro(1.f/60, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
  txt_novo_quadro(); gfx_novo_quadro(); tex_novo_quadro(); gfx_sem_recorte();
  gfx_ambiente_preparar();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1);
  glClear(GL_COLOR_BUFFER_BIT); gfx_ambiente(1);
  if (!ffDetail || !detail_cobre_tela()) home_desenhar(now);
  if (ffDetail) detail_desenhar(now);
  else menu_desenhar(now);
  gfx_ambiente_descarregar();
}
static void ff_png(const char *name, const unsigned char *pixels) {
  char path[800];
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y*s->pitch, pixels + (size_t)(1079-y)*1920*4, 1920*4);
  snprintf(path, sizeof path, "%s/%s.png", saida, name);
  assert(!IMG_SavePNG(s, path)); SDL_FreeSurface(s);
}
static void ff_pace(Uint64 start) {
  double elapsed = (SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
  if (elapsed < 1000.0/60) SDL_Delay((Uint32)ceil(1000.0/60-elapsed));
}
static int ff_luma(const unsigned char *p) {
  return (54*p[0] + 183*p[1] + 19*p[2] + 128) / 256;
}

// Hold backdrop, foreground and glass geometry fixed; only the cache switches
// from a new-region miss to reuse. Any >2/255 luma jump inside B is a defect.
static int ff_region(const char *label) {
  const GfxRect a = {100,40,300,90}, b = {1100,250,600,600};
  unsigned max = 0; size_t changed = 0; double mean = 0;
  for (int n = -35; n < FF_FRAMES; n++) {
    Uint64 start = SDL_GetPerformanceCounter();
    gfx_novo_quadro(); gfx_sem_recorte();
    if (n == -35 || n == -1) gfx_vidro_sujar();
    glClearColor(.5f,.5f,.5f,1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_cor((GfxRect){0,0,1920,1080},0,.5f,.5f,.5f,1);
    gfx_vidro_folha(a,.3f,1);
    gfx_cor((GfxRect){1000,200,800,800},0,0,1,0,1);
    if (n >= 0) gfx_vidro_painel(b,.06f,.8f,1);
    glFinish();
    if (n >= 0) {
      unsigned char *cur = ffPixels + (size_t)n*FF_BYTES;
      mean += (SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
      glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,cur);
      if (n) for (int y=400; y<600; y++) for (int x=1250; x<1450; x++) {
        size_t i=((size_t)(1079-y)*1920+x)*4;
        unsigned delta=abs(ff_luma(cur+i)-ff_luma(cur-FF_BYTES+i));
        if(delta>max)max=delta;
        changed += delta>2;
      }
    }
    ff_pace(start);
  }
  for(int n=0;n<8;n++) {
    char name[96];snprintf(name,sizeof name,"%s-region-%02d",label,n);
    ff_png(name,ffPixels+(size_t)n*FF_BYTES);
  }
  printf("%s region: frames=60 roi=1250,400,200,200 threshold=2 max_luma_delta=%u changed=%zu render_ms=%.3f\n",
         max<=2?"PASS":"FAIL",max,changed,mean/FF_FRAMES);
  return max>2;
}

// Actual Home/Detail routes, live clock, uploads and focus. These recordings
// contain intentional motion; their full-frame differences are not flicker.
static void ff_scene(const char *label, const char *scene) {
  int uploads=0, sources=0, captures=0;
  double work=0;
  if(!strcmp(scene,"detail")) {
    HomeItem selected={0}; assert(home_item_focado(&selected));
    detail_abrir(&selected); ffDetail=1;
  }
  if(!strcmp(scene,"row")) ff_key(SDLK_DOWN);
  if(!strcmp(scene,"hero")) ff_key(SDLK_RIGHT);
  if(!strcmp(scene,"posters")) for(int i=0;i<16;i++) tex_esquecer(ffItems[i].poster);
  for(int n=0;n<FF_FRAMES;n++) {
    Uint64 start=SDL_GetPerformanceCounter();
    if(!strcmp(scene,"row") && n>=6 && n%6==0)ff_key(SDLK_RIGHT);
    ff_frame();glFinish();
    work+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
    uploads+=tex_upl_n;sources+=gfx_vidro_com_fonte;captures+=gfx_vidro_capturas;
    glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,ffPixels+(size_t)n*FF_BYTES);
    ff_pace(start);
  }
  // Write after recording, so PNG compression cannot perturb animation timing.
  for(int n=0;n<8;n++) {
    char name[96];snprintf(name,sizeof name,"%s-%s-%02d",label,scene,n);
    ff_png(name,ffPixels+(size_t)n*FF_BYTES);
  }
  printf("CAPTURE %s %s: frames=60 fps=60 uploads=%d frost_draws=%d captures=%d render_ms=%.3f\n",
         label,scene,uploads,sources,captures,work/FF_FRAMES);
  fflush(stdout);
}
static int ff_run(void) {
  char label[40];int failures=0;
  snprintf(label,sizeof label,"layout%s",getenv("NUVIO_SHOT_LAYOUT")?getenv("NUVIO_SHOT_LAYOUT"):"0");
  ffPixels=malloc(FF_BYTES*FF_FRAMES);assert(ffPixels);
  ajustes_definir_vidro(1);
  assert(ajustes_shot_valor("relogioTelaLocal",1)); // 1 is off in V_LIGA
  assert(ajustes_shot_valor("vidroQualidadeLocal",2));
  assert(ajustes_shot_valor("fundoLocal",FUNDO_FROST));
  assert(ajustes_shot_valor("trailerHero",1));
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_vidro_qualidade()==2);
  failures+=ff_region(label);
  assert(home_iniciar("deploy/app/art"));
  for(int i=0;i<16;i++) {
    ffItems[i]=itens[1];
    snprintf(ffItems[i].titulo,sizeof ffItems[i].titulo,"Offline movie %d",i+1);
    snprintf(ffItems[i].imdb,sizeof ffItems[i].imdb,"tt%07d",9000000+i);
    snprintf(ffItems[i].poster,sizeof ffItems[i].poster,"deploy/app/art/poster/%02d.jpg",i);
    snprintf(ffItems[i].backdrop,sizeof ffItems[i].backdrop,"deploy/app/art/%02d.jpg",i);
    snprintf(ffItems[i].backdropCatalogo,sizeof ffItems[i].backdropCatalogo,"%s",ffItems[i].backdrop);
    snprintf(ffItems[i].backdropTmdb,sizeof ffItems[i].backdropTmdb,"%s",ffItems[i].backdrop);
    snprintf(ffItems[i].logo,sizeof ffItems[i].logo,"deploy/app/art/logo/%02d.png",i%8);
  }
  snprintf(ffRow.chave,sizeof ffRow.chave,"frost_movies");
  snprintf(ffRow.titulo,sizeof ffRow.titulo,"Offline movies");
  snprintf(ffRow.tipo,sizeof ffRow.tipo,"movie");ffRow.n=16;
  AddonRemoto addon={.nome="Offline",.url="local",.ativo=1};
  assert(addons_definir_lista(&addon,1));
  cat_definir_tudo(ffItems,16,&ffRow,1); menu_iniciar();
  for(int n=0;n<240;n++){ff_frame();SDL_Delay(4);}
  ff_scene(label,"static");ff_scene(label,"hero");ff_scene(label,"row");
  ff_scene(label,"posters");ff_scene(label,"detail");
  // Controls retain their old paths: Performance has no blur; Automatic
  // is deliberately outside this High-only regression assertion.
  for(int quality=0;quality<2;quality++) {
    ajustes_shot_valor("vidroQualidadeLocal",quality);
    gfx_vidro_sujar();
    gfx_novo_quadro();gfx_sem_recorte();
    gfx_cor((GfxRect){0,0,1920,1080},0,.5f,.5f,.5f,1);
    gfx_vidro_folha((GfxRect){100,100,400,200},.1f,1);
    int ok=quality ? gfx_vidro_com_fonte==0 && gfx_vidro_chapados>0 : gfx_vidro_com_fonte>0;
    printf("%s control quality=%d frost=%d flat=%d\n",ok?"PASS":"FAIL",quality,gfx_vidro_com_fonte,gfx_vidro_chapados);
    failures+=!ok;
  }
  tex_encerrar();txt_encerrar();gfx_encerrar();
  SDL_DestroyWindow(janela);SDL_Quit();
  free(ffPixels);return failures?1:0;
}
#endif
