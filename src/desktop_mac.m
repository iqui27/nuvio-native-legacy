#if defined(NV_DESKTOP_UI) && defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <QuartzCore/QuartzCore.h>
#include <SDL2/SDL_syswm.h>
#include "desktop_native.h"
#include "menu.h"
#include "colecoes.h"
#include "home.h"
#include "perfis.h"
#include "idioma.h"
#include "esportes.h"
#include "tex_cache.h"
static NSWindow *nativeWindow;
static NSView *sidebar, *sidebarControls;
static NSView *sidebarBackdrop;
static NSBitmapImageRep *sidebarPixels;
static NSMutableArray<NSTextField *> *groupLabels;
static NSTextField *profileChevron;
static float sidebarFraction, sidebarReveal;
static id hoverMonitor;
static id scrollMonitor;
static NSMutableArray<NSButton *> *rows;
static NSButton *profile;
static NSMutableArray<NSButton *> *services;
static NSScrollView *serviceScroll;
static NSView *serviceContent;
// Keep every symbol centred in one column, independent of its intrinsic width.
@interface NVSidebarCell : NSButtonCell
@end
@implementation NVSidebarCell
- (NSRect)imageRectForBounds:(NSRect)bounds {
  CGFloat size = self.image.size.width;
  return NSMakeRect(bounds.origin.x + (24-size)*.5, NSMidY(bounds)-size*.5, size, size);
}
- (NSRect)titleRectForBounds:(NSRect)bounds {
  NSRect rect = [super titleRectForBounds:bounds];
  rect.origin.x = bounds.origin.x + 32; rect.size.width = MAX(0,bounds.size.width-44);
  return rect;
}
@end
@interface NVSidebarButton : NSButton
@end
@implementation NVSidebarButton
+ (Class)cellClass { return NVSidebarCell.class; }
- (void)mouseEntered:(NSEvent *)event { self.layer.backgroundColor = [NSColor.labelColor colorWithAlphaComponent:.08].CGColor; [[NSCursor pointingHandCursor] set]; }
- (void)mouseExited:(NSEvent *)event { self.layer.backgroundColor = (self.tag==menu_destino()?[NSColor.controlAccentColor colorWithAlphaComponent:.18]:NSColor.clearColor).CGColor; [[NSCursor arrowCursor] set]; }
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
  serviceContent=[NSView new];serviceScroll.documentView=serviceContent;[sidebarControls addSubview:serviceScroll];
  const int destinations[]={MENU_BUSCAR,MENU_INICIO,MENU_BIBLIOTECA,MENU_GUIA,MENU_LISTAS,MENU_CONQUISTAS,MENU_JUNTOS,MENU_ESPORTES,MENU_STREAMING,MENU_FONTES,MENU_AJUSTES};
  NSArray *symbols=@[@"magnifyingglass",@"house",@"square.stack",@"tv",@"list.bullet",@"trophy",@"person.2",@"sportscourt",@"play.rectangle",@"puzzlepiece.extension",@"gearshape"];
  for(unsigned i=0;i<sizeof destinations/sizeof *destinations;i++) {
    NVSidebarButton *button=[NVSidebarButton new]; button.tag=destinations[i];
    button.title=[NSString stringWithUTF8String:i18n(menu_rotulo(destinations[i]))];
    button.image=[NSImage imageWithSystemSymbolName:symbols[i] accessibilityDescription:button.title];
    button.image.size=NSMakeSize(18,18);
    button.imagePosition=NSImageLeft; button.imageHugsTitle=NO; button.alignment=NSTextAlignmentLeft;
    button.font=[NSFont systemFontOfSize:15 weight:NSFontWeightRegular];
    button.bordered=NO; button.bezelStyle=NSBezelStyleInline; button.contentTintColor=NSColor.controlAccentColor;
    button.cell.lineBreakMode=NSLineBreakByTruncatingTail;
    button.cell.usesSingleLineMode=YES;
    button.attributedTitle=[[NSAttributedString alloc] initWithString:button.title attributes:@{NSForegroundColorAttributeName:NSColor.labelColor,NSFontAttributeName:button.font}];
    button.target=button; button.action=@selector(activate:);button.wantsLayer=YES;button.layer.cornerRadius=8;
    [sidebarControls addSubview:button];[rows addObject:button];
  }
  profile=[NVSidebarButton new];profile.tag=-1;profile.bordered=NO;profile.alignment=NSTextAlignmentLeft;
  profile.font=[NSFont systemFontOfSize:15 weight:NSFontWeightSemibold];profile.target=profile;profile.action=@selector(activate:);
  profile.image=[NSImage imageWithSystemSymbolName:@"person.crop.circle.fill" accessibilityDescription:nil];
  profile.wantsLayer=YES;profile.layer.cornerRadius=6;
  [sidebarControls addSubview:profile];
  groupLabels=[NSMutableArray new];
  for (NSString *title in @[@"Browse", @"You", @"Services"]) {
    NSTextField *label=[NSTextField labelWithString:title];
    label.font=[NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
    label.textColor=NSColor.secondaryLabelColor;
    [sidebarControls addSubview:label];[groupLabels addObject:label];
  }
  profileChevron=[NSTextField labelWithString:@"›"];
  profileChevron.textColor=NSColor.secondaryLabelColor;
  [sidebarControls addSubview:profileChevron];
}
void desktop_native_init(SDL_Window *window) {
  SDL_SysWMinfo info; SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info)) return;
  nativeWindow = info.info.cocoa.window;
  BOOL capture = getenv("NV_CAPTURA") != NULL;
  [NSApp setActivationPolicy:capture ? NSApplicationActivationPolicyAccessory : NSApplicationActivationPolicyRegular];
  if (capture) {
    nativeWindow.hidesOnDeactivate = NO;
    [nativeWindow orderBack:nil];
  } else {
    [NSApp activateIgnoringOtherApps:YES];
    [nativeWindow makeKeyAndOrderFront:nil];
  }
  nativeWindow.titlebarAppearsTransparent = YES;
  nativeWindow.titleVisibility = NSWindowTitleHidden;
  nativeWindow.styleMask |= NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView;
  nativeWindow.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
  nativeWindow.opaque = NO;
  nativeWindow.backgroundColor = NSColor.clearColor;
  NSView *content = nativeWindow.contentView;
  [content setWantsLayer:YES];
  content.layer.opaque = NO;
  NSView *container = content.superview;
  container.wantsLayer = YES;
  // SDL's GL surface must stay inside the titled window's system shape.
  CGFloat radius = container.layer.cornerRadius;
  if (radius <= 0) radius = 16;
  content.layer.cornerRadius = radius;
  content.layer.masksToBounds = YES;
  container.layer.cornerRadius = radius;
  container.layer.masksToBounds = YES;
  sidebarControls = [NSView new];
  if (@available(macOS 26.0, *)) {
    NSGlassEffectView *glass = [NSGlassEffectView new];
    glass.cornerRadius = 16;
    glass.style = NSGlassEffectViewStyleClear;
    glass.tintColor = NSColor.clearColor;
    glass.contentView = sidebarControls;
    sidebar = glass;
  } else {
    NSVisualEffectView *effect = [NSVisualEffectView new];
    effect.material = NSVisualEffectMaterialSidebar;
    effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    effect.state = NSVisualEffectStateFollowsWindowActiveState;
    sidebar = effect;
    [sidebar addSubview:sidebarControls];
  }
  sidebar.wantsLayer = YES;
  sidebar.layer.cornerRadius = 16;
  sidebar.layer.masksToBounds = YES;
  sidebarBackdrop = [NSView new];
  sidebarBackdrop.wantsLayer = YES;
  sidebarBackdrop.layer.cornerRadius = 16;
  sidebarBackdrop.layer.masksToBounds = YES;
  [container addSubview:sidebarBackdrop positioned:NSWindowAbove relativeTo:content];
  [container addSubview:sidebar positioned:NSWindowAbove relativeTo:sidebarBackdrop];
  sidebarBackdrop.layer.shadowOpacity=.18;
  sidebarBackdrop.layer.shadowRadius=12;
  sidebarBackdrop.layer.shadowOffset=CGSizeMake(0,-3);
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
  nativeWindow.acceptsMouseMovedEvents=YES;
  hoverMonitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMouseMoved|NSEventMaskLeftMouseDragged handler:^NSEvent *(NSEvent *event) {
    if(event.window==nativeWindow) menu_desktop_hover(event.locationInWindow.x);
    return event;
  }];
  printf("[desktop-native] window=%ld material=Sidebar transparent=1 glass=%d radius=%.1f\n", (long)nativeWindow.windowNumber, [sidebar isKindOfClass:NSClassFromString(@"NSGlassEffectView")], radius);
}
void desktop_native_sidebar(float fraction, float reveal) {
  if (getenv("NV_CAPTURA_GL")) fraction=0;
  sidebarFraction=fraction; sidebarReveal=reveal;
  if (!sidebar || fraction<=0) return;
  // Preserve the live rendered scene beneath glass before GL clears its hole.
  // Every frame is copied, including hero transitions and scrolling.
  GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
  int width=MAX(1,(int)(viewport[2]*fraction)), height=viewport[3];
  if (sidebarPixels.pixelsWide!=width || sidebarPixels.pixelsHigh!=height) {
    [sidebarPixels release];
    sidebarPixels=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:width*4 bitsPerPixel:32];
  }
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,sidebarPixels.bitmapData);
  // GL rows start at the bottom. Normalize once before publishing the image;
  // never animate a layer transform through an inverted intermediate frame.
  unsigned char *pixels=sidebarPixels.bitmapData;
  size_t stride=(size_t)width*4;
  for (int y=0;y<height/2;y++) {
    unsigned char *top=pixels+y*stride,*bottom=pixels+(height-1-y)*stride;
    for (size_t x=0;x<stride;x++) { unsigned char v=top[x];top[x]=bottom[x];bottom[x]=v; }
  }
  [CATransaction begin]; [CATransaction setDisableActions:YES];
  sidebarBackdrop.layer.contents=(id)sidebarPixels.CGImage;
  [CATransaction commit];
}
void desktop_native_finish(void) {
  float fraction=sidebarFraction;
  if (!sidebar) return;
  if(fraction<=0) {sidebar.hidden=YES;sidebarBackdrop.hidden=YES;return;}
  [CATransaction begin]; [CATransaction setDisableActions:YES];
  NSRect bounds=nativeWindow.contentView.bounds;
  BOOL fullscreen=(nativeWindow.styleMask & NSWindowStyleMaskFullScreen)!=0;
  nativeWindow.contentView.layer.cornerRadius=fullscreen?0:16;
  nativeWindow.contentView.superview.layer.cornerRadius=fullscreen?0:16;
  CGFloat titleInset=fullscreen?8:36;
  NSRect frame=NSMakeRect(8-(1-sidebarReveal)*bounds.size.width*fraction,8,MAX(1,bounds.size.width*fraction-16),bounds.size.height-titleInset-8);
  sidebar.alphaValue=.86f*sidebarReveal; sidebarBackdrop.alphaValue=sidebarReveal;
  if(!NSEqualRects(sidebar.frame,frame)) sidebar.frame = frame;
  if(sidebar.hidden) sidebar.hidden=NO;
  sidebarControls.frame=sidebar.bounds;
  sidebarBackdrop.hidden=NO;
  sidebarBackdrop.frame=frame;
  sidebarBackdrop.layer.contentsRect=CGRectMake(8/(bounds.size.width*fraction),titleInset/bounds.size.height,frame.size.width/(bounds.size.width*fraction),frame.size.height/bounds.size.height);
  sidebarRows();
  CGFloat w=frame.size.width,h=frame.size.height; BOOL collapsed=w<120;
  BOOL sportsEnabled=NO;for(int i=0;i<esportes_modalidades_n();i++)sportsEnabled|=esportes_habilitado(i);
  for(NSButton *row in rows) {
    NSUInteger i=[rows indexOfObjectIdenticalTo:row];
    row.hidden=row.tag==MENU_ESPORTES&&!sportsEnabled;
    CGFloat top=12+i*35+(i>=1?28:0)+(i>=4?28:0)+(i>=8?28:0)-(i>=8&&!sportsEnabled?35:0);
    if(row.tag==MENU_AJUSTES) top=h-88;
    NSRect rowFrame=NSMakeRect(10,h-top-35,w-20,35);
    if(!NSEqualRects(row.frame,rowFrame)) row.frame=rowFrame;
    if(row.imagePosition!=(collapsed?NSImageOnly:NSImageLeft)) row.imagePosition=collapsed?NSImageOnly:NSImageLeft;
    BOOL hovered=NSPointInRect([row convertPoint:nativeWindow.mouseLocationOutsideOfEventStream fromView:nil],row.bounds);
    row.layer.backgroundColor=(row.tag==menu_destino()?[NSColor.controlAccentColor colorWithAlphaComponent:.18]:hovered?[NSColor.labelColor colorWithAlphaComponent:.08]:NSColor.clearColor).CGColor;
  }
  const CGFloat groupTop[]={55,188,356};
  for (NSUInteger i=0;i<groupLabels.count;i++)
    groupLabels[i].frame=NSMakeRect(16,h-groupTop[i]-14,w-32,14);
  profileChevron.frame=NSMakeRect(w-27,23,14,20);
  const ContaPerfil *active=perfis_item_ativo();
  NSString *profileTitle=active?[NSString stringWithUTF8String:active->nome]:[NSString stringWithUTF8String:i18n("Sua conta")];
  if(![profile.title isEqualToString:profileTitle]) profile.title=profileTitle;
  const char *avatarPath=active&&active->avatarUrl[0]?tex_arquivo(active->avatarUrl):NULL;
  NSString *avatarKey=[NSString stringWithFormat:@"%@:%s",profileTitle,avatarPath?avatarPath:""];
  static NSString *previousAvatar;
  if(![previousAvatar isEqualToString:avatarKey]) {
    [previousAvatar release]; previousAvatar=[avatarKey copy];
    NSImage *avatar=avatarPath?[[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:avatarPath]]:nil;
    NSMutableString *initials=[NSMutableString new];
    for(NSString *word in [profileTitle componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceCharacterSet]) {
      if(word.length) [initials appendString:[[word substringWithRange:[word rangeOfComposedCharacterSequenceAtIndex:0]] uppercaseString]];
      if(initials.length>=2) break;
    }
    profile.image=[NSImage imageWithSize:NSMakeSize(28,28) flipped:NO drawingHandler:^BOOL(NSRect rect) {
      [[NSBezierPath bezierPathWithOvalInRect:rect] addClip];
      if(avatar) [avatar drawInRect:rect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
      else {
        [NSColor.systemGrayColor setFill]; NSRectFill(rect);
        NSDictionary *attributes=@{NSFontAttributeName:[NSFont systemFontOfSize:11 weight:NSFontWeightSemibold],NSForegroundColorAttributeName:NSColor.whiteColor};
        NSSize size=[initials sizeWithAttributes:attributes];
        [initials drawAtPoint:NSMakePoint((28-size.width)*.5,(28-size.height)*.5) withAttributes:attributes];
      }
      return YES;
    }];
  }
  NSRect profileFrame=NSMakeRect(14,16,w-28,32);if(!NSEqualRects(profile.frame,profileFrame))profile.frame=profileFrame;
  if(profile.imagePosition!=(collapsed?NSImageOnly:NSImageLeft))profile.imagePosition=collapsed?NSImageOnly:NSImageLeft;
  const int *folders;int count=menu_desktop_streaming()?home_streaming_barra(&folders):0;
  while(services.count<(NSUInteger)count){
    NVSidebarButton *button=[NVSidebarButton new];button.bordered=NO;button.alignment=NSTextAlignmentLeft;
    button.font=[NSFont systemFontOfSize:13];button.target=button;button.action=@selector(activate:);
    button.tag=MENU_N+services.count;button.wantsLayer=YES;button.layer.cornerRadius=5;[serviceContent addSubview:button];[services addObject:button];
  }
  CGFloat serviceTop=h-12-9*35-84+(sportsEnabled?0:35);
  CGFloat serviceHeight=MIN(count*29,MAX(0,serviceTop-130));
  serviceScroll.frame=NSMakeRect(28,serviceTop-serviceHeight,w-38,serviceHeight);serviceScroll.hidden=!count||collapsed;
  serviceContent.frame=NSMakeRect(0,0,w-38,count*29);
  for(NSButton *button in services){
    int k=(int)[services indexOfObjectIdenticalTo:button];button.hidden=k>=count||collapsed;
    if(k<count){const ColFolder *folder=col_folder(folders[k]);button.title=folder?[NSString stringWithUTF8String:folder->title]:@"";}
    button.frame=NSMakeRect(4,count*29-(k+1)*29,w-48,28);
  }
  for(NSButton *row in rows) if(row.tag==MENU_FONTES){NSRect r=row.frame;r.origin.y-=serviceHeight;row.frame=r;}
  [CATransaction commit];
}
void desktop_native_appearance(const char *name) {
  if (getenv("NV_CAPTURA")) {
    NSAppearance *appearance=[NSAppearance appearanceNamed:!strcmp(name,"Aqua")?NSAppearanceNameAqua:NSAppearanceNameDarkAqua];
    NSApp.appearance=appearance;
    nativeWindow.appearance=appearance;sidebar.appearance=appearance;sidebarControls.appearance=appearance;
    nativeWindow.contentView.appearance=appearance;
    [nativeWindow.contentView setNeedsDisplay:YES];
    [sidebar setNeedsDisplay:YES];
    [sidebarControls setNeedsDisplay:YES];
    [nativeWindow displayIfNeeded];
  }
}
int desktop_native_dark(void) {
  return [[nativeWindow.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameDarkAqua, NSAppearanceNameAqua]] isEqualToString:NSAppearanceNameDarkAqua];
}
int desktop_native_swipes(void) { return NSEvent.isSwipeTrackingFromScrollEventsEnabled; }
#endif
