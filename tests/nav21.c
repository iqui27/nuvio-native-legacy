// Offline host gate. Expectations use rendered rectangles; input uses real SDL handlers.
#include "nav21.h"
#include "dados.h"
#include "ajustes.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Nav21Target targets[NAV21_MAX];
static int count, failures, checks;
static const char *filter;
static FILE *logfile, *registry;
static const char *artifactDir;
void nav21_aio(void);
void nav21_library(void);
void nav21_filmo(void);
void nav21_addonguide(void);
void nav21_detail(void);
void nav21_social(void);
void nav21_settings(void);
void nav21_sources(void);
void nav21_juntos(void);
void nav21_home(void);
void nav21_offline(void);
static const SDL_Keycode keys[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT};
static const char *names[]={"Up","Down","Left","Right"};
void nav21_target(int id,GfxRect r) {
  if(count==NAV21_MAX) { fprintf(stderr,"target overflow\n"); exit(2); }
  targets[count++]=(Nav21Target){id,r};
}
void nav21_expect(const char *screen,const char *path,int actual,int expected) {
  checks++;
  if(actual!=expected) { failures++; fprintf(logfile,"| %s | %s | %d | %d |\n",screen,path,actual,expected); }
}
static void key(Nav21Screen s,SDL_Keycode code) {
  SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=code; s.key(&e);
  e.type=SDL_KEYUP; s.key(&e);
}
static void frame(Nav21Screen s,float dt) {
  s.step(dt); count=0; txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  glClearColor(.01f,.01f,.01f,1); glClear(GL_COLOR_BUFFER_BIT); s.draw();
}
static void settle(Nav21Screen s) { for(int i=0;i<90;i++) s.step(1.f/60); for(int i=0;i<4;i++)frame(s,.1f); }
static int nearest(Nav21Target *v,int n,int from,int direction) {
  int best=from; float distance=INFINITY;
  for(int i=0;i<n;i++) {
    float x=v[i].r.x+v[i].r.w/2-v[from].r.x-v[from].r.w/2;
    float y=v[i].r.y+v[i].r.h/2-v[from].r.y-v[from].r.h/2;
    float along=direction==0?-y:direction==1?y:direction==2?-x:x;
    if(along<1) continue;
    // Directional TV distance: penalize movement across the requested axis.
    float d=along+3*fabsf(direction<2?x:y);
    if(d<distance) {distance=d;best=i;}
  }
  return best;
}
static unsigned char pixels[1920*1080*4];
static unsigned long long hash(void) {
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  unsigned long long h=1469598103934665603ULL;
  for(size_t i=0;i<sizeof pixels;i+=16) h=(h^pixels[i])*1099511628211ULL;
  return h;
}
static void motion(Nav21Screen s,int closing) {
  s.open(); settle(s);
  if(closing) key(s,SDLK_ESCAPE); else s.open();
  frame(s,0); unsigned long long first=hash(), last=first;
  int changed=0, firstChange=-1, lastChange=-1;
  for(int ms=25;ms<=600;ms+=25) {
    SDL_Delay(25);frame(s,.025f); unsigned long long h=hash();
    if(h!=last) {changed++;if(firstChange<0)firstChange=ms;lastChange=ms;} last=h;
  }
  nav21_expect(s.name,closing?"Back: exit frames change through >=150ms":"Open: entry frames change through >=150ms",changed>1&&lastChange-firstChange>=150,1);
}
void nav21_check(Nav21Screen s) {
  if(filter && !strstr(s.name,filter))return;
  int before=failures; s.open(); settle(s);
  nav21_expect(s.name,"Open: primary focus",s.focus(),s.primary);
  Nav21Target base[NAV21_MAX]; int n=count; memcpy(base,targets,n*sizeof *base);
  nav21_expect(s.name,"Open: nonempty focus registry",n>0,1);
  for(int i=0;i<n;i++)fprintf(registry,"%s\t%d\t%.1f\t%.1f\t%.1f\t%.1f\n",s.name,base[i].id,base[i].r.x,base[i].r.y,base[i].r.w,base[i].r.h);
  if(!strcmp(s.name,"Library + Origins")||!strcmp(s.name,"Settings/UX")){
    hash();
    SDL_Surface *upright=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
    for(int y=0;y<1080;y++)memcpy((char*)upright->pixels+y*upright->pitch,pixels+(1079-y)*1920*4,1920*4);
    char path[1024];snprintf(path,sizeof path,"%s/%s.png",artifactDir,s.name[0]=='L'?"library":"settings");
    if(IMG_SavePNG(upright,path))exit(2);SDL_FreeSurface(upright);
  }
  int edges[NAV21_MAX][4], expectedEdges[NAV21_MAX][4], actualEdges[NAV21_MAX][4]; memset(edges,-1,sizeof edges);memset(expectedEdges,-1,sizeof expectedEdges);
  int initial=-1;
  for(int i=0;i<n;i++) if(base[i].id==s.focus())initial=i;
  for(int i=0;i<n;i++) for(int d=0;d<4;d++) {
    s.open();s.select(base[i].id);settle(s);
    int from=-1;for(int j=0;j<count;j++)if(targets[j].id==base[i].id)from=j;
    if(from<0) { nav21_expect(s.name,"focus absent from rendered targets",base[i].id,-1);continue; }
    int expected=targets[nearest(targets,count,from,d)].id;
    key(s,keys[d]); int actual=s.focus();
    actualEdges[i][d]=actual;
    if(expected!=base[i].id)expectedEdges[i][d]=expected;
    for(int j=0;j<n;j++)if(base[j].id==actual)edges[i][d]=j;
  }
  int seen[NAV21_MAX]={0},parent[NAV21_MAX],via[NAV21_MAX];memset(parent,-1,sizeof parent);
  if(initial>=0)seen[initial]=1;
  for(int pass=0;pass<n;pass++)for(int i=0;i<n;i++)if(seen[i])for(int d=0;d<4;d++){int j=edges[i][d];if(j>=0&&!seen[j]){seen[j]=1;parent[j]=i;via[j]=d;}}
  for(int i=0;i<n;i++){
    int reverse[NAV21_MAX],depth=0;for(int j=i;parent[j]>=0&&depth<n;j=parent[j])reverse[depth++]=via[j];
    char path[4096]="Open";
    for(int j=depth-1;j>=0;j--){size_t used=strlen(path);snprintf(path+used,sizeof path-used,", %s",names[reverse[j]]);}
    if(seen[i]){s.open();settle(s);for(int j=depth-1;j>=0;j--){key(s,keys[reverse[j]]);settle(s);}nav21_expect(s.name,path,s.focus(),base[i].id);}
    else {char missing[120];snprintf(missing,sizeof missing,"Open: no D-pad path to %d",base[i].id);nav21_expect(s.name,missing,0,1);}
    for(int d=0;d<4;d++)if(expectedEdges[i][d]>=0){char edge[4300];snprintf(edge,sizeof edge,"%s; from %d, %s: went to %d, expected %d",path,base[i].id,names[d],actualEdges[i][d],expectedEdges[i][d]);nav21_expect(s.name,edge,actualEdges[i][d],expectedEdges[i][d]);}
  }
  motion(s,0);if(strcmp(s.name,"Home 2.1"))motion(s,1);
  if(strcmp(s.name,"Home 2.1")){s.open();settle(s);int previous=s.page();key(s,SDLK_ESCAPE);settle(s);nav21_expect(s.name,"Open, Back: exactly one page changes",s.page()!=previous,1);}
  printf("SCREEN %s targets=%d violations=%d\n",s.name,n,failures-before);fflush(stdout);
}
int main(int argc,char **argv) {
  assert(argc==2); filter=getenv("NAV21_SCREEN"); artifactDir=getenv("NAV21_OUT");assert(artifactDir);
  char targetPath[1024];snprintf(targetPath,sizeof targetPath,"%s/targets.tsv",artifactDir);registry=fopen(targetPath,"w");assert(registry);
  logfile=fopen(argv[1],"w");assert(logfile);
  fprintf(logfile,"| Screen | Exact key path / check | Actual | Expected |\n|---|---|---:|---:|\n");
  nav21_offline();
  assert(getenv("NUVIO_DADOS"));dados_iniciar(getenv("NUVIO_DADOS"));ajustes_dir(dados_dir());
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG);
  SDL_Window *w=SDL_CreateWindow("nav21",0,0,64,64,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);assert(w);assert(SDL_GL_CreateContext(w));
  GLuint t,f;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");gfx_escala_txt_definir(1);
  nav21_servers();nav21_achievements();nav21_start();nav21_aio();nav21_library();nav21_filmo();nav21_addonguide();nav21_detail();nav21_social();nav21_settings();nav21_sources();nav21_juntos();nav21_home();
  fprintf(logfile,"\n%d checks, %d violations.\n",checks,failures);fclose(logfile);fclose(registry);
  printf("nav21: %d checks, %d violations\n",checks,failures);return failures?1:0;
}
