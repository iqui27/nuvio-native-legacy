// Real rendering and pointer registration; removing safe-area reflow must fail.
#include "filmo.h"
#include "filmodados.h"
#include "pessoa.h"
#include "conquistas.h"
#include "conquistasdados.h"
#include "conquistasregra.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "plrui.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <OpenGL/gl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float tx, ty, transform = 1;
static int failures, controls, targets;
static PonteiroFn achievementActivate;
static int activateId;
static const char *screen;
static void bounds(GfxRect r, const char *kind) {
  float k = gfx_escala();
  r = (GfxRect){(r.x * transform + tx) * k, (r.y * transform + ty) * k,
                r.w * transform * k, r.h * transform * k};
  if (r.x < -.1f || r.y < -.1f || r.w <= 0 || r.h <= 0 ||
      r.x + r.w > 1920.1f || r.y + r.h > 1080.1f) {
    if (failures++ < 20) fprintf(stderr, "FAIL %s %.0f%% %s: %.1f %.1f %.1f %.1f\n",
        screen, gfx_escala_txt() * 100, kind, r.x, r.y, r.w, r.h);
  }
}
void t21_checked_transform(float x, float y, float s, float dx, float dy) {
  transform = s; tx = x * (1-s) + dx; ty = y * (1-s) + dy;
  gfx_transformar(x,y,s,dx,dy);
}
void t21_checked_untransform(void) { transform=1; tx=ty=0; gfx_sem_transformar(); }
void t21_checked_cor(GfxRect r,float radius,float cr,float cg,float cb,float a) {
  bounds(r,"paint"); gfx_cor(r,radius,cr,cg,cb,a);
}
void t21_checked_material(GfxRect r,float radius,int modal,float a) {
  bounds(r,"panel"); plrui_material(r,radius,modal,a);
}
void t21_checked_button(GfxRect r,float a) {
  bounds(r,"control"); controls++; plrui_botao_repouso(r,a);
}
void t21_checked_target(float x,float y,float w,float h,PonteiroFn focus,PonteiroFn activate,int a,int b) {
  // Pointer registration does not inherit gfx group transforms.
  float old=transform, oldx=tx, oldy=ty; transform=1; tx=ty=0;
  bounds((GfxRect){x,y,w,h},"pointer"); targets++;
  transform=old; tx=oldx; ty=oldy;
  if (activate && a==activateId) achievementActivate=activate;
  ponteiro_alvo(x,y,w,h,focus,activate,a,b);
}
static void key(SDL_Keycode k) {
  SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=k; filmo_evento(&e);
}
static void capture(const char *out,const char *name,int achievement,int save) {
  screen=name; controls=targets=0;
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  glClear(GL_COLOR_BUFFER_BIT);
  if (achievement) { conquistas_atualizar(1,SDL_GetTicks()); conquistas_desenhar(SDL_GetTicks()); }
  else filmo_desenhar(1);
  assert(targets>0);
  if (!achievement) assert(controls>0);
  if (!save) return;
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32); assert(s);
  for(int y=0;y<1080;y++) memcpy((char*)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[900]; snprintf(path,sizeof path,"%s/%s%.0f-%s.png",out,achievement?"conquistas":"filmo",gfx_escala_txt()*100,name);
  assert(!IMG_SavePNG(s,path)); puts(path); SDL_FreeSurface(s); free(pixels);
}
int main(int argc,char **argv) {
  assert(argc==2); dados_iniciar(NULL); ajustes_dir(dados_dir());
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1"); assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)); IMG_Init(IMG_INIT_PNG);
  SDL_Window *win=SDL_CreateWindow("tela21",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN); assert(win); assert(SDL_GL_CreateContext(win));
  GLuint tex,fbo; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080); gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  assert(pessoa_ler(123,"{\"name\":\"Mara Valente\",\"biography\":\"A career across cinema and theatre.\",\"combined_credits\":{\"cast\":[{\"id\":1,\"title\":\"First summer\",\"media_type\":\"movie\",\"release_date\":\"1998-06-01\",\"vote_count\":500}]}}","2026-10-13"));
  // Enough credits for two full rows and the year overflow action.
  char json[24000]; size_t used=(size_t)snprintf(json,sizeof json,"{\"name\":\"Mara Valente\",\"biography\":\"A career across cinema and theatre.\",\"combined_credits\":{\"cast\":[");
  for(int i=0;i<36;i++) used+=(size_t)snprintf(json+used,sizeof json-used,"%s{\"id\":%d,\"title\":\"First summer %d\",\"media_type\":\"movie\",\"release_date\":\"1998-06-01\",\"vote_count\":500}",i?",":"",i+1,i+1);
  snprintf(json+used,sizeof json-used,"]}}"); assert(pessoa_ler(123,json,"2026-10-13"));
  CqEstado state={0};
  for(int i=0;i<15;i++) { char id[40]; snprintf(id,sizeof id,"dir:fixture%d",i); cqregra_registrar(&state,id,CQ_FAM_DIRETOR,"Christopher Nolan",2,7,12,1700000000L); }
  cqregra_marcar_avisados(&state); assert(cqd_gravar(&state));
  const float scales[]={.8f,.85f,1,1.15f,1.3f};
  for(int rail=0;rail<2;rail++) for(unsigned i=0;i<sizeof scales/sizeof *scales;i++) {
    assert(dados_gravar("ajustes.txt",rail?"homeLayoutLocal 0\nmodernSidebar 0\ncollapseSidebar 1\n":"homeLayoutLocal 0\nmodernSidebar 1\ncollapseSidebar 0\n"));
    ajustes_dir(dados_dir()); gfx_escala_txt_definir(scales[i]);
    int save=rail&&(i==1||i==2||i==4);
    filmo_abrir(); capture(argv[1],"history",0,save);
    key(SDLK_DOWN); key(SDLK_RETURN); assert(filmo_camada()==1); capture(argv[1],"options",0,save);
    key(SDLK_ESCAPE); key(SDLK_ESCAPE); key(SDLK_RIGHT); capture(argv[1],"page",0,save);
    key(SDLK_DOWN); key(SDLK_DOWN); capture(argv[1],"grid",0,save);
    key(SDLK_ESCAPE); key(SDLK_RIGHT); capture(argv[1],"about",0,save);
    key(SDLK_DOWN); key(SDLK_RETURN); assert(filmo_camada()==5); capture(argv[1],"biography",0,save);
    activateId=2; achievementActivate=NULL;
    conquistas_teste_esquecer(); conquistas_abrir(); capture(argv[1],"hub",1,save);
    assert(achievementActivate); achievementActivate(2,0);
    assert(conquistas_vista()==CONQUISTAS_VISTA_FAMILIAS); capture(argv[1],"families",1,save);
    activateId=100; achievementActivate=NULL;
    conquistas_teste_esquecer(); conquistas_abrir(); capture(argv[1],"hub",1,0);
    assert(achievementActivate); achievementActivate(100,0);
    assert(conquistas_vista()==CONQUISTAS_VISTA_DETALHE); capture(argv[1],"detail",1,save);
  }
  if(failures) { fprintf(stderr,"FAIL tela21 render: %d rectangles overflow\n",failures); return 1; }
  puts("PASS tela21 render: controls, paint, pointers at 80/85/100/115/130%, Modern rail pinned/hidden");
  return 0;
}
