#include "video.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static SDL_Window *window;
static void frame(void) {
  SDL_PumpEvents();
  video_bombear();
  glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
  video_desktop_render(320,180);
  SDL_GL_SwapWindow(window); video_desktop_swap();
  SDL_Delay(10);
}
static void waitReady(void) {
  Uint64 until=SDL_GetTicks64()+10000;
  while ((!video_pronto() || video_n_audio()!=2 || video_largura()!=320) && SDL_GetTicks64()<until) frame();
  if (video_falhou()) fprintf(stderr,"%s\n",video_erro_texto());
  assert(video_pronto() && !video_falhou());
  assert(video_n_audio()==2 && video_n_legenda()>=1);
  assert(video_largura()==320 && video_altura()==180);
}
static void waitTrack(int audio, int sub) {
  Uint64 until=SDL_GetTicks64()+3000;
  while ((video_audio_atual()!=audio || video_legenda_atual()!=sub) && SDL_GetTicks64()<until) frame();
  assert(video_audio_atual()==audio && video_legenda_atual()==sub);
}
int main(int argc, char **argv) {
  assert(argc==3);
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
  window=SDL_CreateWindow("mpv smoke",0,0,320,180,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context=SDL_GL_CreateContext(window); assert(context);
  assert(video_iniciar());
  video_volume(0);
  video_definir_cabecalhos("Referer: https://example.test/\nX-Example: a,b\n");
  assert(video_tocar(argv[1]));
  waitReady();
  video_pausar(1);
  Uint64 until=SDL_GetTicks64()+3000;
  while (!video_pausa_confirmada() && SDL_GetTicks64()<until) frame();
  assert(video_pausa_confirmada());
  video_escolher_audio(1); video_escolher_legenda(0); waitTrack(1,0);
  assert(!strcmp(video_audio(1)->idioma,"por"));
  video_buscar(3);
  until=SDL_GetTicks64()+3000;
  while (video_pos()<2.9 && SDL_GetTicks64()<until) frame();
  assert(video_pos()>=2.9 && video_pos()<3.3);
  // time-pos can precede the paused seek's rendered subtitle frame.
  for (int i=0;i<30;++i) frame();
  Uint64 sample=0; video_pos_amostra(&sample); assert(sample);
  assert(video_duracao()>5.9 && video_fonte_tocou());
  video_janela(0,0,1920,1080);
  glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
  PFNGLVERTEXATTRIBPOINTERPROC attrib=(PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
  PFNGLENABLEVERTEXATTRIBARRAYPROC enable=(PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
  PFNGLGETVERTEXATTRIBIVPROC get=(PFNGLGETVERTEXATTRIBIVPROC)SDL_GL_GetProcAddress("glGetVertexAttribiv");
  float vertices[8]={0}; attrib(0,2,GL_FLOAT,GL_FALSE,0,vertices); enable(0);
  assert(video_desktop_render(320,180));
  GLint enabled=0,size=0; get(0,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&enabled); get(0,GL_VERTEX_ATTRIB_ARRAY_SIZE,&size);
  assert(enabled && size==2);
  unsigned char top[4],bottom[4];
  glReadPixels(20,160,1,1,GL_RGBA,GL_UNSIGNED_BYTE,top);
  glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,bottom);
  assert(top[0]>150 && top[1]<80); // red top, blue bottom: orientation matters
  assert(bottom[2]>150 && bottom[0]<80);
  unsigned char *withSub=malloc(320*180*4), *withoutSub=malloc(320*180*4);
  assert(withSub && withoutSub);
  glReadPixels(0,0,320,180,GL_RGBA,GL_UNSIGNED_BYTE,withSub);
  video_escolher_legenda(-1); waitTrack(1,-1);
  for (int i=0;i<10;++i) frame();
  glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); video_desktop_render(320,180);
  glReadPixels(0,0,320,180,GL_RGBA,GL_UNSIGNED_BYTE,withoutSub);
  assert(memcmp(withSub,withoutSub,320*180*4)!=0); // actual ASS/subtitle pixels
  free(withSub); free(withoutSub);
  // Opaque UI must survive; alpha 0.5 green veil composites over red video.
  glClearColor(0,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
  video_desktop_render(320,180);
  glReadPixels(20,160,1,1,GL_RGBA,GL_UNSIGNED_BYTE,top);
  assert(top[0]<5 && top[1]>250);
  glClearColor(0,0.5f,0,0.5f); glClear(GL_COLOR_BUFFER_BIT);
  video_desktop_render(320,180);
  glReadPixels(20,160,1,1,GL_RGBA,GL_UNSIGNED_BYTE,top);
  assert(top[0]>75 && top[0]<160 && top[1]>110 && top[1]<145);
  video_legenda_externa(argv[2]);
  until=SDL_GetTicks64()+3000;
  while (video_n_legenda()!=2 && SDL_GetTicks64()<until) frame();
  assert(video_n_legenda()==2);
  video_janela_fonte(0,0,320,90,0,0,1920,1080);
  glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); video_desktop_render(320,180);
  glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,bottom);
  assert(bottom[0]>150 && bottom[2]<80); // source crop fills destination
  video_velocidade(150); assert(video_velocidade_atual()==150);
  video_buscar(5.7); video_pausar(0);
  until=SDL_GetTicks64()+5000;
  while (!video_terminou() && SDL_GetTicks64()<until) frame();
  assert(video_terminou() && !video_falhou());
  video_parar(); assert(!video_ativo() && !*video_url_atual());
  assert(video_tocar("/nonexistent/nuvio-mpv-smoke.mkv"));
  until=SDL_GetTicks64()+5000;
  while (!video_falhou() && SDL_GetTicks64()<until) frame();
  assert(video_falhou() && *video_erro_texto());
  video_parar(); assert(!video_falhou());
  assert(video_tocar(argv[1]));
  waitReady();
  video_parar(); assert(video_tocar(argv[1]));
  waitReady(); // rapid replacement must ignore the previous stop/EOF events
  video_encerrar(); SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  puts("mpv smoke: states, GL pixels/orientation/alpha/crop, audio/subtitles, seek, EOF/error OK");
  return 0;
}
