#include "filmo.h"
#include "filmodados.h"
#include "pessoa.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenGL/gl.h>
#include "ilha.h"
#include "j4_proof.h"
#include "js.h"
#include "idioma.h"
// Offline adapter: no enrichment, follow, or episode worker can mutate proof state.
static FdFicha fixture;
void fd_abrir(void) {}
void fd_atualizar(void) {}
const FdFicha *fd_ficha(void) { return &fixture; }
void fd_enriquecer(int i) { (void)i; }
int fd_preparado(int i) { return i >= 0 && i < fixture.n; }
int fd_quiz(int i,FmQuiz *q) { (void)i;(void)q;return 0; }
int fd_tentativas(int i) { (void)i;return 2; }
void fd_falhou(int i) { (void)i; }
int fd_marcar(int i,int como,int release,int remover) { (void)i;(void)como;(void)release;(void)remover;return 0; }
void fd_seguir(void) { fixture.seguindo=!fixture.seguindo; }
void fd_sobre(void) {}
const char *fd_aviso(void) { return ""; }
static Uint32 proof_now;
static void load_fixture(void) {
  FILE *f=fopen(getenv("FILMO_FIXTURE"),"rb");assert(f);
  assert(!fseek(f,0,SEEK_END));long n=ftell(f);rewind(f);
  char *json=calloc(1,(size_t)n+1);assert(json);assert(fread(json,1,(size_t)n,f)==(size_t)n);fclose(f);
  assert(pessoa_ler(1204418,json,"2026-10-13"));
  fixture.pessoa=pessoa_id();fixture.n=pessoa_n_creditos();fixture.pronto=1;
  memcpy(fixture.c,pessoa_creditos(),fixture.n*sizeof *fixture.c);
  const char *p=js_array(json,NULL,"entries");
  while(p){const char *end=js_fim(p);int id=(int)js_num(p,end,"id",0),i=0;
    while(i<fixture.n&&fixture.c[i].tmdb!=id)i++;assert(i<fixture.n);
    FmCredito *c=fixture.c+i;FmEstado *s=fixture.st+i;
    c->ruido=(int)js_num(p,end,"noise",0);c->eps=(int)js_num(p,end,"eps",0);c->epsTotal=(int)js_num(p,end,"epsTotal",0);
    char state[24]="";js_texto(p,end,"state",state,sizeof state);
    s->estado=c->proximo?FM_EM_BREVE:!strcmp(state,"seen")?FM_VISTO:!strcmp(state,"progress")?FM_ANDA:FM_NAO;
    s->pct=s->estado==FM_VISTO?100:(int)js_num(p,end,"pct",0);s->epsVistos=c->eps*s->pct/100;
    p=js_prox(end);
  }
  p=js_array(json,NULL,"awards");
  while(p&&fixture.np<FD_LINHAS){const char *end=js_fim(p);FdLinha *a=fixture.premios+fixture.np++;
    js_texto(p,end,"name",a->nome,sizeof a->nome);a->ano=(int)js_num(p,end,"year",0);snprintf(a->rel,sizeof a->rel,"%s",strstr(a->nome,"indicação")?"Indicação":"Prêmio");p=js_prox(end);}
  FmResumo sum;fm_resumo(fixture.c,fixture.st,fixture.n,&sum);assert(sum.visto==13&&sum.andamento==1&&sum.total==41);
  free(json);
}
static void fixture_art(void) {
  FmCredito *c=(FmCredito *)pessoa_creditos();
  for(int i=0;i<pessoa_n_creditos();i++) {
    snprintf(c[i].poster,sizeof c[i].poster,"deploy/app/art/poster/%02d.jpg",i%40);
    snprintf(fixture.c[i].poster,sizeof fixture.c[i].poster,"%s",c[i].poster);
    tex_obter(c[i].poster);
  }
  // This fictional person has no portrait: hero uses the selected rose accent.
  int ready=0;
  for(int attempt=0;attempt<20&&!ready;attempt++){
    j4_art_ready();ready=1;
    for(int i=0;i<fixture.n;i++)ready&=tex_obter(fixture.c[i].poster)!=0;
  }
  assert(ready);
}
static void proof_reset(void) {
  filmo_abrir();
  SDL_Event e={0};e.type=SDL_KEYDOWN;
  if(filmo_foco()/1000!=1){e.key.keysym.sym=SDLK_ESCAPE;filmo_evento(&e);}
  while(filmo_aba()!=1){e.key.keysym.sym=SDLK_RIGHT;filmo_evento(&e);}
  e.key.keysym.sym=SDLK_DOWN;filmo_evento(&e);filmo_evento(&e);
  assert(filmo_foco()/1000==4);
}
static void proof_focus(void) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RIGHT;filmo_evento(&e); }
static void proof_close(void) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;
  filmo_evento(&e);filmo_evento(&e);
}
static void proof_step(float dt) { proof_now+=(Uint32)(dt*1000); filmo_atualizar(dt); }
static void proof_draw(void) { ilha_relogio_visivel(1);ilha_posicionar(1);filmo_desenhar(1);ilha_desenhar(proof_now); }
static void key(SDL_Keycode k){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;filmo_evento(&e);}
static void motion_strip(const char *prefix,const char *mode) {
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,7680,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
  unsigned char *px=malloc(1920*1080*4),*before=malloc(1920*1080*4);assert(px&&before);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,before);
  const int times[]={0,80,160,320};int elapsed=0;
  for(int i=0;i<4;i++){
    while(elapsed<times[i]){j4_frame(.01f,proof_step,proof_draw);elapsed+=10;}
    if(!i)j4_frame(0,proof_step,proof_draw);
    glFinish();glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,px);
    // Year changes retain the outgoing chapter at t=0, including its artwork.
    if(!strcmp(mode,"focus")&&!i){long delta=0;
      for(int y=130;y<440;y++)for(int x=100;x<1100;x++)for(int c=0;c<3;c++)
        delta+=abs((int)px[(y*1920+x)*4+c]-(int)before[(y*1920+x)*4+c]);
      assert(delta<310L*1000*3);
    }
    for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch+i*1920*4,px+(1079-y)*1920*4,1920*4);
  }
  char path[700];snprintf(path,sizeof path,"%s-%s-motion.png",prefix,mode);assert(!IMG_SavePNG(s,path));
  puts(path);free(before);free(px);SDL_FreeSurface(s);
}
static void shot(const char *prefix,const char *suffix){for(int i=0;i<150;i++){j4_frame(.01f,proof_step,proof_draw);}j4_art_ready();txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();glClear(GL_COLOR_BUFFER_BIT);proof_draw();unsigned char *px=malloc(1920*1080*4);assert(px);glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,px);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,px+(1079-y)*1920*4,1920*4);char path[600];snprintf(path,sizeof path,"%s-%s.png",prefix,suffix);assert(IMG_SavePNG(s,path)==0);puts(path);SDL_FreeSurface(s);free(px);}
int main(int argc,char **argv){assert(argc==2||argc==3);dados_iniciar(NULL);assert(!strcmp(dados_dir(),getenv("NUVIO_DADOS")));assert(dados_gravar("ajustes.txt","idioma 0\nselected_theme 2\n"));ajustes_dir(dados_dir());SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_Window *win=SDL_CreateWindow("filmo",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(win);assert(SDL_GL_CreateContext(win));GLuint tex,fbo;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(256);char *art=realpath("deploy/app/art",NULL);assert(art);gfx_icones_dir(art);free(art);
 load_fixture();fixture_art();assert(pessoa_n_creditos()==46);assert(strstr(pessoa_outros(),"Mara Lindquist"));filmo_abrir();assert(filmo_foco()==3000);
 if(argc==3&&!strcmp(argv[2],"history")){
   for(int sc=85;sc<=100;sc+=15)for(int g=0;g<2;g++){
     j4_settings(0,0,6,0);ajustes_definir_vidro(g);gfx_escala_txt_definir(sc/100.f);
     key(SDLK_ESCAPE);while(filmo_aba()!=0)key(SDLK_LEFT);key(SDLK_DOWN);key(SDLK_DOWN);filmo_abrir();
     for(int yr=0;yr<30;yr++)key(SDLK_LEFT);for(int yr=2011;yr<2024;yr++)key(SDLK_RIGHT);
     char pfx[600];snprintf(pfx,sizeof pfx,"%s-%d-g%d",argv[1],sc,g);
     shot(pfx,"historia");key(SDLK_RIGHT);motion_strip(pfx,"focus");key(SDLK_LEFT);
     for(int i=0;i<150;i++)j4_frame(.01f,proof_step,proof_draw);
     key(SDLK_DOWN);key(SDLK_RETURN);assert(filmo_camada()==1);shot(pfx,"folha");key(SDLK_ESCAPE);
   }
   puts("History geometry, glass and year retention: PASS");return 0;
 }
 j4_reopen=filmo_abrir;j4_proof("filmography",proof_reset,proof_focus,proof_step,proof_draw,proof_close);for(int sc=85;sc<=100;sc+=15)for(int g=0;g<2;g++){j4_settings(0,0,6,0);ajustes_definir_vidro(g);gfx_escala_txt_definir(sc/100.f);char pfx[600];snprintf(pfx,sizeof pfx,"%s-%d-g%d",argv[1],sc,g);key(SDLK_ESCAPE);while(filmo_aba()!=0)key(SDLK_LEFT);key(SDLK_DOWN);key(SDLK_DOWN);filmo_abrir();for(int yr=0;yr<30;yr++)key(SDLK_LEFT);for(int yr=2011;yr<2024;yr++)key(SDLK_RIGHT);motion_strip(pfx,"entry");shot(pfx,"historia");key(SDLK_RIGHT);motion_strip(pfx,"focus");key(SDLK_LEFT);key(SDLK_PAGEUP);key(SDLK_PAGEDOWN);assert(filmo_foco()==3000);key(SDLK_DOWN);key(SDLK_RETURN);assert(filmo_camada()==1);motion_strip(pfx,"sheet-open");shot(pfx,"folha");key(SDLK_ESCAPE);motion_strip(pfx,"sheet-close");assert(!filmo_camada());key(SDLK_ESCAPE);assert(filmo_foco()==1000);key(SDLK_RIGHT);assert(filmo_aba()==1);shot(pfx,"filmografia");key(SDLK_DOWN);key(SDLK_RETURN);shot(pfx,"filtros");key(SDLK_DOWN);assert(filmo_foco()==4000);shot(pfx,"grade");key(SDLK_ESCAPE);key(SDLK_RIGHT);assert(filmo_aba()==2);shot(pfx,"sobre");key(SDLK_DOWN);key(SDLK_RETURN);assert(filmo_camada()==5);shot(pfx,"biografia");key(SDLK_ESCAPE);key(SDLK_ESCAPE);key(SDLK_RIGHT);assert(filmo_aba()==3);shot(pfx,"conquistas");int remembered=filmo_foco();filmo_abrir();assert(filmo_aba()==3&&filmo_foco()==remembered);}puts("filmografia D-pad: OK");return 0;}
