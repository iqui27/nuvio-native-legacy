// Native renderer evidence; no screenshot interpolation. All data stays offline.
#ifndef NV_J4_PROOF_H
#define NV_J4_PROOF_H
#include <sys/stat.h>
#include "ajustes.h"
#include "dados.h"
#include "menu.h"
// Decode real bundled artwork before timing animation; no network fixtures.
static void j4_art_ready(void) {
  for(int i=0;i<80;i++){tex_novo_quadro();tex_bombear(20);SDL_Delay(5);}
}
static void j4_settings(int layout,int fixed,int theme,int reduced) {
  char s[256];
  snprintf(s,sizeof s,"idioma 0\nidiomaAutoLocal 0\nbordaFocoCartaz 1\nhomeLayoutLocal %d\ncollapseSidebar %d\nmodernSidebar %d\nselected_theme %d\nanimacoes %d\n",
    layout,fixed,layout==1?0:1,theme,reduced);
  assert(dados_gravar("ajustes.txt",s)); ajustes_dir(dados_dir());
  assert(ajustes_idioma()==0);
}
static void j4_save(const char *path) {
  unsigned char *p = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
  assert(p && s);
  glFinish(); glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,p);
  for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,p+(1079-y)*1920*4,1920*4);
  assert(!IMG_SavePNG(s,path)); SDL_FreeSurface(s); free(p);
}
static void j4_frame(float dt, void (*step)(float), void (*draw)(void)) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
  glViewport(0,0,1920,1080);
  step(dt); glClear(GL_COLOR_BUFFER_BIT); draw();
}
static void j4_sequence(const char *screen,const char *mode,void (*step)(float),void (*draw)(void)) {
  const char *dir=getenv("J4_PROOF_DIR");if(!dir)return;
  static const int times[]={0,50,80,100,150,160,250,320,400};
  int elapsed=0;char path[800];
  for(unsigned i=0;i<sizeof times/sizeof *times;i++) {
    while(elapsed<times[i]){int ms=times[i]-elapsed;if(ms>10)ms=10;j4_frame(ms/1000.f,step,draw);elapsed+=ms;}
    if(!times[i])j4_frame(0,step,draw);
    snprintf(path,sizeof path,"%s/%s-%s-%03d.png",dir,screen,mode,times[i]);j4_save(path);
  }
}
static void j4_proof(const char *screen, void (*reset)(void), void (*focus)(void),
                     void (*step)(float), void (*draw)(void), void (*close)(void)) {
  const char *dir=getenv("J4_PROOF_DIR"); if(!dir)return;
  mkdir(dir,0755); char path[800];
  for(int scale=85;scale<=100;scale+=15) for(int glass=0;glass<2;glass++) {
    j4_settings(0,0,2,0);
    ajustes_definir_vidro(glass); gfx_escala_txt_definir(scale/100.f); reset();
    for(int i=0;i<100;i++)j4_frame(.01f,step,draw);
    snprintf(path,sizeof path,"%s/%s-%d-g%d.png",dir,screen,scale,glass); j4_save(path);
  }
  for(int scale=85;scale<=100;scale+=15)for(int glass=0;glass<2;glass++) {
    ajustes_definir_vidro(glass);gfx_escala_txt_definir(scale/100.f);
    for(int mode=0;mode<4;mode++) {
      reset();
      if(mode==1){for(int i=0;i<100;i++)j4_frame(.01f,step,draw);focus();}
      if(mode>=2){for(int i=0;i<(mode==2?100:8);i++)j4_frame(.01f,step,draw);close();}
      // Reverse: reopen mid-exit so the strip shows the interrupted trajectory.
      if(mode==3){for(int i=0;i<6;i++)j4_frame(.01f,step,draw);reset();}
      char name[100];snprintf(name,sizeof name,"%s-%d-g%d",screen,scale,glass);
      j4_sequence(name,(const char *[]){"entry","focus","exit","reverse"}[mode],step,draw);
    }
  }
  // Additional scale/sidebar/layout/theme and user-reduced acceptance stills.
  static const int scales[]={90,110,120,130};
  for(int i=0;i<4;i++)for(int layout=0;layout<3;layout++)for(int fixed=0;fixed<2;fixed++) {
    j4_settings(layout,fixed,(i+layout)%2?2:0,0);
    ajustes_definir_vidro(i%2);gfx_escala_txt_definir(scales[i]/100.f);reset();
    for(int k=0;k<100;k++)j4_frame(.01f,step,draw);
    menu_definir_destino(MENU_PERFIL);menu_desenhar(SDL_GetTicks());
    snprintf(path,sizeof path,"%s/%s-%d-layout%d-rail%d.png",dir,screen,scales[i],layout,fixed);j4_save(path);
  }
  j4_settings(0,0,0,1);gfx_escala_txt_definir(1);reset();focus();j4_frame(.01f,step,draw);
  snprintf(path,sizeof path,"%s/%s-reduced.png",dir,screen);j4_save(path);
  j4_settings(0,0,2,0);ajustes_definir_vidro(0);gfx_escala_txt_definir(.85f);reset();
}
#endif
