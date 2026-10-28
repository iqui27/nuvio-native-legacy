// Actual parser, sheet and auto-pick paths; no network or user data.
#ifdef NV_WEBOS
#define dts_playback_enabled test_dts_playback_enabled
#define nv_webos_major test_webos_major
#endif
#include "../src/streams.c"
#ifdef NV_WEBOS
static int adapterProbes;
int test_dts_playback_enabled(void) { adapterProbes++; return 0; }
int test_webos_major(void) { return 6; }
#endif
#include <SDL2/SDL_image.h>
#include <assert.h>
#include "ilha.h"
extern void fonteincompat_settings(void);
extern void fonteincompat_settings_draw(void);
static unsigned char pixels[1920*1080*3];
static GLuint framebuffer;
static void frames(int count,int settings) {
  static Uint32 now=100000;
  for(int i=0;i<count;i++,now+=16) {
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glViewport(0,0,1920,1080);
    glClearColor(.035,.04,.05,1);glClear(GL_COLOR_BUFFER_BIT);gfx_novo_quadro();
    if(settings) fonteincompat_settings_draw();
    else {stream_folha_atualizar(1.f/60,now);stream_folha_desenhar(now);ilha_desenhar(now);}
  }
}
static void shot(const char *dir,const char *name,int settings) {
  frames(70,settings);
  glFinish();glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,1920,1080,GL_RGB,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *s=SDL_CreateRGBSurface(0,1920,1080,24,0xff,0xff00,0xff0000,0);assert(s);
  for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,pixels+(1080-1-y)*1920*3,1920*3);
  char path[700];snprintf(path,sizeof path,"%s/%s.png",dir,name);assert(IMG_SavePNG(s,path)==0);SDL_FreeSurface(s);
}
static void key(SDL_Keycode code) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;
  stream_folha_evento(&e);
  e.type=SDL_KEYUP;stream_folha_evento(&e);
  frames(8,0);
}
static void profile(int dv,int thd,int dts,int hd,int av1) {
  stream_definir_tela(1,dv);stream_definir_audio_capacidades(thd,dts,hd,1,1);
  stream_definir_decoder4k(1,1,1,av1);
}
int main(int argc,char **argv) {
  assert(argc==2);
#ifdef NV_WEBOS
  for(int i=0;i<100;i++)incompatPlataforma();
  assert(adapterProbes==1&&audioCap[0]==0&&audioCap[1]==0&&audioCap[2]==0);
  puts("PASS: failed webOS adapter probe cached across list capability checks");
  return 0;
#endif
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);IMG_Init(IMG_INIT_PNG);
  SDL_Window *window=SDL_CreateWindow("Sources compatibility",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
  assert(SDL_GL_CreateContext(window));GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(32);
  gfx_icones_dir("deploy/app/art");badges_carregar("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS"));ajustes_iniciar();
  assert(ajustes_shot_valor("idioma",IDIOMA_EN+1)); // English screenshots.
  assert(ajustes_shot_valor("fonteTextoLocal",0));
  char json[4096];FILE *f=fopen("tests/fixtures/fonteincompat.json","rb");assert(f);
  size_t len=fread(json,1,sizeof json-1,f);fclose(f);json[len]=0;Stream *sources=NULL;
  assert(stream_extrair(json,"Fixture",&sources)==6);
  stream_definir_alvo("tt-fixture");stream_definir_lista(sources,6);
  // Samsung web Tizen: no DV, TrueHD or DTS. AV1 support unknown.
  profile(0,0,0,0,-1);
  assert(ajustes_fonte_ocultar(0)&&ajustes_fonte_ocultar(1)&&!ajustes_fonte_ocultar(2));
  assert(incompatContar()==3);
  assert(ajustes_shot_valor("fonteOcultarDvLocal",1));assert(incompatContar()==2);
  assert(ajustes_shot_valor("fonteOcultarAudioLocal",1));assert(incompatContar()==0);
  assert(ajustes_shot_valor("fonteOcultarDvLocal",2));assert(ajustes_shot_valor("fonteOcultarAudioLocal",2));
  for(int i=0;i<6;i++)assert(incompatOculta(&lista[i])==(i==0||i==2||i==3));
  assert(stream_automatico_disponivel(0)==0&&stream_automatico_disponivel(2)==0&&stream_automatico_disponivel(3)==0);
  int pick=stream_automatico();assert(pick>=0&&!incompatOculta(&lista[pick]));
  ilha_abrir_sessao();ilha_coberta(0);ilha_relogio_visivel(1);
  stream_folha_nome("Capability fixtures");stream_folha_abrir();montar(-1);assert(nOrdem==3);
  shot(argv[1],"sources-hidden",0);
  int target=0;for(int i=0;i<navigationCount;i++)target|=navigation[i].group==2;assert(target);
  // Real D-pad input, with multiple update/draw frames between presses.
  assert(grupo==1);
  for(int i=0;i<10&&grupo==1;i++)key(SDLK_UP);
  assert(grupo==2&&!incompatRevelada);
  key(SDLK_DOWN);assert(grupo==1);
  key(SDLK_UP);assert(grupo==2);
  shot(argv[1],"sources-hidden-focused",0);assert(grupo==2);
  key(SDLK_RETURN);assert(incompatRevelada&&nOrdem==6);
  assert(grupo==1&&foco>=0&&foco<nOrdem&&filtrado(foco)>=0);
  shot(argv[1],"sources-revealed",0);
  aberta=0;stream_folha_abrir();assert(incompatRevelada);
  stream_definir_alvo("tt-android");stream_definir_lista(sources,6);
  // Android: decodes DV and audio, AV1 decoder explicitly rejects UHD.
  profile(1,1,1,1,0);assert(!ajustes_fonte_ocultar(0)&&!ajustes_fonte_ocultar(1)&&ajustes_fonte_ocultar(2));
  assert(incompatContar()==1&&incompatOculta(&lista[4])&&!incompatOculta(&lista[0]));
  assert(!stream_automatico_disponivel(4));
  stream_preferir(4);int autoFila[1],livres=0;assert(filaDoPlay(1,4,autoFila,&livres)>0&&autoFila[0]!=4);
  assert(ajustes_shot_valor("fonteOcultarVideoLocal",1));assert(incompatContar()==0);
  assert(ajustes_shot_valor("fonteOcultarVideoLocal",2));
  stream_definir_alvo("tt-all");stream_definir_lista(sources,1);profile(0,0,0,0,-1);
  assert(incompatContar()==1&&!incompatOculta(&lista[0]));
  stream_folha_abrir();montar(-1);assert(nOrdem==1&&strstr(fraseDicaChips(),"All"));
  shot(argv[1],"sources-all-hidden-warning",0);
  // Multiple audio formats and uncertain metadata must remain available.
  Stream multi=sources[2];multi.filtroLido=0;snprintf(multi.descricao,sizeof multi.descricao,"HEVC TrueHD AAC");
  assert(!incompatRegra(&multi));
  Stream fallback=sources[0];snprintf(fallback.arquivo,sizeof fallback.arquivo,"HEVC.DV.HDR10.mkv");assert(!incompatRegra(&fallback));
  Stream p7=sources[0];snprintf(p7.descricao,sizeof p7.descricao,"2160p HEVC Dolby Vision profile 7 HDR10 AAC");assert(!incompatRegra(&p7));
  stream_definir_decoder4k(0,1,1,-1);assert(incompatRegra(&sources[5])==3);
  Stream hd=sources[5];hd.altura=1080;assert(!incompatRegra(&hd));
  stream_definir_audio_capacidades(0,0,0,0,0);
  Stream ac3=sources[5];ac3.filtroLido=0;snprintf(ac3.descricao,sizeof ac3.descricao,"1080p HEVC AC3");ac3.altura=1080;assert(incompatRegra(&ac3)==2);
  stream_definir_audio_capacidades(0,0,0,0,1);
  const char *audioJson="{\"streams\":["
    "{\"title\":\"1080p HEVC EAC3\",\"url\":\"https://example.invalid/eac3.mkv\"},"
    "{\"title\":\"1080p HEVC EAC3 AC3\",\"url\":\"https://example.invalid/multi.mkv\"},"
    "{\"title\":\"1080p HEVC EAC3\",\"audioCodec\":\"AC3\",\"url\":\"https://example.invalid/fallback.mkv\"},"
    "{\"title\":\"1080p HEVC EAC3\",\"behaviorHints\":{\"filename\":\"Movie.EAC3.AC3.mkv\"},\"url\":\"https://example.invalid/file.mkv\"}]}";
  Stream *audio=NULL;assert(stream_extrair(audioJson,"Fixture",&audio)==4);
  assert(incompatRegra(&audio[0])==2);
  for(int i=1;i<4;i++)assert(!incompatRegra(&audio[i]));
  free(audio);
  profile(-1,-1,-1,-1,-1);assert(incompatRegra(&sources[0])==0&&incompatRegra(&sources[2])==0);
  profile(0,0,0,0,-1);fonteincompat_settings();shot(argv[1],"settings-sources",1);
  free(sources);puts("PASS: Samsung/Android; hidden count; reveal; auto skip; all-hidden; fallback; local toggles/search");
  return 0;
}
