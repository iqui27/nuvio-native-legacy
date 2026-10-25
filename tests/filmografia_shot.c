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
static Uint32 proof_now;
static void fixture_art(void) {
  FmCredito *c=(FmCredito *)pessoa_creditos();
  for(int i=0;i<pessoa_n_creditos();i++) {
    snprintf(c[i].poster,sizeof c[i].poster,"deploy/app/art/poster/%02d.jpg",i%40);
    tex_obter(c[i].poster);
  }
  snprintf((char *)pessoa_foto(),200,"deploy/app/art/elenco/09_0.jpg");
  j4_art_ready();
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
static void proof_draw(void) { filmo_desenhar(1);ilha_posicionar(1);ilha_desenhar(proof_now); }
static void key(SDL_Keycode k){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;filmo_evento(&e);}
static void shot(const char *prefix,const char *suffix){for(int i=0;i<150;i++){j4_frame(.01f,proof_step,proof_draw);}j4_art_ready();txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();glClear(GL_COLOR_BUFFER_BIT);proof_draw();unsigned char *px=malloc(1920*1080*4);assert(px);glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,px);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,px+(1079-y)*1920*4,1920*4);char path[600];snprintf(path,sizeof path,"%s-%s.png",prefix,suffix);assert(IMG_SavePNG(s,path)==0);puts(path);SDL_FreeSurface(s);free(px);}
int main(int argc,char **argv){assert(argc==2);dados_iniciar(NULL);assert(!strcmp(dados_dir(),getenv("NUVIO_DADOS")));assert(dados_gravar("ajustes.txt","idioma 0\nselected_theme 2\n"));ajustes_dir(dados_dir());SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_Window *win=SDL_CreateWindow("filmo",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(win);assert(SDL_GL_CreateContext(win));GLuint tex,fbo;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(256);char *art=realpath("deploy/app/art",NULL);assert(art);gfx_icones_dir(art);free(art);
 char *json=calloc(1,60000);assert(json);strcpy(json,"{\"name\":\"Mara Valente\",\"known_for_department\":\"Acting\",\"birthday\":\"1972-03-14\",\"place_of_birth\":\"Recife, Brasil\",\"biography\":\"Mara Valente começou no teatro e chegou ao cinema no final dos anos noventa. Entre papéis principais, direção e roteiro, construiu uma carreira que atravessa gêneros e gerações.\",\"also_known_as\":[\"Mara\",\"M. Valente\"],\"combined_credits\":{\"cast\":[");size_t u=strlen(json);for(int i=0;i<46;i++)u+=(size_t)snprintf(json+u,60000-u,"%s{\"id\":%d,\"title\":\"%s\",\"media_type\":\"movie\",\"release_date\":\"%d-06-01\",\"order\":%d,\"character\":\"%s\",\"vote_count\":%d}",i?",":"",i+1,(const char*[]){"O Primeiro Verão","Entre Dois Mundos","A Última Viagem","Casa de Vidro","O Caminho","Cidade Azul"}[i%6],1998+i/2,i%5,i%10==9?"Herself":"Clara",300+i*100);strcpy(json+u,"],\"crew\":[{\"id\":1,\"title\":\"O Primeiro Verão\",\"media_type\":\"movie\",\"release_date\":\"1998-06-01\",\"job\":\"Director\"}]}}");assert(pessoa_ler(123,json,"2026-10-13"));free(json);fixture_art();assert(pessoa_n_creditos()==46);assert(pessoa_creditos()[0].papeis==(FM_ATUA|FM_DIR));assert(strstr(pessoa_outros(),"M. Valente"));filmo_abrir();assert(filmo_foco()==3000);j4_reopen=filmo_abrir;j4_proof("filmography",proof_reset,proof_focus,proof_step,proof_draw,proof_close);for(int sc=85;sc<=100;sc+=15)for(int g=0;g<2;g++){ajustes_definir_vidro(g);gfx_escala_txt_definir(sc/100.f);char pfx[600];snprintf(pfx,sizeof pfx,"%s-%d-g%d",argv[1],sc,g);key(SDLK_ESCAPE);while(filmo_aba()!=0)key(SDLK_LEFT);key(SDLK_DOWN);key(SDLK_DOWN);filmo_abrir();shot(pfx,"historia");key(SDLK_DOWN);key(SDLK_RETURN);assert(filmo_camada()==1);j4_sequence("filmography","sheet-open",proof_step,proof_draw);shot(pfx,"folha");key(SDLK_ESCAPE);j4_sequence("filmography","sheet-close",proof_step,proof_draw);assert(!filmo_camada());key(SDLK_ESCAPE);assert(filmo_foco()==1000);key(SDLK_RIGHT);assert(filmo_aba()==1);shot(pfx,"filmografia");key(SDLK_DOWN);key(SDLK_RETURN);shot(pfx,"filtros");key(SDLK_DOWN);assert(filmo_foco()==4000);shot(pfx,"grade");key(SDLK_ESCAPE);key(SDLK_RIGHT);assert(filmo_aba()==2);shot(pfx,"sobre");key(SDLK_DOWN);key(SDLK_RETURN);assert(filmo_camada()==5);shot(pfx,"biografia");key(SDLK_ESCAPE);key(SDLK_ESCAPE);key(SDLK_RIGHT);assert(filmo_aba()==3);shot(pfx,"conquistas");int remembered=filmo_foco();filmo_abrir();assert(filmo_aba()==3&&filmo_foco()==remembered);}puts("filmografia D-pad: OK");return 0;}
