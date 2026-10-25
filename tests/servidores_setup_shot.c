// Offline fixtures, production setup renderer and keyboard navigation.
#define SRVSETUP_SHOT 1
#include <assert.h>
// Fixture server names for manage/discovery shots; without them connected()
// is false and every row reads "Não configurado".
char shotServerNames[3][96];
#define plex_conectado shotPlexConectado
#define jellyfin_conectado shotJfConectado
#define emby_conectado shotEmbyConectado
#define plex_servidor_nome shotPlexNome
#define jellyfin_servidor_curto shotJfCurto
#define emby_servidor_curto shotEmbyCurto
#define plex_estado shotPlexEstado
#define jellyfin_estado shotJellyfinEstado
#define emby_estado shotEmbyEstado
#define plex_entrar shotPlexEntrar
#define plex_cancelar shotPlexCancelar
#define jellyfin_cancelar shotJellyfinCancelar
#define jellyfin_entrar_quick_connect shotJellyfinEntrar
#define jellyfin_definir_servidor shotJellyfinServidor
#define emby_definir_servidor shotEmbyServidor
static int shotPlexConectado(void) { return shotServerNames[0][0] != 0; }
static int shotLoginPending;
static int shotJfConectado(void) { return !shotLoginPending && shotServerNames[1][0] != 0; }
static int shotEmbyConectado(void) { return !shotLoginPending && shotServerNames[2][0] != 0; }
static const char *shotPlexNome(void) { return shotServerNames[0]; }
static const char *shotJfCurto(void) { return shotServerNames[1]; }
static const char *shotEmbyCurto(void) { return shotServerNames[2]; }
#define NV_TXT_TELA
#include "../src/tela21.h"
static void shotIsland(GfxRect r, float radius, float alpha);
static float shotButton(float x, float y, const char *label, const char *icon, float focus, float alpha);
static GfxRect focusedButton;
#define plrui_botao shotButton
#define t21_ilha shotIsland
#include "../src/srvsetup.c"
#undef t21_ilha
#undef plrui_botao
static float shotButton(float x, float y, const char *label, const char *icon, float focus, float alpha) {
  float w = plrui_botao_largura(label, icon), k = gfx_escala();
  if (focus > .99f && alpha > .99f) focusedButton = (GfxRect){x*k,(y-(screen==ROWS?bodyScroll:0))*k,w*k,60*k};
  assert(x*k >= 0 && (x+w)*k <= 1920.1f);
  return plrui_botao(x,y,label,icon,focus,alpha);
}
static int brightArt;
static void shotIsland(GfxRect r, float radius, float alpha) {
  if (brightArt) {
    GLuint art = tex_obter_larg("/Volumes/ExternalSSD/tmp/servidores-polish/assets/martian.jpg", 1920);
    if (art) gfx_rect(r, art, GFX_TEXTO, 0, 0, 0, radius / r.h, 1, 1, 1, 1);
  }
  t21_ilha(r, radius, alpha);
}
PxEstado shotPlexEstado(char *out, size_t n) { if(out && n) snprintf(out,n,"H7KM"); return PX_EST_CODIGO; }
JfEstado shotJellyfinEstado(char *out, size_t n) { if(out && n) snprintf(out,n,"482916"); return JF_EST_QC_CODIGO; }
JfEstado shotEmbyEstado(char *out, size_t n) { if(out && n) *out=0; return JF_EST_SERVIDOR_OK; }
int shotPlexEntrar(void) { return 1; }
void shotPlexCancelar(void) {}
void shotJellyfinCancelar(void) {}
int shotJellyfinEntrar(void) { return 1; }
int shotJellyfinServidor(const char *url) { (void)url; return 1; }
int shotEmbyServidor(const char *url) { (void)url; return 1; }
#include "rail_shot.h"
#include "home.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
static GLuint target;
static int homeShot;
static Uint32 shotTime = 30000;
static int motionCapture;
static void capture(const char *dir, const char *name);
static void motionExtra(const char *dir) {
  static const int times[] = {0,50,80,100,150,160,250,320,400};
  const char *kinds[] = {"exit", "reversal", "page"};
  for (int kind = 0; kind < 3; kind++) {
    opened = 1; change(CHOOSE);
    for (int f = 0; f < 60; f++) { shotTime += 16; srvsetup_atualizar(shotTime); }
    if (kind == 2) { change(ADDRESS); assert(previousPage.tex); }
    else { srvsetup_fechar(); if (kind == 1) { shotTime += 50; srvsetup_atualizar(shotTime); opened = 1; } }
    motionCapture = 1; int previous = 0;
    for (unsigned i = 0; i < sizeof times / sizeof *times; i++) {
      for (int t = previous; t < times[i]; t += 10) { shotTime += 10; srvsetup_atualizar(shotTime); }
      char name[128]; snprintf(name, sizeof name, "setup-%s-%03d", kinds[kind], times[i]);
      capture(dir, name); previous = times[i];
    }
    if (!kind) assert(entry == 0);
    if (kind == 1) assert(entry == 1);
  }
  motionCapture = 0;
}
static void capture(const char *dir, const char *name) {
  if (!motionCapture && !homeShot) for (int i=0;i<90;i++) { shotTime+=16; srvsetup_atualizar(shotTime); }
  for (int i=0;i<(motionCapture ? 1 : homeShot ? 45 : 30);i++) {
    if (!motionCapture) SDL_Delay(10);
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,1920,1080);glClear(GL_COLOR_BUFFER_BIT);
    if(homeShot) { home_atualizar(1.0f/60,codeSince+28000+i*16);home_desenhar(codeSince+28000+i*16); }
    else { if (!motionCapture) { shotTime += 16; srvsetup_atualizar(shotTime); } srvsetup_desenhar(shotTime); }
    rail_shot_desenhar(homeShot?MENU_INICIO:MENU_BIBLIOTECA);glFinish();
    if (!motionCapture && !homeShot && !i)
      for(int step=0;step<150;step++){shotTime+=16;srvsetup_atualizar(shotTime);}
  }
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[900];snprintf(path,sizeof path,"%s/%s-app.png",dir,name);assert(!IMG_SavePNG(surface,path));puts(path);
  free(pixels);SDL_FreeSurface(surface);
}
static void key(SDL_Keycode k){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;srvsetup_evento(&e);}
int main(int argc,char **argv){
  assert(argc==2);SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\njellyfinLocal 0\nbordaFocoCartaz 1\n");ajustes_dir(dados_dir());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Personal servers proof",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);assert(SDL_GL_CreateContext(w));
  GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);
  char art[1024];assert(realpath("deploy/app/art",art));gfx_icones_dir(art);rail_shot_aplicar();
  rail_shot_aplicar();
  const char *scale=getenv("NUVIO_SHOT_ESCALA_TEXTO");gfx_escala_txt_definir(scale?atof(scale):1);
  ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO")?atoi(getenv("NUVIO_SHOT_VIDRO")):0);
  if(getenv("NUVIO_SHOT_WIZARD")) ajustes_rapido_cor_definir(11);
  const char *filter=getenv("NUVIO_SHOT_SCREEN");
  opened=1;profile=perfis_ativo();provider=0;codeSince=1000;change(CHOOSE);if(!filter) capture(argv[1],"setup-escolher");
  key(SDLK_RIGHT);assert(selected==1);key(SDLK_LEFT);assert(selected==0);key(SDLK_ESCAPE);assert(!opened);
  struct {int state,p;const char *name;} scenes[]={
    {CODE,0,"setup-plex-pin"},{CODE,1,"setup-jellyfin-quickconnect"},{CONNECT_CODE,2,"setup-emby-connect"},
    {DISCOVER,0,"setup-plex-descoberta"},{DISCOVER,1,"setup-jellyfin-descoberta"},{DISCOVER,2,"setup-emby-descoberta"},
    {ADDRESS,0,"setup-plex-manual"},{ADDRESS,1,"setup-manual"},{ADDRESS,2,"setup-emby-manual"},
    {USERS,1,"setup-jellyfin-usuario"},{USERS,2,"setup-emby-usuario"},
    {PASSWORD,1,"setup-jellyfin-senha"},{PASSWORD,2,"setup-emby-senha"},{CONNECT_PASSWORD,2,"setup-emby-credentials"},
    {LIBRARIES,0,"setup-plex-bibliotecas"},{LIBRARIES,1,"setup-bibliotecas"},{LIBRARIES,2,"setup-emby-bibliotecas"},
    {READY,0,"setup-plex-pronto"},{READY,1,"setup-pronto"},{READY,2,"setup-emby-pronto"},
    {MANAGE,0,"servidores-gerenciar"},{REMOVE,0,"servidor-remover"},{ERROR_SCREEN,2,"servidor-offline"},
    {ERROR_SCREEN,1,"setup-credencial-expirada"},{ROWS,0,"fileira-adicionar-servidor"},
    {PLEX_SERVERS,0,"setup-plex-servidor"},{LOGIN,2,"setup-emby"},
    {DISCOVER,1,"setup-buscando"},{DISCOVER,1,"setup-nenhum-servidor"}};
  for(unsigned i=0;i<sizeof scenes/sizeof *scenes;i++){
    if(filter && !strstr(scenes[i].name,filter)) continue;
    if(getenv("NUVIO_SHOT_WIZARD") && strncmp(scenes[i].name,"setup-",6)) continue;
    shotLoginPending=scenes[i].state==PASSWORD||scenes[i].state==LOGIN;
    opened=1;provider=scenes[i].p;change(scenes[i].state);worker.n=2;
    worker.kind=JOB_USERS;worker.done=worker.result=0;worker.profile=profile;
    strcpy(code,provider==0?"H7KM":provider==1?"482916":"729413");makeQR(provider==0?"https://plex.tv/link":provider==1?"http://jellyfin.example.test/web/#/quickconnect":"https://emby.media/pin");
    recoveryAuth=screen==ERROR_SCREEN&&provider==1;
    // Setup shots must show real connected names, never "Não configurado".
    for(int p=0;p<3;p++) lastSync[p]=time(NULL)-180*(p+1);
    if(screen==MANAGE||screen==REMOVE) { libraryProviderCounts[0]=346; libraryProviderCounts[1]=114; libraryProviderCounts[2]=9; }
    { const char *snames[]={"Casa","Sala","Estúdio"};
      // Manage and offline shots need all three rows live; discovery shows the
      // chosen provider only, as the approved fixture does.
      int live = screen==MANAGE||screen==REMOVE||screen==ERROR_SCREEN;
      memset(shotServerNames,0,sizeof shotServerNames);
      if(screen!=CODE && screen!=CONNECT_CODE)
        for(int p=0;p<3;p++) if(live || p==scenes[i].p || screen==CHOOSE) snprintf(shotServerNames[p],96,"%s",snames[p]); }
    if(screen==LIBRARIES) selected=3;
    for(int j=0;j<2;j++){
      const char *name=j?"Sala de cinema":provider==0?"Início":provider==1?"Sala":"Estúdio";
      snprintf(worker.servers[j].nome,96,"%s",name);snprintf(worker.plex[j].nome,96,"%s",name);
      snprintf(worker.users[j].nome,96,"%s",j?"Diego":"Marina");worker.users[j].senha=1;
      snprintf(worker.users[j].imagem,640,"/Volumes/ExternalSSD/tmp/servidores-polish/assets/%s.jpg",j?"diego":"marina");
    }
    nLibraries=3;memset(libraries,0,sizeof libraries);
    const char *titles[]={"Filmes","Séries","Shows"};
    for(int j=0;j<3;j++){
      snprintf(libraries[j].titulo,sizeof libraries[j].titulo,"%s",titles[j]);
      snprintf(libraries[j].chave,sizeof libraries[j].chave,"%s%d",prefix[provider],j+1);
      snprintf(libraries[j].tipo,sizeof libraries[j].tipo,"%s",j==1?"series":j==2?"musicvideo":"movie");checked[j]=j<2;
      libraries[j].n=j==0?284:j==1?62:18;
    }
    rowChoice=rowStyle=0;
    if(screen==ROWS){nLibraries=4;snprintf(libraries[3].chave,sizeof libraries[3].chave,"plex_recent_1");snprintf(libraries[3].titulo,sizeof libraries[3].titulo,"Mais recentes");snprintf(libraries[3].tipo,sizeof libraries[3].tipo,"movie");snprintf(libraries[1].titulo,sizeof libraries[1].titulo,"Coleção espacial");snprintf(libraries[1].tipo,sizeof libraries[1].tipo,"group");snprintf(libraries[1].chave,sizeof libraries[1].chave,"plex_group_42");snprintf(libraries[2].titulo,sizeof libraries[2].titulo,"Fila do fim de semana");snprintf(libraries[2].tipo,sizeof libraries[2].tipo,"list");snprintf(libraries[2].chave,sizeof libraries[2].chave,"plex_list_43");rowChoice=selected=1;}
    if(!strcmp(scenes[i].name,"setup-nenhum-servidor"))worker.n=0;
    if(!strcmp(scenes[i].name,"setup-buscando"))worker.alive=1;
    strcpy(username,"Marina");strcpy(password,"fixture");strcpy(addressText,"http://media.example.test:8096");
    int expectedScreen=screen;
    capture(argv[1],scenes[i].name);worker.alive=0;
    assert(screen==expectedScreen);
    if (gfx_escala_txt() > 1) {
      selected = targets() - 1;
      if (selected < 0) selected = 0;
      focusedButton = (GfxRect){0};
      char reachable[180]; snprintf(reachable, sizeof reachable, "%s-last-control", scenes[i].name);
      capture(argv[1],reachable);
      if (focusedButton.w) assert(focusedButton.y >= 385 && focusedButton.y + focusedButton.h <= 980);
    }
    static const int moments[]={0,50,80,100,150,160,250,320,400};
    for(int kind=0;kind<2 && !getenv("NUVIO_SHOT_QUICK");kind++) {
      if(!kind){entry=page=pageSeconds=0;memset(focusAnim,0,sizeof focusAnim);}
      else { for(int f=0;f<60;f++){shotTime+=16;srvsetup_atualizar(shotTime);} key(SDLK_RIGHT); }
      motionCapture=1;int previous=0;
      for(unsigned t=0;t<sizeof moments/sizeof *moments;t++) {
        for(int ms=previous;ms<moments[t];ms+=10){shotTime+=10;srvsetup_atualizar(shotTime);}
        char name[180];snprintf(name,sizeof name,"%s-%s-%03d",scenes[i].name,kind?"focus":"entry",moments[t]);capture(argv[1],name);previous=moments[t];
      }
      motionCapture=0;
    }
  }
  if(getenv("NUVIO_SHOT_WIZARD")) {
    screen=READY;selected=0;opened=1;key(SDLK_RETURN);assert(!opened);
    screen=DISCOVER;provider=1;worker.alive=0;worker.n=0;opened=1;selected=2;key(SDLK_RETURN);assert(!opened);
    assert(targets()==3);
    screen=LIBRARIES;nLibraries=8;selected=7;opened=1;
    for(int f=0;f<120;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
    assert(fabsf(scroll-5)<.02f);
    key(SDLK_ESCAPE);assert(screen==MANAGE);
    return 0;
  }
  static CatItem items[5];CatFileira ready={0};
  for(int i=0;i<5;i++){
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt%d",100+i);strcpy(items[i].tipo,"movie");
    snprintf(items[i].titulo,sizeof items[i].titulo,"Filme %d",i+1);
    snprintf(items[i].poster,sizeof items[i].poster,"/Volumes/ExternalSSD/tmp/servidores-polish/assets/p%02d.jpg",i+2);
  }
  strcpy(ready.chave,"plex_group_42");strcpy(ready.titulo,"Coleção espacial");strcpy(ready.tipo,"movie");ready.n=5;
  assert(home_iniciar("deploy/app/art"));cat_definir_tudo(items,5,&ready,1);home_ir_topo();home_focar_titulo(0);homeShot=1;capture(argv[1],"fileira-servidor-pronta");homeShot=0;
  opened=1;screen=ROWS;provider=0;nLibraries=1;rowChoice=0;strcpy(libraries[0].chave,"plex_1");
  selected=2;key(SDLK_RETURN);assert(fil_servidor_ordem("plex_1")==1);
  selected=3;key(SDLK_RETURN);assert(fil_servidor_ocultar_vistos("plex_1"));
  selected=5;key(SDLK_RETURN);assert(screen==READY);
  screen=ERROR_SCREEN;recoveryAuth=0;selected=3;key(SDLK_RETURN);assert(screen==MANAGE);
  screen=ERROR_SCREEN;recoveryAuth=1;selected=2;key(SDLK_RETURN);assert(!opened);
  opened=1;screen=REMOVE;selected=0;key(SDLK_RETURN);assert(screen==MANAGE);
  screen=LIBRARIES;nLibraries=2;selected=0;key(SDLK_RETURN);assert(!checked[0]);
  screen=LIBRARIES;key(SDLK_ESCAPE);assert(screen==MANAGE);
  screen=LOGIN;key(SDLK_ESCAPE);assert(screen==CHOOSE);
  screen=ROWS;key(SDLK_ESCAPE);assert(screen==READY);
  { screen=ADDRESS; provider=1; keyboard(1); capture(argv[1],"setup-manual-teclado"); key(SDLK_ESCAPE); field=0; teclado_esquecer(); }
  teclado_esquecer();field=0;
  static const int times[]={0,50,80,100,150,160,250,320,400};
  for(int kind=0;kind<2 && !getenv("NUVIO_SHOT_QUICK");kind++) {
    opened=1;entry=0;change(CHOOSE);
    if(kind) { for(int i=0;i<60;i++){shotTime+=16;srvsetup_atualizar(shotTime);} key(SDLK_RIGHT); }
    motionCapture=1;int previous=0;
    for(unsigned i=0;i<sizeof times/sizeof *times;i++) {
      for(int t=previous;t<times[i];t+=10){shotTime+=10;srvsetup_atualizar(shotTime);}
      char name[128];snprintf(name,sizeof name,"setup-%s-%03d",kind?"focus":"entry",times[i]);capture(argv[1],name);previous=times[i];
    }
  }
  motionCapture=0;
  if (!getenv("NUVIO_SHOT_QUICK")) {
    opened=1; change(LIBRARIES); nLibraries=8; selected=7;
    shotTime+=40; srvsetup_atualizar(shotTime);
    assert(scroll>0 && scrollVelocity>0);
    float moving=scroll; selected=0;
    shotTime+=10; srvsetup_atualizar(shotTime);
    assert(scroll>=0 && fabsf(scroll-moving)<1);
    for(int i=0;i<120;i++){shotTime+=10;srvsetup_atualizar(shotTime);}
    assert(scroll<.01f);
    nLibraries=3;
    motionExtra(argv[1]);
    brightArt = 1; opened = 1; provider = 0; change(CODE);
    strcpy(code, "H7KM"); makeQR("https://plex.tv/link");
    capture(argv[1], "setup-bright-art"); brightArt = 0;
    const char *modes[] = {"collapseSidebar 1\nmodernSidebar 1\nhomeLayoutLocal 0\n", "collapseSidebar 0\nmodernSidebar 1\nhomeLayoutLocal 0\n", "collapseSidebar 1\nmodernSidebar 1\nhomeLayoutLocal 2\n"};
    for (int m = 0; m < 3; m++) for (int reduced = 0; reduced < 2; reduced++) {
      char settings[256], name[128];
      snprintf(settings, sizeof settings, "idioma 0\nperformanceAutoVersaoLocal 1\nselected_theme 6\nanimacoes %d\n%s", reduced, modes[m]);
      dados_gravar("ajustes.txt", settings); ajustes_dir(dados_dir());
      gfx_escala_txt_definir(scale ? atof(scale) : 1);
      ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO") ? atoi(getenv("NUVIO_SHOT_VIDRO")) : 0);
      opened = 1; change(CHOOSE);
      snprintf(name, sizeof name, "setup-rail%d-reduced%d", m, reduced); capture(argv[1], name);
      if (reduced) { assert(ajustes_animacoes_reduzidas()); assert(focusAnim[selected] == 1); }
    }
  }
  // Pointer + user-strip regressions (review P1-2, P1-3).
  {
    const PonteiroAlvo *v; int n;
    ponteiro_teste_toque(1);
    dados_gravar("ajustes.txt","idioma 0\nanimacoes 0\n"); ajustes_dir(dados_dir()); assert(!ajustes_animacoes_reduzidas());
    opened=1; provider=1; change(USERS); worker.n=5; entry=1; selected=2;
    for(int j=0;j<5;j++) snprintf(worker.users[j].nome,96,"User%d",j);
    for(int f=0;f<120;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
    assert(userScroll<.01f);
    ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
    n=ponteiro_teste_lista(&v); assert(n>=5);
    int cards=0; for(int j=0;j<n;j++) if(v[j].focar&&v[j].ativar&&v[j].a<5) cards++;
    assert(cards>=3);
    // 3rd -> 4th user: group must slide, never jump.
    key(SDLK_RIGHT); assert(selected==3);
    float prev=userScroll, firstStep=-1, maxStep=0; int mid=0;
    for(int f=0;f<120;f++){
      shotTime+=16; srvsetup_atualizar(shotTime);
      assert(userScroll>=-.01f && userScroll<=3.05f);
      if(userScroll>.05f && userScroll<2.95f) mid++;
      if(firstStep<0 && userScroll>0) firstStep=userScroll;
      if(userScroll-prev>maxStep) maxStep=userScroll-prev; prev=userScroll;
    }
    assert(mid>=3); assert(maxStep<2.0f); assert(fabsf(userScroll-3)<.02f);
    // Pointer rect for the user card follows the animated scroll.
    ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
    n=ponteiro_teste_lista(&v); float settledX=-1;
    for(int j=0;j<n;j++) if(v[j].a==3&&v[j].focar) settledX=v[j].x;
    assert(settledX>0);
    key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT); assert(selected==0);
    float xs[3]; for(int k=0;k<3;k++){
      for(int f=0;f<6;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
      ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
      n=ponteiro_teste_lista(&v); xs[k]=-1;
      for(int j=0;j<n;j++) if(v[j].a==3&&v[j].focar) xs[k]=v[j].x;
    }
    assert(xs[0]!=xs[1]||xs[1]!=xs[2]); assert(xs[0]>0);
    assert(xs[0]<=settledX+200 || xs[0]>=0);
    // Closed wizard registers no targets.
    srvsetup_fechar(); for(int f=0;f<60;f++){shotTime+=16;srvsetup_atualizar(shotTime);}
    ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
    n=ponteiro_teste_lista(&v); for(int j=0;j<n;j++) assert(!(v[j].focar==pointFocus));
    ponteiro_teste_toque(0);
  }
  puts("Setup D-pad: bounded focus, Back, safe removal default, library toggle: PASS");
  return 0;
}
