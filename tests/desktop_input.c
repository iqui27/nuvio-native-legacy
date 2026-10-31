#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#define janelaW layoutWindowW
#define janelaH layoutWindowH
#include "../src/layout.c"
#undef janelaW
#undef janelaH
#define desktop_native_init test_native_init
#define desktop_native_sidebar test_native_sidebar
#define desktop_native_dark test_native_dark
#define desktop_native_swipes test_native_swipes
#include "../src/ponteiro.c"
#include "../src/rolagemtoque.h"
static int opens, saved, focuses, arrows, moves, contexts, backs, panel;
static float pixels;
static int allowSwipes=1;
void test_native_init(SDL_Window *w) { (void)w; }
void test_native_sidebar(float x,float reveal) { (void)x;(void)reveal; }
int test_native_dark(void) { return 1; }
int test_native_swipes(void) { return allowSwipes; }
void menu_desktop_expandir(void) { opens++; }
void menu_desktop_revelar(void) { opens++; }
void menu_desktop_hover(float x) { (void)x; }
void spainel_abrir(void) { saved++; panel=1; }
int spainel_aberto(void) { return panel; }
void spainel_fechar(void) { panel=0; }
static float drawScale=1;
float gfx_escala(void) { return drawScale; }
void gfx_escala_sair(float x) { drawScale=x; }
float gfx_opacidade_grupo=1;
void gfx_sem_recorte(void) {}
void gfx_cor(GfxRect r,float radius,float a,float b,float c,float d) { (void)r;(void)radius;(void)a;(void)b;(void)c;(void)d; }
static void focus(int a,int b) { (void)a;(void)b;focuses++; }
static void context(int a,int b) { (void)a;(void)b;contexts++; }
static void deliver(const SDL_Event *e) { if(e->type==SDL_KEYDOWN) { if(e->key.keysym.sym==SDLK_AC_BACK) backs++; else arrows++; } }
static int row, rowStart, realScroll;
static float capturedScale;
static int latchedRow=-1;
static int scaledRows(const PonteiroRolagem *e) {
  capturedScale=gfx_escala();
  if(e->fase==PONT_ROL_INICIO) latchedRow=(int)(e->y/capturedScale/200);
  if(e->fase==PONT_ROL_MOVER) pixels+=e->delta/capturedScale;
  return 1;
}
static float realOffset;
static ToqueRolagem preciseRow;
static int scrolling(const PonteiroRolagem *e) {
  if(realScroll) return toquerol_evento(&preciseRow,e);
  if(e->fase==PONT_ROL_INICIO && !e->eixoY && (!row || (rowStart && e->delta>0))) return 0;
  if(e->fase==PONT_ROL_MOVER){pixels+=e->delta;moves++;rowStart=1;}
  return 1;
}
static void phased(int x,int y,float dx,float dy,int phase) {
  SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.which=NV_DESKTOP_PIXEL_WHEEL;
  e.wheel.mouseX=x;e.wheel.mouseY=y;e.wheel.preciseX=dx;e.wheel.preciseY=dy;
  // Wire values are also used by the native Cocoa bridge.
  e.wheel.x=NV_WHEEL_PHASED|phase;
  ponteiro_evento(&e,deliver);
}
static Uint32 fakeNow;
static Uint32 fakeClock(void) { return fakeNow; }
static ToqueRolagem replayRow;
static float replayOffset;
static int replayOwner(const PonteiroRolagem *e) { return toquerol_evento(&replayRow,e); }
static void replayFrame(void) {
  ponteiro_quadro(fakeNow);ponteiro_rolagem(replayOwner);
  toquerol_vincular(&replayRow,(GfxRect){0,0,1920,1200},1,-1e7f,1e7f,1,&replayOffset);
  fecharQuadro();
}
// Exactly what the Cocoa monitor pushes for one NSEvent.
static void cocoa(int x,int y,float dx,float dy,int phase,int momentum) {
  int flags=NV_WHEEL_PHASED;
  if(phase&1) flags|=NV_WHEEL_BEGIN;
  if((phase|momentum)&8) flags|=NV_WHEEL_END;
  if((phase|momentum)&16) flags|=NV_WHEEL_CANCEL;
  if(momentum) flags|=NV_WHEEL_MOMENTUM;
  if(!phase && !momentum) flags=NV_WHEEL_UNPHASED;
  SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.which=NV_DESKTOP_PIXEL_WHEEL;
  e.wheel.mouseX=x;e.wheel.mouseY=y;e.wheel.preciseX=dx;e.wheel.preciseY=dy;
  e.wheel.x=flags;e.wheel.y=phase|momentum<<8;
  ponteiro_evento(&e,deliver);
}
static int notes(const char *needle) {
  char path[600],line[512];int n=0;
  snprintf(path,sizeof path,"%s/Library/Logs/Nuvio/input.log",getenv("HOME"));
  FILE *f=fopen(path,"r");
  while(f && fgets(line,sizeof line,f)) n+=strstr(line,needle)!=NULL;
  if(f) fclose(f);
  return n;
}
static void wheel(int x,int y,float dx,float dy) {
  SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.which=NV_DESKTOP_PIXEL_WHEEL;
  e.wheel.mouseX=x;e.wheel.mouseY=y;e.wheel.preciseX=dx;e.wheel.preciseY=dy;
  ponteiro_evento(&e,deliver);
}
int main(void) {
  char home[]="/tmp/nuvio-desktop-input.XXXXXX";
  assert(mkdtemp(home));setenv("HOME",home,1); // never the owner's input.log
  char logs[600];snprintf(logs,sizeof logs,"%s/Library",home);mkdir(logs,0700);
  snprintf(logs,sizeof logs,"%s/Library/Logs",home);mkdir(logs,0700);
  assert(SDL_Init(SDL_INIT_TIMER)==0);
  float offset=100;
  ToqueRolagem realRow={0};
  toquerol_vincular(&realRow,(GfxRect){0,0,1000,500},1,0,500,0,&offset);
  PonteiroRolagem start={PONT_ROL_INICIO,0,20,0,200,200};
  assert(toquerol_evento(&realRow,&start));
  offset=0;assert(!toquerol_evento(&realRow,&start));
  start.delta=-20;assert(toquerol_evento(&realRow,&start));
  start.x=1200;assert(!toquerol_evento(&realRow,&start));
  const int sizes[][2]={{1280,800},{1440,900},{2560,1440}};
  for(int i=0;i<3;i++) for(int retina=1;retina<=2;retina++) {
    int w=sizes[i][0],h=sizes[i][1];
    layout_janela_definir(w,h);layout_tela_definir(w*retina,h*retina);
    assert(fabsf(NV_TELA_W/w-NV_TELA_H/h)<.0001f);
    assert(fabsf(fminf(NV_TELA_W/1920,NV_TELA_H/1080)-1)<.0001f);
    ponteiro_teste_janela(w,h);converter(0,w/2,h/2);
    assert(fabsf(px-NV_TELA_W/2)<.001f && fabsf(py-NV_TELA_H/2)<.001f);
  }
  layout_janela_definir(1280,800);ponteiro_iniciar();ponteiro_teste_janela(1280,800);
  // A wheel arriving during device/trackpad touch activity must retain ownership.
  ponteiro_quadro(SDL_GetTicks());ponteiro_rolagem(scrolling);fecharQuadro();
  phased(500,200,0,-1,NV_WHEEL_BEGIN);
  assert(nv_desktop_scroll_active);
  SDL_Event indirect={0};indirect.type=SDL_FINGERDOWN;indirect.tfinger.touchId=123;
  ponteiro_evento(&indirect,deliver);
  assert(nv_desktop_scroll_active);
  indirect.type=SDL_FINGERUP;ponteiro_evento(&indirect,deliver);
  phased(500,200,0,0,NV_WHEEL_END|NV_WHEEL_MOMENTUM);
  moves=0;pixels=0;
  ponteiro_quadro(SDL_GetTicks());
  ponteiro_alvo(100,100,500,500,focus,NULL,0,0);ponteiro_rolagem(scrolling);fecharQuadro();
  SDL_Event e={0};e.type=SDL_MOUSEMOTION;e.motion.x=200;e.motion.y=200;ponteiro_evento(&e,deliver);
  assert(focuses==1 && !arrows && moves==0);
  wheel(200,200,0,-.25f);wheel(200,200,0,-.25f);
  assert(moves==2 && fabsf(pixels+.75f)<.001f && !inRolagem && !arrows);
  nv_desktop_wheel_until=SDL_GetTicks()+100;
  float bounce=layout_scroll_delta(0,60,0,100);
  assert(bounce<0 && bounce>=-72 && layout_scroll_assentar(bounce,0,100,.016f)==bounce);
  nv_desktop_wheel_until=0;assert(layout_scroll_assentar(bounce,0,100,.016f)>bounce);
  SDL_Delay(190);wheel(10,200,60,0);assert(opens==1 && moves==2);
  SDL_Delay(190);wheel(1275,200,-60,0);assert(saved==1);
  SDL_Delay(190);allowSwipes=0;row=1;wheel(10,200,40,0);assert(opens==1 && moves==3);
  allowSwipes=1;panel=0;row=0;
  phased(500,200,0,0,1); // Zero-delta begin still preserves the origin.
  phased(500,200,60,0,0);assert(backs==1);
  SDL_Delay(190);phased(500,200,100,0,0);assert(backs==1);
  phased(500,200,0,0,2);phased(500,200,100,0,8);phased(500,200,0,0,8|2);
  assert(backs==1);
  phased(500,200,60,0,1);assert(backs==2);
  phased(500,200,0,0,2);
  row=1;rowStart=0;int beforeMoves=moves;
  phased(500,200,30,0,1);phased(500,200,60,0,0);
  // Row reached its start during this gesture: it must retain ownership.
  phased(500,200,100,0,8);assert(backs==2 && moves==beforeMoves+3);
  phased(500,200,0,0,8|2);
  phased(500,200,60,0,1);assert(backs==3); // Fresh gesture at row start.
  phased(500,200,0,0,2);
  row=0;
  phased(500,200,-100,0,1);assert(backs==3 && saved==2 && panel); // No forward history: Saved.
  phased(500,200,0,0,2);spainel_fechar();saved=1;
  phased(1275,200,0,0,1);phased(800,200,-60,0,0);assert(saved==2 && panel);
  phased(800,200,-100,0,8);assert(saved==2);
  phased(800,200,0,0,8|2);
  phased(900,200,60,0,1);assert(!panel && backs==3);
  phased(900,200,0,0,2);
  phased(10,200,60,0,1);assert(opens==2 && backs==3);
  phased(10,200,0,0,2);
  phased(500,200,20,0,1);phased(500,200,0,0,4);
  phased(500,200,100,0,8);assert(backs==3);
  phased(500,200,0,-100,1);assert(backs==3);
  phased(500,200,0,0,2);
  e.type=SDL_MOUSEBUTTONDOWN;e.button.x=200;e.button.y=200;e.button.button=SDL_BUTTON_LEFT;
  ponteiro_evento(&e,deliver);assert(focuses==2 && arrows==1);
  e.button.button=SDL_BUTTON_RIGHT;ponteiro_evento(&e,deliver);
  assert(focuses==3 && arrows==2 && !ponteiro_ok_longo());
  ponteiro_quadro(SDL_GetTicks());
  ponteiro_alvo(100,100,500,500,focus,NULL,0,0);
  ponteiro_alvo_segurar(context);fecharQuadro();
  ponteiro_evento(&e,deliver);
  assert(contexts==1 && focuses==3 && arrows==2);
  // Real row consumer, redrawn between events: no per-frame clamp or snap.
  realScroll=1;realOffset=100;
  ponteiro_quadro(SDL_GetTicks());ponteiro_rolagem(scrolling);fecharQuadro();
  toquerol_vincular(&preciseRow,(GfxRect){0,0,1920,1200},1,0,110,0,&realOffset);
  phased(500,200,-.25f,0,NV_WHEEL_BEGIN);
  assert(fabsf(realOffset-(100+.25f*NV_TELA_W/1280))<.001f);
  phased(500,200,0,0,NV_WHEEL_END); // Finger up precedes OS momentum.
  for(int i=0;i<8;i++) {
    float before=realOffset;
    phased(500,200,-.125f,0,NV_WHEEL_MOMENTUM);
    toquerol_vincular(&preciseRow,(GfxRect){0,0,1920,1200},1,0,110,0,&realOffset);
    assert(fabsf(realOffset-before-.125f*NV_TELA_W/1280)<.001f);
    assert(layout_scroll_assentar(realOffset,0,110,.016f)==realOffset);
  }
  phased(500,200,-40,0,NV_WHEEL_MOMENTUM);
  assert(realOffset>110 && realOffset<=182);
  float stretched=realOffset;
  for(int i=0;i<80;i++) {
    float previous=realOffset;phased(500,500,-2,0,NV_WHEEL_MOMENTUM);
    assert(realOffset>previous);
  }
  stretched=realOffset;
  toquerol_vincular(&preciseRow,(GfxRect){0,0,1920,1200},1,0,110,0,&realOffset);
  assert(realOffset==stretched);
  phased(500,200,0,0,NV_WHEEL_MOMENTUM|NV_WHEEL_END);
  assert(!nv_desktop_scroll_active && !inRolagem);
  nv_desktop_wheel_until=0;
  for(int i=0;i<120;i++) realOffset=layout_scroll_assentar(realOffset,0,110,1.0f/60);
  assert(realOffset==110);
  realOffset=50;
  phased(500,200,-.2f,0,NV_WHEEL_UNPHASED);
  assert(fabsf(realOffset-(50+.2f*NV_TELA_W/1280))<.001f && !inRolagem);
  float beforeWheel=realOffset;
  e=(SDL_Event){0};e.type=SDL_MOUSEWHEEL;e.wheel.preciseX=-1;
  ponteiro_evento(&e,deliver);
  assert(realOffset==beforeWheel && inRolagem);
  inerciaQuadro(SDL_GetTicks()+16);
  assert(realOffset>beforeWheel);
  pararInercia();
  // Draw at enlarged text scale, deliver outside that layer, cross into another row.
  ponteiro_quadro(SDL_GetTicks());drawScale=1.5f;ponteiro_rolagem(scaledRows);fecharQuadro();drawScale=1;
  pixels=0;
  phased(500,200,-1,0,NV_WHEEL_BEGIN);
  assert(capturedScale==1.5f && latchedRow==1);
  phased(500,500,-2,0,0);
  phased(500,500,0,0,NV_WHEEL_END);
  phased(500,600,-.25f,0,NV_WHEEL_MOMENTUM);
  assert(latchedRow==1 && fabsf(pixels+3.25f)<.001f && drawScale==1);
  phased(500,600,0,0,NV_WHEEL_MOMENTUM|NV_WHEEL_END);
  // One click must focus and activate an initially unfocused card.
  ponteiro_quadro(SDL_GetTicks());ponteiro_alvo(100,100,500,500,focus,context,8,9);fecharQuadro();
  int oldContexts=contexts;
  e=(SDL_Event){0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=200;e.button.y=200;
  ponteiro_evento(&e,deliver);e.type=SDL_MOUSEBUTTONUP;ponteiro_evento(&e,deliver);
  assert(contexts==oldContexts+1);
  // REAL INPUT: the owner's recorded session (0 OS momentum events) replayed
  // through the exact Cocoa monitor encoding, at a 16 ms display refresh.
  ponteiro_teste_relogio(fakeClock);fakeNow=100000;replayOffset=0;
  int startsBefore=notes("event=momentum source=synthetic state=start");
  FILE *log=fopen("tests/fixtures/desktop-trackpad-dono-2203.txt","r");
  assert(log);
  char line[256];int ms,ph,mph,x,y,events=0,flings=0,coasted=0;float dx,dy;
  Uint32 base=fakeNow,endedAt=0;float endedOffset=0,lastStep=0;int decaying=0;
  while(fgets(line,sizeof line,log)) {
    if(line[0]=='#') continue;
    assert(sscanf(line,"%d %d %d %f %f %d %d",&ms,&ph,&mph,&dx,&dy,&x,&y)==7);
    assert(mph==0);
    while(fakeNow+16<=base+(Uint32)ms) {
      fakeNow+=16;float before=replayOffset;replayFrame();
      float step=fabsf(replayOffset-before);
      if(decaying && inRolagem) {
        // Smooth: every refresh moves, each step no larger than the previous.
        assert(step>0 && step<=lastStep*1.0001f+.0001f);
        if(fakeNow-endedAt>=300 && replayOffset!=endedOffset) coasted|=1;
      }
      lastStep=step;
      if(decaying && !inRolagem) decaying=0;
    }
    fakeNow=base+(Uint32)ms;
    cocoa(x,y,dx,dy,ph,mph);events++;
    if(ph==8) {
      endedAt=fakeNow;endedOffset=replayOffset;
      if(inRolagem) { flings++;decaying=1;lastStep=1e9f; }
    }
  }
  fclose(log);
  for(int i=0;i<400 && inRolagem;i++) { fakeNow+=16;replayFrame(); }
  assert(events==677 && flings>=20 && coasted && !inRolagem);
  assert(notes("event=momentum source=synthetic state=start")-startsBefore==flings);
  assert(notes("event=momentum source=synthetic state=rest")>=1);
  // The first real flick keeps moving past 300 ms after phase=8 and rests.
  fakeNow+=1000;replayOffset=0;base=fakeNow;
  log=fopen("tests/fixtures/desktop-trackpad-dono-2203.txt","r");
  while(fgets(line,sizeof line,log)) {
    if(line[0]=='#') continue;
    sscanf(line,"%d %d %d %f %f %d %d",&ms,&ph,&mph,&dx,&dy,&x,&y);
    fakeNow=base+(Uint32)ms;replayFrame();cocoa(x,y,dx,dy,ph,mph);
    if(ph==8) break;
  }
  fclose(log);
  endedOffset=replayOffset;endedAt=fakeNow;float at300=0;
  while(inRolagem) { fakeNow+=16;replayFrame();if(fakeNow-endedAt<=300) at300=replayOffset; }
  assert(fabsf(at300-endedOffset)>50 && fabsf(replayOffset-at300)>1 && fakeNow-endedAt>300);
  printf("PASS: owner's real flick coasts %u ms after phase=8 (%.0f px) and decays to rest\n",fakeNow-endedAt,fabsf(replayOffset-endedOffset));
  // OS momentum, when AppKit does send it, takes over without a double push.
  int handoffs=notes("event=momentum source=os state=handoff");
  cocoa(451,384,0,40,1,0);fakeNow+=8;cocoa(451,384,0,60,4,0);cocoa(451,384,0,0,8,0);
  assert(inRolagem);float handed=replayOffset;
  fakeNow+=8;cocoa(451,384,0,30,0,1);
  assert(!inRolagem && fabsf(fabsf(replayOffset-handed)-30*NV_TELA_W/1280)<.01f);
  assert(notes("event=momentum source=os state=handoff")==handoffs+1);
  cocoa(451,384,0,0,0,8);
  // Horizontal-dominant two-finger swipe: Back exactly once for the gesture.
  realScroll=0;row=0;ponteiro_quadro(fakeNow);ponteiro_rolagem(scrolling);fecharQuadro();
  int backsBefore=backs;fakeNow+=500;
  cocoa(640,400,3,1,1,0);
  for(int i=0;i<12;i++) { fakeNow+=8;cocoa(640,400,9,1,4,0); }
  cocoa(640,400,0,0,8,0);
  assert(backs==backsBefore+1 && notes("action=back")>=1);
  // Right-edge inward swipe opens the Saved panel.
  panel=0;int savedBefore=saved;fakeNow+=500;
  cocoa(1270,400,-2,0,1,0);
  for(int i=0;i<8;i++) { fakeNow+=8;cocoa(1270,400,-12,1,4,0); }
  cocoa(1270,400,0,0,8,0);
  assert(saved==savedBefore+1 && panel && notes("action=open-panel")>=1);
  puts("PASS: OS momentum hands over once; horizontal two-finger swipe = one Back; right-edge inward swipe opens Saved");
  ponteiro_teste_relogio(NULL);
  puts("PASS: 1x/2x and three window sizes, scaled callback, latched row across pointer movement and momentum, single click");
  puts("PASS: precise deltas and momentum remain fractional across redraw; rubber-band survives redraw and settles exactly at bound");
  puts("PASS: hover updates focus without scrolling; fractional pixels; no Mac double momentum; left/right edges; one Back per phase sequence; row ownership; panel reverse; cancelled momentum; disabled swipe preference; click activation");
  SDL_Quit();return 0;
}
