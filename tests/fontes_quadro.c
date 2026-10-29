#include "../src/streams.c"
#include <assert.h>
#include <unistd.h>
static int counting;
static unsigned long visits, positions;
void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *fn, void *caller) {
  (void)caller;
  if (counting && fn == (void *)linhaAltura) positions++;
  if (counting && (fn == (void *)alturaLinha || fn == (void *)passaChips ||
      fn == (void *)grupoRes || fn == (void *)pontos || fn == (void *)doProvedor)) visits++;
}
void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *fn, void *caller) { (void)fn; (void)caller; }
static void frame(void) {
  txt_novo_quadro();
  stream_folha_atualizar(1.f/60, SDL_GetTicks());
  stream_folha_desenhar(SDL_GetTicks());
  if(counting) { assert(positions<60 && "draw/culling must not visit every row"); positions=0; }
}
static void geometry(void) {
  float *y=malloc(nOrdem*sizeof *y), *h=malloc(nOrdem*sizeof *h); assert(y && h);
  int count=nOrdem; float total=alturaTotal, sections[FOLHA_GRUPOS];
  for(int i=0;i<count;i++) { y[i]=linhaTopo(i); h[i]=linhaAltura(i); }
  for(int g=0;g<FOLHA_GRUPOS;g++) sections[g]=secTopo(g);
  montarBase(automaticaDaFolha());
  assert(count==nOrdem && fabsf(total-alturaTotal)<.1f);
  for(int i=0;i<count;i++) { assert(fabsf(y[i]-linhaY[i])<.1f); assert(fabsf(h[i]-linhaH[i])<.01f); }
  for(int g=0;g<FOLHA_GRUPOS;g++) if(secN[g]) assert(fabsf(sections[g]-secY[g])<.1f);
  free(y); free(h);
  atomic_fetch_add(&folhaRevisao,1); montar(automaticaDaFolha());
}
static unsigned long measure(int count, int addon, int verify) {
  Stream *s=calloc(count,sizeof *s); assert(s);
  for(int i=0;i<count;i++) {
    s[i].altura=1080; s[i].mp4=1; s[i].fileIdx=-1;
    snprintf(s[i].url,sizeof s[i].url,"https://fixture.invalid/%d.mp4",i);
    snprintf(s[i].provedor,sizeof s[i].provedor,"AIOStreams");
    snprintf(s[i].rotulo,sizeof s[i].rotulo,"Source %d",i);
    snprintf(s[i].descricao,sizeof s[i].descricao,"Size\nAudio\nLanguages\nGroup\nMovie.mkv");
  }
  stream_definir_lista(s,count); free(s); stream_folha_abrir();
  anim=1; grupo=1; foco=0; linhaAnt=-1; abreFoco=1; abreAnt=0;
  for(int i=0;i<3;i++) frame();
  visits=0; counting=1;
  Uint64 start=SDL_GetPerformanceCounter();
  frame();
  double ms=(SDL_GetPerformanceCounter()-start)*1000./SDL_GetPerformanceFrequency();
  counting=0;
  unsigned long idle=visits;
  printf("sources=%d addon=%d idle visits=%lu host frame=%.3f ms\n",count,addon,idle,ms); fflush(stdout);
  if(verify) assert(idle<100 && "steady frame must only process visible rows");
  foco=1;
  for(int i=0;i<5;i++) {
    visits=0; counting=1; frame(); counting=0;
    if(verify) { assert(visits<100 && "focus animation must not rebuild the list"); geometry(); }
  }
  // Scroll near the end: culling must work at either end of a long list.
  foco=nOrdem-1; focoVisto=foco; linhaAnt=-1; abreFoco=1;
  montar(automaticaDaFolha()); rolagem=alvoRolagem();
  visits=0; counting=1; frame(); counting=0;
  if(verify) assert(visits<100);
  // A changed chip must rebuild once, then settle back to bounded work.
  soMp4=1; frame(); assert(nOrdem==count);
  visits=0; counting=1; frame(); counting=0;
  if(verify) assert(visits<100);
  if(verify) {
    geometry();
    // Real mutations: failure marks, same-length replacement, append, filters.
    int raw=filtrado(foco); stream_vod_falhou(raw); frame(); geometry();
    stream_vod_tocou(raw); frame(); geometry();
    Stream extra=lista[0]; extra.mp4=0;
    stream_lista_acrescentar(&extra,1,0); frame(); assert(nOrdem==count); geometry();
    soMp4=0; frame(); assert(nOrdem==count+1); geometry();
    filtro=1; frame(); assert(nOrdem==count+1); geometry();
    grupo=0; frame(); geometry();
    Stream *mixed=malloc(n*sizeof *mixed); assert(mixed); memcpy(mixed,lista,n*sizeof *mixed);
    int total=n;
    for(int i=0;i<total;i++) { mixed[i].altura=i%2?2160:720; mixed[i].foraCache=i%3==0; mixed[i].tamanhoMB=i%7*1024; }
    stream_definir_lista(mixed,total); free(mixed); stream_folha_abrir();
    grupo=1; foco=total/2; frame(); geometry();
    grupo=0; frame(); geometry();
  }
  return idle;
}
int main(int argc,char **argv) {
  int verify=argc==1 || strcmp(argv[1],"--measure");
  char dir[]="/tmp/nuvio-frame479-XXXXXX"; assert(mkdtemp(dir));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1"); assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("frame479",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl=SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); glViewport(0,0,1920,1080);
  for(int addon=0;addon<2;addon++) {
    char path[512]; snprintf(path,sizeof path,"%s/ajustes.txt",dir);
    FILE *f=fopen(path,"w"); assert(f); fprintf(f,"fonteTextoLocal %d\nvidroLocal 0\n",addon); fclose(f); ajustes_dir(dir);
    unsigned long small=measure(50,addon,verify), large=measure(1217,addon,verify);
    if(verify) assert(small==large && "work must be independent of total sources");
    unlink(path);
  }
  txt_encerrar(); stream_definir_lista(NULL,0); SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit(); rmdir(dir);
}
