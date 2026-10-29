#if defined(NV_DESKTOP_UI) && defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include <SDL2/SDL_syswm.h>
#include "desktop_native.h"
#include "menu.h"
#include "colecoes.h"
#include "home.h"
#include "perfis.h"
#include "idioma.h"
#include "esportes.h"
static NSWindow *nativeWindow;
static NSVisualEffectView *sidebar;
static float sidebarFraction;
static id scrollMonitor;
static NSMutableArray<NSButton *> *rows;
static NSTextField *sectionLabel;
static NSButton *profile;
static NSMutableArray<NSButton *> *services;
static NSScrollView *serviceScroll;
static NSView *serviceContent;
@interface NVSidebarButton : NSButton
@end
@implementation NVSidebarButton
- (void)mouseEntered:(NSEvent *)event { self.layer.backgroundColor = [NSColor.labelColor colorWithAlphaComponent:.08].CGColor; [[NSCursor pointingHandCursor] set]; }
- (void)mouseExited:(NSEvent *)event { self.layer.backgroundColor = NSColor.clearColor.CGColor; [[NSCursor arrowCursor] set]; }
- (void)updateTrackingAreas {
  for (NSTrackingArea *area in self.trackingAreas) [self removeTrackingArea:area];
  [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:self.bounds options:NSTrackingMouseEnteredAndExited|NSTrackingActiveAlways|NSTrackingInVisibleRect owner:self userInfo:nil]];
  [super updateTrackingAreas];
}
- (void)activate:(id)sender {
  if(self.tag == MENU_STREAMING) { menu_ir(MENU_STREAMING);menu_desktop_expandir(); return; }
  if(self.tag >= MENU_N) {
    const int *folders; int n=home_streaming_barra(&folders), k=(int)self.tag-MENU_N;
    if(k<n) menu_desktop_servico(k);
    return;
  }
  if(self.tag==-1) { menu_desktop_perfil(); return; }
  menu_ir((int)self.tag); menu_fechar();
}
@end
static void sidebarRows(void) {
  if(rows) return;
  rows=[NSMutableArray new]; services=[NSMutableArray new];
  serviceScroll=[NSScrollView new];serviceScroll.drawsBackground=NO;serviceScroll.hasVerticalScroller=YES;serviceScroll.autohidesScrollers=YES;
  serviceContent=[NSView new];serviceScroll.documentView=serviceContent;[sidebar addSubview:serviceScroll];
  const int destinations[]={MENU_BUSCAR,MENU_INICIO,MENU_BIBLIOTECA,MENU_GUIA,MENU_LISTAS,MENU_CONQUISTAS,MENU_JUNTOS,MENU_ESPORTES,MENU_STREAMING,MENU_FONTES,MENU_AJUSTES};
  NSArray *symbols=@[@"magnifyingglass",@"house",@"square.stack",@"tv",@"list.bullet",@"trophy",@"person.2",@"sportscourt",@"play.rectangle",@"puzzlepiece.extension",@"gearshape"];
  for(unsigned i=0;i<sizeof destinations/sizeof *destinations;i++) {
    NVSidebarButton *button=[NVSidebarButton new]; button.tag=destinations[i];
    button.title=[NSString stringWithUTF8String:i18n(menu_rotulo(destinations[i]))];
    button.image=[NSImage imageWithSystemSymbolName:symbols[i] accessibilityDescription:button.title];
    button.imagePosition=NSImageLeft; button.imageHugsTitle=NO; button.alignment=NSTextAlignmentLeft;
    button.font=[NSFont systemFontOfSize:14 weight:i<4?NSFontWeightSemibold:NSFontWeightRegular];
    button.bordered=NO; button.bezelStyle=NSBezelStyleInline; button.contentTintColor=NSColor.controlAccentColor;
    button.attributedTitle=[[NSAttributedString alloc] initWithString:button.title attributes:@{NSForegroundColorAttributeName:NSColor.labelColor,NSFontAttributeName:button.font}];
    button.target=button; button.action=@selector(activate:);button.wantsLayer=YES;button.layer.cornerRadius=6;
    [sidebar addSubview:button];[rows addObject:button];
  }
  sectionLabel=[NSTextField labelWithString:[NSString stringWithUTF8String:i18n(menu_rotulo(MENU_BIBLIOTECA))]];
  sectionLabel.font=[NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];sectionLabel.textColor=NSColor.secondaryLabelColor;
  [sidebar addSubview:sectionLabel];
  profile=[NVSidebarButton new];profile.tag=-1;profile.bordered=NO;profile.alignment=NSTextAlignmentLeft;
  profile.font=[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold];profile.target=profile;profile.action=@selector(activate:);
  profile.image=[NSImage imageWithSystemSymbolName:@"person.crop.circle.fill" accessibilityDescription:nil];
  profile.wantsLayer=YES;profile.layer.cornerRadius=6;
  [sidebar addSubview:profile];
}
void desktop_native_init(SDL_Window *window) {
  SDL_SysWMinfo info; SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info)) return;
  nativeWindow = info.info.cocoa.window;
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:YES];
  [nativeWindow makeKeyAndOrderFront:nil];
  nativeWindow.titlebarAppearsTransparent = YES;
  nativeWindow.titleVisibility = NSWindowTitleHidden;
  nativeWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
  nativeWindow.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
  nativeWindow.opaque = NO;
  nativeWindow.backgroundColor = NSColor.clearColor;
  NSView *content = nativeWindow.contentView;
  [content setWantsLayer:YES];
  content.layer.opaque = NO;
  sidebar = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0, 0, 224, content.bounds.size.height)];
  sidebar.material = NSVisualEffectMaterialSidebar;
  sidebar.blendingMode = NSVisualEffectBlendingModeBehindWindow;
  sidebar.state = NSVisualEffectStateFollowsWindowActiveState;
  sidebar.autoresizingMask = NSViewHeightSizable;
  NSView *container = content.superview;
  [container addSubview:sidebar positioned:NSWindowAbove relativeTo:content];
  GLint opaque = 0;
  [[NSOpenGLContext currentContext] setValues:&opaque forParameter:NSOpenGLContextParameterSurfaceOpacity];
  // SDL normalizes Cocoa precise deltas into wheel ticks. Preserve native
  // pixels, phases and OS momentum in a tagged SDL event instead.
  scrollMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel handler:^NSEvent *(NSEvent *event) {
    if (event.window != nativeWindow || !event.hasPreciseScrollingDeltas) return event;
    SDL_Event wheel = {0}; wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.windowID = SDL_GetWindowID(window);
    wheel.wheel.which = NV_DESKTOP_PIXEL_WHEEL;
    wheel.wheel.preciseX = event.scrollingDeltaX;
    wheel.wheel.preciseY = event.scrollingDeltaY;
    wheel.wheel.y = event.phase == NSEventPhaseBegan ? 1 : 0;
    wheel.wheel.timestamp = SDL_GetTicks();
    NSPoint p = [content convertPoint:event.locationInWindow fromView:nil];
    wheel.wheel.mouseX = p.x; wheel.wheel.mouseY = content.bounds.size.height - p.y;
    SDL_PushEvent(&wheel);
    return nil;
  }];
  printf("[desktop-native] window=%ld material=Sidebar transparent=1\n", (long)nativeWindow.windowNumber);
}
void desktop_native_sidebar(float fraction) { sidebarFraction=fraction; }
void desktop_native_finish(void) {
  float fraction=sidebarFraction;
  if (!sidebar) return;
  if(fraction<=0) {sidebar.hidden=YES;return;}
  NSRect frame = sidebar.frame;
  frame.size.width = nativeWindow.contentView.bounds.size.width * fraction;
  if(!NSEqualRects(sidebar.frame,frame)) sidebar.frame = frame;
  if(sidebar.hidden) sidebar.hidden=NO;
  sidebarRows();
  CGFloat w=frame.size.width,h=frame.size.height; BOOL collapsed=w<120;
  BOOL sportsEnabled=NO;for(int i=0;i<esportes_modalidades_n();i++)sportsEnabled|=esportes_habilitado(i);
  for(NSButton *row in rows) {
    NSUInteger i=[rows indexOfObjectIdenticalTo:row];
    row.hidden=row.tag==MENU_ESPORTES&&!sportsEnabled;
    CGFloat top=52+i*31+(i>=4?26:0)+(i>=8?14:0)-(i>=8&&!sportsEnabled?31:0);
    if(row.tag==MENU_AJUSTES) top=h-82;
    NSRect rowFrame=NSMakeRect(10,h-top-30,w-20,30);
    if(!NSEqualRects(row.frame,rowFrame)) row.frame=rowFrame;
    if(row.imagePosition!=(collapsed?NSImageOnly:NSImageLeft)) row.imagePosition=collapsed?NSImageOnly:NSImageLeft;
    row.layer.backgroundColor=(row.tag==menu_destino()?[NSColor.labelColor colorWithAlphaComponent:.12]:NSColor.clearColor).CGColor;
  }
  const ContaPerfil *active=perfis_item_ativo();
  NSString *profileTitle=active?[NSString stringWithUTF8String:active->nome]:[NSString stringWithUTF8String:i18n("Sua conta")];
  if(![profile.title isEqualToString:profileTitle]) profile.title=profileTitle;
  NSRect profileFrame=NSMakeRect(14,16,w-28,32);if(!NSEqualRects(profile.frame,profileFrame))profile.frame=profileFrame;
  if(profile.imagePosition!=(collapsed?NSImageOnly:NSImageLeft))profile.imagePosition=collapsed?NSImageOnly:NSImageLeft;
  const int *folders;int count=menu_desktop_streaming()?home_streaming_barra(&folders):0;
  while(services.count<(NSUInteger)count){
    NVSidebarButton *button=[NVSidebarButton new];button.bordered=NO;button.alignment=NSTextAlignmentLeft;
    button.font=[NSFont systemFontOfSize:13];button.target=button;button.action=@selector(activate:);
    button.tag=MENU_N+services.count;button.wantsLayer=YES;button.layer.cornerRadius=5;[serviceContent addSubview:button];[services addObject:button];
  }
  CGFloat serviceTop=h-52-9*31-40;
  CGFloat serviceHeight=MIN(count*29,MAX(0,serviceTop-130));
  serviceScroll.frame=NSMakeRect(28,serviceTop-serviceHeight,w-38,serviceHeight);serviceScroll.hidden=!count||collapsed;
  serviceContent.frame=NSMakeRect(0,0,w-38,count*29);
  for(NSButton *button in services){
    int k=(int)[services indexOfObjectIdenticalTo:button];button.hidden=k>=count||collapsed;
    if(k<count){const ColFolder *folder=col_folder(folders[k]);button.title=folder?[NSString stringWithUTF8String:folder->title]:@"";}
    button.frame=NSMakeRect(4,count*29-(k+1)*29,w-48,28);
  }
  for(NSButton *row in rows) if(row.tag==MENU_FONTES){NSRect r=row.frame;r.origin.y-=serviceHeight;row.frame=r;}
  NSRect sectionFrame=NSMakeRect(16,h-52-4*31-20,w-32,18);
  if(!NSEqualRects(sectionLabel.frame,sectionFrame))sectionLabel.frame=sectionFrame;
  sectionLabel.hidden=collapsed;
}
int desktop_native_dark(void) {
  return [[nativeWindow.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameDarkAqua, NSAppearanceNameAqua]] isEqualToString:NSAppearanceNameDarkAqua];
}
int desktop_native_swipes(void) { return NSEvent.isSwipeTrackingFromScrollEventsEnabled; }
#endif
