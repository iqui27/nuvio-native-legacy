// Generated app-local NSEvents exercise the real monitor -> SDL boundary.
// This does not claim to be a physical trackpad recording.
#import <Cocoa/Cocoa.h>
#include <SDL2/SDL_syswm.h>
#include <assert.h>
#include "../src/desktop_native.h"
#include "../src/gfx.h"
// This harness checks event routing; Settings GL rendering has its own pixel test.
int perftv_enabled, perftv_uploads;
float gfx_escala(void) { return 1; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float x, float y, float radius,
              float red, float green, float blue, float alpha) {
  (void)r; (void)t; (void)m; (void)f; (void)x; (void)y; (void)radius;
  (void)red; (void)green; (void)blue; (void)alpha;
}
void menu_desktop_hover(float x) { (void)x; }
// Native sidebar (fbb448224, 6fb8eee66) calls into the app; inert stubs for this harness.
void menu_desktop_alternar(void) {}
void menu_desktop_expandir(void) {}
void menu_desktop_servico(int i) { (void)i; }
void menu_desktop_perfil(void) {}
int menu_desktop_foco(void) { return -2; }
void menu_fechar(void) {}
void menu_ir(int d) { (void)d; }
int menu_destino(void) { return 0; }
int home_streaming_barra(const int **p) { if (p) *p = 0; return 0; }
const char *i18n(const char *s) { return s; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1; return 0; }
@interface NVProofScroll : NSEvent
@property(assign) NSWindow *proofWindow;
@property NSPoint proofPoint;
@property NSEventPhase proofPhase, proofMomentum;
@property CGFloat proofDY;
@end
@implementation NVProofScroll
- (NSEventType)type { return NSEventTypeScrollWheel; }
- (NSWindow *)window { return self.proofWindow; }
- (NSPoint)locationInWindow { return self.proofPoint; }
- (BOOL)hasPreciseScrollingDeltas { return YES; }
- (CGFloat)scrollingDeltaX { return 0; }
- (CGFloat)scrollingDeltaY { return self.proofDY; }
- (NSEventPhase)phase { return self.proofPhase; }
- (NSEventPhase)momentumPhase { return self.proofMomentum; }
@end
int main(void) { @autoreleasepool {
  setenv("NV_CAPTURA","1",1);
  SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
  SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN,"1");
  pid_t front=NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier;
  assert(!SDL_Init(SDL_INIT_VIDEO));
  SDL_Window *window=SDL_CreateWindow("Nuvio native input proof",0,0,1440,900,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_ALLOW_HIGHDPI);
  assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
  desktop_native_init(window);
  SDL_SysWMinfo info;SDL_VERSION(&info.version);assert(SDL_GetWindowWMInfo(window,&info));
  NSWindow *native=info.info.cocoa.window;
  NSView *view=native.contentView;
  assert(NSApp.activationPolicy==NSApplicationActivationPolicyAccessory);
  assert([view acceptsFirstMouse:nil]);
  NSView *overlay=nil;
  for(NSView *candidate in view.superview.subviews)
    if([NSStringFromClass(candidate.class) isEqualToString:@"NVSidebarHitView"]) overlay=candidate;
  assert(overlay && overlay.hidden);
  overlay.frame=NSMakeRect(8,8,200,600);overlay.hidden=NO;overlay.alphaValue=1;
  assert([overlay hitTest:NSMakePoint(600,300)]==nil);
  assert([overlay hitTest:NSMakePoint(100,300)]==nil);
  NSButton *button=[NSButton buttonWithTitle:@"Test" target:nil action:nil];
  button.frame=NSMakeRect(10,10,100,30);[overlay addSubview:button];
  assert([overlay hitTest:NSMakePoint(30,30)]==button);
  overlay.hidden=YES;assert([overlay hitTest:NSMakePoint(30,30)]==nil);
  overlay.hidden=NO;overlay.alphaValue=0;assert([overlay hitTest:NSMakePoint(30,30)]==nil);
  overlay.hidden=YES;
  for(int i=0;i<3;i++) {
    const int sizes[][2]={{1280,800},{1440,900},{2560,1440}};
    SDL_SetWindowSize(window,sizes[i][0],sizes[i][1]);SDL_PumpEvents();
    int w,h,dw,dh;SDL_GetWindowSize(window,&w,&h);SDL_GL_GetDrawableSize(window,&dw,&dh);
    assert(w==sizes[i][0] && h==sizes[i][1]);
    assert(view.bounds.size.width==w && view.bounds.size.height==h);
    printf("NATIVE geometry: points=%dx%d drawable=%dx%d\n",w,h,dw,dh);
  }
  const unsigned phases[]={1,4,8,0,0,0};
  const unsigned momentum[]={0,0,0,1,4,8};
  const float deltas[]={-.25f,-12,0,-4,-.125f,0};
  for(int i=0;i<6;i++) {
    SDL_Event ignored;while(SDL_PollEvent(&ignored)) {}
    NVProofScroll *event=[NVProofScroll new];event.proofWindow=i>=3?nil:native;
    event.proofPoint=[view convertPoint:NSMakePoint(700,view.bounds.size.height-350) toView:nil];
    event.proofPhase=phases[i];event.proofMomentum=momentum[i];event.proofDY=deltas[i];
    [NSApp sendEvent:event];[event release];
    int found=0;
    SDL_Event e;while(SDL_PollEvent(&e)) if(e.type==SDL_MOUSEWHEEL) {
      assert(e.wheel.which==NV_DESKTOP_PIXEL_WHEEL);
      assert(e.wheel.mouseX==700 && e.wheel.mouseY==350);
      assert(e.wheel.preciseY==deltas[i]);
      assert((e.wheel.y&255)==phases[i] && ((e.wheel.y>>8)&255)==momentum[i]);
      assert(e.wheel.y!=0 || phases[i]==momentum[i]);
      if(i==3) assert((e.wheel.x&NV_WHEEL_MOMENTUM) && !(e.wheel.x&NV_WHEEL_BEGIN));
      found++;
    }
    assert(found==1);
  }
  for(int right=0;right<2;right++) for(int down=1;down>=0;down--) {
    NSEventType type=right?(down?NSEventTypeRightMouseDown:NSEventTypeRightMouseUp):(down?NSEventTypeLeftMouseDown:NSEventTypeLeftMouseUp);
    NSPoint point=[view convertPoint:NSMakePoint(700,view.bounds.size.height-350) toView:nil];
    NSEvent *event=[NSEvent mouseEventWithType:type location:point modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil eventNumber:1 clickCount:1 pressure:down];
    [NSApp sendEvent:event];
    int found=0;SDL_Event e;
    while(SDL_PollEvent(&e)) if(e.type==(down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP)) {
      assert(e.button.x==700 && e.button.y==350);
      assert(e.button.button==(right?SDL_BUTTON_RIGHT:SDL_BUTTON_LEFT));found++;
    }
    assert(found==1);
  }
  assert(NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier==front);
  [native orderOut:nil];SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
  puts("PASS: app-local NSEvent monitor preserves precise deltas, both phases, point coordinates and first left/right click; foreground unchanged");
} return 0; }
