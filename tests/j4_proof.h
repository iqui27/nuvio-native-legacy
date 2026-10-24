// Native renderer evidence; no screenshot interpolation. All data stays offline.
#ifndef NV_J4_PROOF_H
#define NV_J4_PROOF_H
#include <sys/stat.h>
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
static void j4_proof(const char *screen, void (*reset)(void), void (*focus)(void),
                     void (*step)(float), void (*draw)(void)) {
  const char *dir=getenv("J4_PROOF_DIR"); if(!dir)return;
  mkdir(dir,0755); char path[800];
  for(int scale=85;scale<=100;scale+=15) for(int glass=0;glass<2;glass++) {
    ajustes_definir_vidro(glass); gfx_escala_txt_definir(scale/100.f); reset();
    for(int i=0;i<100;i++)j4_frame(.01f,step,draw);
    snprintf(path,sizeof path,"%s/%s-%d-g%d.png",dir,screen,scale,glass); j4_save(path);
  }
  ajustes_definir_vidro(1); gfx_escala_txt_definir(1);
  static const int times[]={0,50,80,100,150,160,250,320,400};
  for(int mode=0;mode<2;mode++) {
    reset();
    if(mode){for(int i=0;i<100;i++)j4_frame(.01f,step,draw);focus();}
    int elapsed=0;
    for(unsigned i=0;i<sizeof times/sizeof *times;i++) {
      while(elapsed<times[i]){int ms=times[i]-elapsed;if(ms>10)ms=10;j4_frame(ms/1000.f,step,draw);elapsed+=ms;}
      if(!times[i])j4_frame(0,step,draw);
      snprintf(path,sizeof path,"%s/%s-%s-%03d.png",dir,screen,mode?"focus":"entry",times[i]);j4_save(path);
    }
  }
  ajustes_definir_vidro(0); gfx_escala_txt_definir(.85f); reset();
}
#endif
