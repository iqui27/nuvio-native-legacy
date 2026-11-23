#if defined(NV_DESKTOP_UI) && defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
int ajustes_idioma(void);
void ajustes_acento(float *r, float *g, float *b);
float ajustes_acento_tinta(float *r, float *g, float *b);
void gfx_arte_caminho(const char *rel, char *out, unsigned n);
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#include <SDL2/SDL_syswm.h>
#include "desktop_native.h"
#include "layout.h"
#include "gfx.h"
#include "menu.h"
#include "colecoes.h"
#include "home.h"
#include "perfis.h"
#include "idioma.h"
#include "esportes.h"
#include "tex_cache.h"
#include "layout.h"
#include "busca.h"
static NSSearchField *searchField;
static BOOL searchDrawn;
static Uint32 searchWindowID;
@interface NVSearchTarget : NSObject <NSSearchFieldDelegate>
@end
@implementation NVSearchTarget
- (void)controlTextDidChange:(NSNotification *)notification {
  busca_desktop_editar(searchField.stringValue.UTF8String);
}
- (BOOL)control:(NSControl *)control textView:(NSTextView *)view doCommandBySelector:(SEL)selector {
  SDL_Keycode key=selector==@selector(moveDown:)?SDLK_DOWN:
    selector==@selector(moveUp:)?SDLK_UP:selector==@selector(insertNewline:)?SDLK_RETURN:
    selector==@selector(cancelOperation:)?SDLK_ESCAPE:0;
  if(!key) return NO;
  [control.window makeFirstResponder:control.window.contentView];
  SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.windowID=searchWindowID;event.key.keysym.sym=key;
  SDL_PushEvent(&event);event.type=SDL_KEYUP;SDL_PushEvent(&event);
  return YES;
}
@end
#include "gfx.h"
#include "layout.h"
// Use real AppKit controls for system styling, including in GL readback proofs.
// Cache per visible catalog option; changing state/title invalidates only its texture.
int desktop_native_control(int slot, int kind, const char *title, int state, int enabled,
                           float x, float y, float w, float h, float alpha) {
  static struct { NSString *key; GLuint texture; } cache[1024];
  if (slot < 0 || slot >= 1024 || w <= 0 || h <= 0) return 0;
  @autoreleasepool {
    NSString *text = [NSString stringWithUTF8String:title ? title : ""];
    int ww = 1440, wh = 900;
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    if (window) SDL_GetWindowSize(window, &ww, &wh);
    float points = gfx_escala() / layout_window_units(ww, wh);
    NSRect bounds = NSMakeRect(0, 0, w * points, h * points);
    NSString *key = [NSString stringWithFormat:@"%d:%d:%d:%.2f:%.2f:%@", kind, state, enabled, bounds.size.width, bounds.size.height, text];
    if (![cache[slot].key isEqualToString:key]) {
      NSControl *control;
      if (kind == 0) {
        NSSwitch *toggle = [[NSSwitch alloc] initWithFrame:bounds];
        toggle.state = state ? NSControlStateValueOn : NSControlStateValueOff;
        control = toggle;
      } else if (kind == 1) {
        NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:bounds pullsDown:NO];
        [popup addItemWithTitle:text]; popup.bezelStyle = NSBezelStyleAccessoryBar;
        control = popup;
      } else if (kind == 4) {
        NSTextField *label = [[NSTextField labelWithString:text] retain];
        label.frame = bounds; label.textColor = NSColor.whiteColor;
        control = label;
      } else if (kind == 3) {
        NSSearchField *search = [[NSSearchField alloc] initWithFrame:bounds];
        search.placeholderString = text;
        control = search;
      } else {
        NSButton *button = [[NSButton alloc] initWithFrame:bounds];
        button.title = text; button.bezelStyle = NSBezelStyleAccessoryBar;
        control = button;
      }
      control.enabled = enabled;
      control.font = [NSFont systemFontOfSize:kind == 4 ? state : 13];
      control.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
      NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
        pixelsWide:(NSInteger)ceil(bounds.size.width * 2) pixelsHigh:(NSInteger)ceil(bounds.size.height * 2) bitsPerSample:8 samplesPerPixel:4
        hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:0
        bytesPerRow:(NSInteger)ceil(bounds.size.width * 2)*4 bitsPerPixel:32];
      bitmap.size = bounds.size;
      [control.appearance performAsCurrentDrawingAppearance:^{
        [control cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
      }];
      // Quartz draws premultiplied RGBA; the shared GL compositor uses straight alpha.
      for (NSInteger i = 0; i < bitmap.pixelsWide * bitmap.pixelsHigh; i++) {
        unsigned char *p = bitmap.bitmapData + i * 4;
        if (p[3]) for (int c = 0; c < 3; c++) p[c] = MIN(255, p[c] * 255 / p[3]);
      }
      GLint binding; glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
      if (!cache[slot].texture) glGenTextures(1, &cache[slot].texture);
      glBindTexture(GL_TEXTURE_2D, cache[slot].texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)bitmap.pixelsWide, (GLsizei)bitmap.pixelsHigh,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.bitmapData);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glBindTexture(GL_TEXTURE_2D, (GLuint)binding);
      [cache[slot].key release]; cache[slot].key = [key copy];
      [bitmap release]; [control release];
    }
    gfx_rect((GfxRect){x,y,w,h}, cache[slot].texture, GFX_TEXTO, 0, 0, 0, 0, 1,1,1,alpha);
    return 1;
  }
}
static NSWindow *nativeWindow;
static NSView *sidebar, *sidebarControls;
static NSView *sourceControls;
static BOOL sourceActive;
static void (*sourceActivate)(int);
@interface NVSourceControls : NSView
@end
@implementation NVSourceControls
- (BOOL)isFlipped { return YES; }
- (NSView *)hitTest:(NSPoint)p {
  NSView *hit=[super hitTest:p];
  return [hit isKindOfClass:NSControl.class] ? hit : nil;
}
- (void)activate:(NSButton *)sender { if(sourceActivate) sourceActivate((int)sender.tag); }
@end
unsigned desktop_native_sources(const DesktopSourceControl *controls, int count, void (*activate)(int),
                                float x, float y, float w, float h, float scale, float alpha) {
  static DesktopSourceControl previous[32];
  static int previousCount=-1;
  static float previousW, previousH;
  static GLuint texture;
  sourceActive=controls && count>0 && alpha>.005f;
  if(!sourceActive) { sourceControls.hidden=YES; return 0; }
  if(!nativeWindow || count>32 || scale<=0) return 0;
  if(!sourceControls) {
    sourceControls=[NVSourceControls new];
    sourceControls.appearance=[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    sourceControls.accessibilityLabel=[NSString stringWithUTF8String:i18n("Fontes")];
    sourceControls.accessibilityRole=NSAccessibilityGroupRole;
    [nativeWindow.contentView.superview addSubview:sourceControls positioned:NSWindowAbove relativeTo:sidebar];
  }
  sourceActivate=activate;
  NSView *content=nativeWindow.contentView;
  scale*=content.bounds.size.width/nv_layout_w;
  NSRect local=NSMakeRect(x*scale,content.isFlipped?y*scale:content.bounds.size.height-(y+h)*scale,w*scale,h*scale);
  sourceControls.frame=[content convertRect:local toView:sourceControls.superview];
  sourceControls.bounds=NSMakeRect(0,0,w,h);
  sourceControls.alphaValue=alpha;
  sourceControls.hidden=getenv("NV_CAPTURA_GL")!=NULL;
  BOOL changed=previousCount!=count || previousW!=w || previousH!=h || memcmp(previous,controls,sizeof(*controls)*count);
  if(changed) {
    NSArray *oldSubviews=[sourceControls.subviews copy];
    for(NSView *v in oldSubviews) [v removeFromSuperview];
    [oldSubviews release];
    for(int i=0;i<count;i++) {
      const DesktopSourceControl *c=&controls[i];
      NSButton *b=[NSButton buttonWithTitle:[NSString stringWithUTF8String:c->title] target:sourceControls action:@selector(activate:)];
      b.tag=c->action; b.frame=NSMakeRect(c->x,c->y,c->w,c->h);
      b.font=[NSFont systemFontOfSize:18 weight:NSFontWeightMedium];
      if(@available(macOS 26.0,*)) b.controlSize=NSControlSizeExtraLarge;
      else if(@available(macOS 11.0,*)) b.controlSize=NSControlSizeLarge;
      b.bezelStyle=c->kind==2?NSBezelStyleCircular:NSBezelStyleRounded;
      if(c->kind==1) b.buttonType=NSButtonTypePushOnPushOff;
      if(c->kind==2) { b.title=@""; b.image=[NSImage imageWithSystemSymbolName:@"xmark" accessibilityDescription:[NSString stringWithUTF8String:i18n("Fechar")]]; b.imagePosition=NSImageOnly; }
      b.state=c->selected?NSControlStateValueOn:NSControlStateValueOff;
      b.enabled=c->enabled; b.toolTip=[NSString stringWithUTF8String:c->title];
      b.accessibilityLabel=b.toolTip;
      b.contentTintColor=[NSColor colorWithSRGBRed:243/255. green:242/255. blue:239/255. alpha:1];
      b.bezelColor=c->kind==3?NSColor.systemBlueColor:[NSColor colorWithWhite:c->selected?.30:.23 alpha:1];
      [sourceControls addSubview:b];
    }
    memcpy(previous,controls,sizeof(*controls)*count); previousCount=count; previousW=w; previousH=h;
    if(texture) { glDeleteTextures(1,&texture); texture=0; }
  }
  if(!getenv("NV_CAPTURA_GL")) return 0;
  if(!texture) {
    NSBitmapImageRep *bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:(NSInteger)w pixelsHigh:(NSInteger)h
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
        bitmapFormat:0 bytesPerRow:(NSInteger)w*4 bitsPerPixel:32];
    memset(bitmap.bitmapData,0,bitmap.bytesPerRow*bitmap.pixelsHigh);
    // Covered windows may never display their layer tree. Draw the very same
    // NSButtonCells into the readback texture without displaying/activating it.
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext *context=[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    NSGraphicsContext.currentContext=context;
    CGContextTranslateCTM(context.CGContext,0,h);CGContextScaleCTM(context.CGContext,1,-1);
    [sourceControls.appearance performAsCurrentDrawingAppearance:^{
      for(NSButton *button in sourceControls.subviews) [button.cell drawWithFrame:button.frame inView:sourceControls];
    }];
    [NSGraphicsContext restoreGraphicsState];
    if(getenv("NV_CAPTURA_DIR")) {
      NSString *path=[[NSString stringWithUTF8String:getenv("NV_CAPTURA_DIR")] stringByAppendingPathComponent:@"source-controls.png"];
      [[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES];
    }
    // AppKit bitmap contexts require premultiplied storage; GL UI uses straight alpha.
    unsigned char *pixels=bitmap.bitmapData;
    for(NSInteger i=0;i<bitmap.pixelsWide*bitmap.pixelsHigh;i++) {
      unsigned a=pixels[4*i+3];
      if(a) for(int c=0;c<3;c++) pixels[4*i+c]=MIN(255,(pixels[4*i+c]*255u+a/2)/a);
    }
    glActiveTexture(GL_TEXTURE0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)w,(GLsizei)h,0,GL_RGBA,GL_UNSIGNED_BYTE,bitmap.bitmapData);
    [bitmap release];
    gfx_tex_esquecer(0);
  }
  return texture;
}
static int sidebarLanguage = -1;
static NSMutableArray<NSTextField *> *groupLabels;
static NSTextField *profileChevron;
static float sidebarFraction, sidebarReveal;
static id hoverMonitor;
static id scrollMonitor;
static BOOL menuTracking, nativeScrollLatched;
static NSPoint nativeScrollPoint;
static BOOL captureFullscreen;
static IMP sidebarOriginalHit;
static BOOL firstMouse(id self, SEL cmd, NSEvent *event) { return YES; }
// Preserve SDL's class behavior; make decorative overlay regions transparent to input.
static NSView *sidebarHit(id self, SEL cmd, NSPoint point) {
  NSView *view=self;
  if(view.hidden || view.alphaValue<.001 || !NSPointInRect(point,view.frame)) return nil;
  NSView *hit=((NSView *(*)(id,SEL,NSPoint))sidebarOriginalHit)(self,cmd,point);
  for(NSView *v=hit;v && v!=view;v=v.superview)
    if([v isKindOfClass:NSControl.class] || [v isKindOfClass:NSScrollView.class]) return hit;
  return nil;
}
static void specializeView(NSView *view, const char *name, SEL method, IMP implementation, const char *types) {
  Class cls=objc_getClass(name);
  if(!cls) {
    cls=objc_allocateClassPair(object_getClass(view),name,0);
    class_addMethod(cls,method,implementation,types);objc_registerClassPair(cls);
  }
  object_setClass(view,cls);
}
static NSMutableArray<NSButton *> *rows;
static NSButton *profile;
static NSMutableArray<NSButton *> *services;
static NSScrollView *serviceScroll;
static NSView *serviceContent;
@interface NVMenuTarget : NSObject
@property NSInteger selected;
- (void)choose:(NSMenuItem *)item;
@end
@implementation NVMenuTarget
- (void)choose:(NSMenuItem *)item { self.selected = item.tag; }
- (void)toggleSidebar:(id)sender { menu_desktop_alternar(); }
@end
static NSButton *sidebarToggle;
static NSButton *profileButtons[3];
static BOOL profileButtonUsed[3];
static void (*profileButtonActions[3])(int, int);
@interface NVProfileButton : NSButton
@property int actionTag;
@end
@implementation NVProfileButton
- (void)chooseProfile:(id)sender {
  if (profileButtonActions[self.tag]) profileButtonActions[self.tag](self.actionTag, 0);
}
@end

// Native SF glyphs and NSButton snapshots are also used by GL readback proofs.
// Cache only this screen's small, bounded set of labels and controls.
static GLuint profileBitmap(NSBitmapImageRep *bitmap) {
  int w=(int)bitmap.pixelsWide,h=(int)bitmap.pixelsHigh;
  unsigned char *pixels=malloc((size_t)w*h*4);
  for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
    NSColor *c=[[bitmap colorAtX:x y:y] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    size_t p=((size_t)y*w+x)*4;
    pixels[p]=c.redComponent*255; pixels[p+1]=c.greenComponent*255;
    pixels[p+2]=c.blueComponent*255; pixels[p+3]=c.alphaComponent*255;
  }
  GLuint texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  free(pixels); return texture;
}
void desktop_native_profile_text(const char *text, float cx, float y, float size, int bold, float ink, float alpha) {
  static NSMutableDictionary *cache;
  if(!cache) cache=[NSMutableDictionary new];
  NSString *string=[NSString stringWithUTF8String:text];
  NSString *key=[NSString stringWithFormat:@"%@:%g:%d",string,size,bold];
  NSArray *entry=cache[key];
  if(!entry) {
    if(cache.count>=64) {
      for(NSArray *old in cache.allValues) {GLuint t=[old[0] unsignedIntValue];gfx_tex_esquecer(t);glDeleteTextures(1,&t);}
      [cache removeAllObjects];
    }
    NSDictionary *attrs=@{NSFontAttributeName:[NSFont systemFontOfSize:size weight:bold?NSFontWeightSemibold:NSFontWeightRegular],NSForegroundColorAttributeName:NSColor.whiteColor};
    NSSize s=[string sizeWithAttributes:attrs]; s.width=ceil(s.width);s.height=ceil(s.height);
    NSImage *image=[[[NSImage alloc] initWithSize:s] autorelease];
    [image lockFocusFlipped:YES];[string drawAtPoint:NSZeroPoint withAttributes:attrs];[image unlockFocus];
    NSBitmapImageRep *bitmap=[NSBitmapImageRep imageRepWithData:image.TIFFRepresentation];
    entry=@[@(profileBitmap(bitmap)),@(s.width),@(s.height)]; cache[key]=entry;
  }
  float w=[entry[1] floatValue],h=[entry[2] floatValue];
  gfx_rect((GfxRect){cx-w*.5f,y,w,h},[entry[0] unsignedIntValue],GFX_TEXTO,0,1,0,0,ink,ink,ink,alpha);
}
void desktop_native_profile_button(GfxRect rect, const char *title, int enabled, int selected, float alpha,
                                  void (*activate)(int,int), int tag, int slot) {
  if(!nativeWindow || slot<0 || slot>=3) return;
  NVProfileButton *button=(NVProfileButton *)profileButtons[slot];
  if(!button) {
    button=[NVProfileButton new];profileButtons[slot]=button;
    button.tag=slot;button.target=button;button.action=@selector(chooseProfile:);
    button.bezelStyle=slot==2?NSBezelStyleCircular:NSBezelStyleRounded;
    button.controlSize=NSControlSizeLarge;
    button.appearance=[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    button.font=[NSFont systemFontOfSize:15];
    [nativeWindow.contentView addSubview:button];
  }
  profileButtonUsed[slot]=YES;profileButtonActions[slot]=activate;button.actionTag=tag;
  NSView *view=nativeWindow.contentView;
  float scale=view.bounds.size.width/NV_TELA_W;
  button.frame=NSMakeRect(rect.x*scale,view.bounds.size.height-(rect.y+rect.h)*scale,rect.w*scale,rect.h*scale);
  button.title=[NSString stringWithUTF8String:title];button.enabled=enabled;button.alphaValue=alpha;
  [button highlight:selected];button.hidden=NO;
  if(getenv("NV_CAPTURA_GL")) {
    static GLuint textures[3];static NSString *keys[3];
    NSString *key=[NSString stringWithFormat:@"%@:%d:%d:%g:%g",button.title,enabled,selected,rect.w,rect.h];
    if(![keys[slot] isEqual:key]) {
      [keys[slot] release];keys[slot]=[key copy];
      if(textures[slot]) {gfx_tex_esquecer(textures[slot]);glDeleteTextures(1,&textures[slot]);}
      NSImage *image=[[[NSImage alloc] initWithSize:button.bounds.size] autorelease];
      [button.effectiveAppearance performAsCurrentDrawingAppearance:^{
        [image lockFocusFlipped:YES];
        [button.cell drawWithFrame:button.bounds inView:button];
        [image unlockFocus];
      }];
      NSBitmapImageRep *bitmap=[NSBitmapImageRep imageRepWithData:image.TIFFRepresentation];
      textures[slot]=profileBitmap(bitmap);
    }
    button.hidden=YES;
    gfx_rect(rect,textures[slot],GFX_TEXTO,0,0,0,0,1,1,1,alpha);
  }
}
static NSInteger alertAnswer = -1;
static NSString *alertText;
// Window-modal NSAlert sheet; SDL keeps pumping while it is up.
int desktop_native_alert(const char *title, const char *message, const char *const *buttons, int count,
                         int destructive, const char *text, int secure) {
  if (!nativeWindow || nativeWindow.attachedSheet || count < 1) return -1;
  NSAlert *alert = [NSAlert new];
  alert.messageText = [NSString stringWithUTF8String:title ? title : ""];
  alert.informativeText = [NSString stringWithUTF8String:message ? message : ""];
  for (int i = 0; i < count; i++) {
    NSButton *button = [alert addButtonWithTitle:[NSString stringWithUTF8String:buttons[i]]];
    if (i == 0 && destructive) button.hasDestructiveAction = YES;
    if (i == count - 1 && count > 1) button.keyEquivalent = @"\033";
  }
  NSTextField *field = nil;
  if (text) {
    field = secure ? [NSSecureTextField new] : [NSTextField new];
    field.stringValue = [NSString stringWithUTF8String:text];
    field.frame = NSMakeRect(0, 0, 260, 24);
    alert.accessoryView = field;
    alert.window.initialFirstResponder = field;
  }
  alertAnswer = -2;
  [alert beginSheetModalForWindow:nativeWindow completionHandler:^(NSModalResponse response) {
    [alertText release]; alertText = [field.stringValue copy];
    alertAnswer = response - NSAlertFirstButtonReturn;
    [field release]; [alert release];
  }];
  printf("[desktop-sheet] window=%ld parent=%ld field=%d\n", (long)alert.window.windowNumber,
         (long)nativeWindow.windowNumber, text != NULL); fflush(stdout);
  return 0;
}
int desktop_native_alert_result(char *text, int size) {
  if (alertAnswer < 0) return -1;
  int answer = (int)alertAnswer; alertAnswer = -1;
  if (text && size > 0) snprintf(text, size, "%s", alertText ? alertText.UTF8String : "");
  return answer;
}
int desktop_native_menu(const DesktopMenuItem *items, int count, float x, float y) {
  if (!nativeWindow || count <= 0) return -1;
  NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
  menu.autoenablesItems = NO;
  NVMenuTarget *target = [NVMenuTarget new]; target.selected = -1;
  for (int i=0; i<count; i++) {
    if (items[i].separator && menu.numberOfItems) [menu addItem:NSMenuItem.separatorItem];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:items[i].title] action:@selector(choose:) keyEquivalent:@""];
    item.tag = items[i].action; item.target = target;
    item.enabled = items[i].action != 0;
    item.state = items[i].checked ? NSControlStateValueOn : NSControlStateValueOff;
    if (items[i].symbol) {
      item.image = [NSImage imageWithSystemSymbolName:[NSString stringWithUTF8String:items[i].symbol] accessibilityDescription:item.title];
      if (@available(macOS 27.0, *)) item.preferredImageVisibility = NSMenuItemImageVisibilityVisible;
    }
    [menu addItem:item]; [item release];
  }
  NSView *view = nativeWindow.contentView;
  NSPoint point = NSMakePoint(x, view.isFlipped ? y : view.bounds.size.height-y);
  printf("[desktop-menu] native=NSMenu items=%d window=%ld\n",count,(long)nativeWindow.windowNumber); fflush(stdout);
  // Proof tracking uses only an app-local timer; no activation or global input.
  NSTimer *dismiss = nil;
  if (getenv("NV_CAPTURA_MENU")) {
    dismiss = [NSTimer timerWithTimeInterval:8 repeats:NO block:^(NSTimer *timer) { [menu cancelTracking]; }];
    [[NSRunLoop mainRunLoop] addTimer:dismiss forMode:NSEventTrackingRunLoopMode];
  }
  menuTracking=YES;
  [menu popUpMenuPositioningItem:nil atLocation:point inView:view];
  menuTracking=NO;
  [dismiss invalidate];
  int selected = (int)target.selected;
  [menu release]; [target release];
  return selected;
}
// App look (DESIGN.md "Glass UI — ilha"): --fg ink, accent pill for the page,
// white 12 % for hover/focus. Read every frame, so theme changes apply live.
static NSColor *sidebarInk(CGFloat a) { return [NSColor colorWithSRGBRed:243/255. green:242/255. blue:239/255. alpha:a]; }
static BOOL sidebarUnder(NSView *v) {
  // Capture-only: NV_CAPTURA_HOVER=<row tag> proves hover without moving the user's cursor.
  const char *proof=getenv("NV_CAPTURA") ? getenv("NV_CAPTURA_HOVER") : NULL;
  if(proof) return v.tag==atoi(proof);
  return NSPointInRect([v convertPoint:nativeWindow.mouseLocationOutsideOfEventStream fromView:nil],v.bounds);
}
static void paintRow(NSButton *row, BOOL hovered) {
  BOOL selected=row.tag>=0 && row.tag<MENU_N && row.tag==menu_destino();
  BOOL lit=hovered || (row.tag==-1?MENU_N:row.tag)==menu_desktop_foco();
  float r,g,b,ink=ajustes_acento_tinta(&r,&g,&b);
  NSColor *fg=selected?[NSColor colorWithSRGBRed:ink green:ink blue:ink alpha:1]:sidebarInk(lit?1:.78);
  row.layer.backgroundColor=(selected?[NSColor colorWithSRGBRed:r green:g blue:b alpha:1]:lit?[NSColor colorWithWhite:1 alpha:.12]:NSColor.clearColor).CGColor;
  if(![row.contentTintColor isEqual:fg]) row.contentTintColor=fg;
  NSAttributedString *t=row.attributedTitle;
  if(row.title.length && ![[t attribute:NSForegroundColorAttributeName atIndex:0 effectiveRange:NULL] isEqual:fg])
    row.attributedTitle=[[[NSAttributedString alloc] initWithString:row.title attributes:@{NSForegroundColorAttributeName:fg,NSFontAttributeName:row.font}] autorelease];
}
// The app's own stroke icons (art/icones, white on alpha) as template images.
static NSImage *sidebarIcon(const char *name) {
  char rel[96], path[600]; snprintf(rel,sizeof rel,"icones/%s.png",name); gfx_arte_caminho(rel,path,sizeof path);
  NSImage *image=[[[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:path]] autorelease];
  if(!image) image=[NSImage imageWithSystemSymbolName:@"circle" accessibilityDescription:nil];
  image.template=YES; image.size=NSMakeSize(20,20);
  return image;
}
// InterDisplay ships beside art/ (Resources/fonts, deploy/app/fonts).
static NSFont *sidebarFont(CGFloat size, BOOL bold) {
  static BOOL registered;
  if(!registered) {
    registered=YES; char art[600]; gfx_arte_caminho("",art,sizeof art);
    NSString *fonts=[[[NSString stringWithUTF8String:art] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"fonts"];
    for(NSString *f in @[@"InterDisplay-Medium.ttf",@"InterDisplay-Bold.ttf"])
      CTFontManagerRegisterFontsForURL((CFURLRef)[NSURL fileURLWithPath:[fonts stringByAppendingPathComponent:f]],kCTFontManagerScopeProcess,NULL);
  }
  return [NSFont fontWithName:bold?@"InterDisplay-Bold":@"InterDisplay-Medium" size:size] ?: [NSFont systemFontOfSize:size weight:bold?NSFontWeightBold:NSFontWeightMedium];
}
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
- (void)mouseEntered:(NSEvent *)event { paintRow(self,YES); [[NSCursor pointingHandCursor] set]; }
- (void)mouseExited:(NSEvent *)event { paintRow(self,NO); [[NSCursor arrowCursor] set]; }
- (void)updateTrackingAreas {
  for (NSTrackingArea *area in self.trackingAreas) [self removeTrackingArea:area];
  [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:self.bounds options:NSTrackingMouseEnteredAndExited|NSTrackingActiveAlways|NSTrackingInVisibleRect owner:self userInfo:nil]];
  [super updateTrackingAreas];
}
- (void)activate:(id)sender {
  if(self.tag == MENU_STREAMING) { menu_ir(MENU_STREAMING);menu_desktop_expandir(); return; }
  if(self.tag >= MENU_N) {
    const int *folders; int n=home_streaming_barra(&folders), k=(int)self.tag-MENU_N-1;
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
  // mt_* = the TV sidebar's stroke set; aj_* = the same Lucide style for desktop-only rows.
  const char *icons[]={"mt_busca","mt_inicio","mt_biblioteca","mt_guia","aj_list","aj_ribbon","aj_users","menu_sports","aj_tv-minimal-play","aj_puzzle","mt_ajustes"};
  for(unsigned i=0;i<sizeof destinations/sizeof *destinations;i++) {
    NVSidebarButton *button=[NVSidebarButton new]; button.tag=destinations[i];
    button.title=[NSString stringWithUTF8String:i18n(menu_rotulo(destinations[i]))];
    button.image=sidebarIcon(icons[i]); button.image.accessibilityDescription=button.title;
    button.imagePosition=NSImageLeft; button.imageHugsTitle=NO; button.alignment=NSTextAlignmentLeft;
    button.font=sidebarFont(15,NO);
    button.bordered=NO; button.bezelStyle=NSBezelStyleInline;
    button.cell.lineBreakMode=NSLineBreakByTruncatingTail;
    button.cell.usesSingleLineMode=YES;
    button.target=button; button.action=@selector(activate:);button.wantsLayer=YES;button.layer.cornerRadius=10;
    [sidebarControls addSubview:button];[rows addObject:button];
  }
  profile=[NVSidebarButton new];profile.tag=-1;profile.bordered=NO;profile.alignment=NSTextAlignmentLeft;
  profile.font=sidebarFont(15,YES);profile.target=profile;profile.action=@selector(activate:);
  profile.wantsLayer=YES;profile.layer.cornerRadius=10;
  [sidebarControls addSubview:profile];
  groupLabels=[NSMutableArray new];
  for (NSString *title in @[@"", @"", @""]) {
    NSTextField *label=[NSTextField labelWithString:title];
    label.font=sidebarFont(12,NO);
    label.textColor=[NSColor colorWithSRGBRed:145/255. green:147/255. blue:152/255. alpha:1];
    [sidebarControls addSubview:label];[groupLabels addObject:label];
  }
  profileChevron=[NSTextField labelWithString:@"›"];
  profileChevron.textColor=sidebarInk(.55);
  [sidebarControls addSubview:profileChevron];
}
void desktop_native_init(SDL_Window *window) {
  SDL_SysWMinfo info; SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info)) return;
  nativeWindow = info.info.cocoa.window;
  BOOL capture = getenv("NV_CAPTURA") != NULL;
  BOOL readback = capture && getenv("NV_CAPTURA_GL");
  [NSApp setActivationPolicy:readback ? NSApplicationActivationPolicyProhibited :
                            capture ? NSApplicationActivationPolicyAccessory : NSApplicationActivationPolicyRegular];
  if (capture) {
    nativeWindow.hidesOnDeactivate = NO;
    nativeWindow.ignoresMouseEvents = YES;
    if (readback) [nativeWindow orderOut:nil];
    else [nativeWindow orderBack:nil];
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
  // SDL registers AppleMomentumScrollSupported=NO (Cocoa_RegisterApp), so AppKit
  // never generated trackpad momentum for this app. Restore the system default.
  BOOL sdlMomentum = [NSUserDefaults.standardUserDefaults boolForKey:@"AppleMomentumScrollSupported"];
  [NSUserDefaults.standardUserDefaults registerDefaults:@{@"AppleMomentumScrollSupported": @YES}];
  printf("[desktop-momentum] sdl=%d now=%d\n", sdlMomentum,
         [NSUserDefaults.standardUserDefaults boolForKey:@"AppleMomentumScrollSupported"]);
  NSView *content = nativeWindow.contentView;
  specializeView(content,"NVFirstMouseSDLView",@selector(acceptsFirstMouse:),(IMP)firstMouse,"c@:@");
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
    glass.style = NSGlassEffectViewStyleRegular;
    // The island's cold dark core (DESIGN.md ilha-vidro-miolo), not grey system glass.
    glass.tintColor = [NSColor colorWithSRGBRed:14/255. green:15/255. blue:18/255. alpha:.72];
    glass.contentView = sidebarControls;
    sidebar = glass;
  } else {
    NSVisualEffectView *effect = [NSVisualEffectView new];
    effect.material = NSVisualEffectMaterialSidebar;
    effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    effect.state = NSVisualEffectStateActive;
    sidebar = effect;
    [sidebar addSubview:sidebarControls];
  }
  sidebarOriginalHit=class_getMethodImplementation(object_getClass(sidebar),@selector(hitTest:));
  specializeView(sidebar,"NVSidebarHitView",@selector(hitTest:),(IMP)sidebarHit,
    method_getTypeEncoding(class_getInstanceMethod(NSView.class,@selector(hitTest:))));
  sidebar.hidden=YES;
  sidebar.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];  // never light grey
  sidebar.wantsLayer = YES;
  sidebar.layer.cornerRadius = 16;
  sidebar.layer.masksToBounds = YES;
  // The GL surface has an alpha hole only over this native material.
  [container addSubview:sidebar positioned:NSWindowAbove relativeTo:content];
  GLint opaque = 0;
  [[NSOpenGLContext currentContext] setValues:&opaque forParameter:NSOpenGLContextParameterSurfaceOpacity];
  // SDL normalizes Cocoa precise deltas into wheel ticks. Preserve native
  // pixels, phases and OS momentum in a tagged SDL event instead.
  scrollMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel|NSEventMaskSwipe handler:^NSEvent *(NSEvent *event) {
    BOOL swipe = event.type == NSEventTypeSwipe;
    if(menuTracking || nativeWindow.attachedSheet) return event;
    BOOL momentum=event.momentumPhase!=NSEventPhaseNone;
    // Momentum may carry no window after the finger leaves. Its initial
    // window and point remain authoritative, even over another row/view.
    if(event.window!=nativeWindow && !(event.window==nil && momentum && nativeScrollLatched)) return event;
    if(event.phase==NSEventPhaseNone && !momentum) nativeScrollLatched=NO;
    NSPoint p=momentum && nativeScrollLatched?nativeScrollPoint:[content convertPoint:event.locationInWindow fromView:nil];
    if(!nativeScrollLatched || (event.phase & NSEventPhaseBegan)) {
      NSPoint containerPoint=[sidebar.superview convertPoint:event.locationInWindow fromView:nil];
      if([sidebar hitTest:containerPoint]) return event;
      nativeScrollPoint=p;nativeScrollLatched=YES;
    }
    SDL_Event wheel = {0}; wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.windowID = SDL_GetWindowID(window);
    wheel.wheel.which = NV_DESKTOP_PIXEL_WHEEL;
    float unit=event.hasPreciseScrollingDeltas?1:36;
    wheel.wheel.preciseX = swipe ? event.deltaX * 60 : event.scrollingDeltaX*unit;
    wheel.wheel.preciseY = swipe ? event.deltaY * 60 : event.scrollingDeltaY*unit;
    // Keep both native phases losslessly for session diagnostics/replay.
    wheel.wheel.y=(int)event.phase|((int)event.momentumPhase<<8)|(event.hasPreciseScrollingDeltas?0:1<<16);
    wheel.wheel.x = NV_WHEEL_PHASED;
    if (event.phase & NSEventPhaseBegan) wheel.wheel.x |= NV_WHEEL_BEGIN;
    if ((event.phase | event.momentumPhase) & NSEventPhaseEnded) wheel.wheel.x |= NV_WHEEL_END;
    if ((event.phase | event.momentumPhase) & NSEventPhaseCancelled) wheel.wheel.x |= NV_WHEEL_CANCEL;
    if (event.momentumPhase != NSEventPhaseNone) wheel.wheel.x |= NV_WHEEL_MOMENTUM;
    // Magic Mouse has precise deltas without a finger lifecycle. Keep its
    // pixel route; the consumer ends ownership after a short quiet interval.
    if (event.phase == NSEventPhaseNone && event.momentumPhase == NSEventPhaseNone)
      wheel.wheel.x = NV_WHEEL_UNPHASED;
    if (swipe) wheel.wheel.x = NV_WHEEL_PHASED|NV_WHEEL_BEGIN|NV_WHEEL_END|NV_WHEEL_SWIPE;
    wheel.wheel.timestamp = SDL_GetTicks();
    wheel.wheel.mouseX = p.x; wheel.wheel.mouseY = content.isFlipped?p.y:content.bounds.size.height-p.y;
    SDL_PushEvent(&wheel);
    if((event.phase&NSEventPhaseCancelled) || (event.momentumPhase&(NSEventPhaseEnded|NSEventPhaseCancelled))) nativeScrollLatched=NO;
    return nil;
  }];
  searchWindowID=SDL_GetWindowID(window);
  nativeWindow.acceptsMouseMovedEvents=YES;
  // Titlebar sidebar button, beside the traffic lights like Finder and Notes.
  NSButton *zoom = [nativeWindow standardWindowButton:NSWindowZoomButton];
  NVMenuTarget *toggleTarget = [NVMenuTarget new];
  sidebarToggle = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"sidebar.left" accessibilityDescription:nil]
                                     target:toggleTarget action:@selector(toggleSidebar:)];
  sidebarToggle.bordered = NO;
  sidebarToggle.contentTintColor = [NSColor colorWithWhite:1 alpha:0.78];
  sidebarToggle.toolTip = [NSString stringWithUTF8String:i18n("Mostrar ou ocultar a barra lateral")];
  sidebarToggle.accessibilityLabel = sidebarToggle.toolTip;
  sidebarToggle.frame = NSMakeRect(NSMaxX(zoom.frame) + 14, NSMidY(zoom.frame) - 14, 36, 28);
  sidebarToggle.autoresizingMask = NSViewMinYMargin;
  [zoom.superview addSubview:sidebarToggle];
  // Cocoa and SDL disagree on the content origin after FullSizeContentView.
  // Use the very same view conversion for move, click and scroll; no focus-only click.
  hoverMonitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMouseMoved|NSEventMaskLeftMouseDragged|NSEventMaskLeftMouseDown|NSEventMaskLeftMouseUp|NSEventMaskRightMouseDown|NSEventMaskRightMouseUp handler:^NSEvent *(NSEvent *event) {
    // A sheet blocks its parent: let AppKit refuse the event, never forward it.
    if(menuTracking || event.window!=nativeWindow || nativeWindow.attachedSheet) return event;
    NSPoint p=[content convertPoint:event.locationInWindow fromView:nil];
    for(NSWindowButton button=NSWindowCloseButton;button<=NSWindowZoomButton;button++) {
      NSButton *control=[nativeWindow standardWindowButton:button];
      if(control && NSPointInRect([control convertPoint:event.locationInWindow fromView:nil],control.bounds)) return event;
    }
    if(!sidebarToggle.hidden && NSPointInRect([sidebarToggle convertPoint:event.locationInWindow fromView:nil],sidebarToggle.bounds)) return event;
    if(!searchField.hidden && searchField && NSPointInRect([searchField convertPoint:event.locationInWindow fromView:nil],searchField.bounds)) return event;
    if(event.type==NSEventTypeLeftMouseDown && searchField.currentEditor)
      [nativeWindow makeFirstResponder:content];
    menu_desktop_hover(p.x);
    NSPoint containerPoint=[sidebar.superview convertPoint:event.locationInWindow fromView:nil];
    if(sourceActive) {
      NSPoint q=[sourceControls.superview convertPoint:event.locationInWindow fromView:nil];
      if(!sourceControls.hidden && [sourceControls hitTest:q]) return event;
    } else if([sidebar hitTest:containerPoint]) return event;
    SDL_Event mouse={0};
    BOOL move=event.type==NSEventTypeMouseMoved || event.type==NSEventTypeLeftMouseDragged;
    float y=content.isFlipped?p.y:content.bounds.size.height-p.y;
    if(move) {
      mouse.type=SDL_MOUSEMOTION;mouse.motion.windowID=SDL_GetWindowID(window);
      mouse.motion.x=p.x;mouse.motion.y=y;
    } else {
      BOOL down=event.type==NSEventTypeLeftMouseDown || event.type==NSEventTypeRightMouseDown;
      mouse.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
      mouse.button.windowID=SDL_GetWindowID(window);
      mouse.button.button=event.type==NSEventTypeRightMouseDown || event.type==NSEventTypeRightMouseUp || (event.modifierFlags&NSEventModifierFlagControl)?SDL_BUTTON_RIGHT:SDL_BUTTON_LEFT;
      mouse.button.state=down?SDL_PRESSED:SDL_RELEASED;
      mouse.button.x=p.x;mouse.button.y=y;
    }
    SDL_PushEvent(&mouse);
    return nil;
  }];
  int ww,wh,dw,dh;SDL_GetWindowSize(window,&ww,&wh);SDL_GL_GetDrawableSize(window,&dw,&dh);
  printf("[desktop-coordinates] window=%dx%d view=%.0fx%.0f drawable=%dx%d flipped=%d\n",ww,wh,content.bounds.size.width,content.bounds.size.height,dw,dh,content.isFlipped);
  printf("[desktop-native] window=%ld material=Sidebar transparent=1 glass=%d radius=%.1f\n", (long)nativeWindow.windowNumber, [sidebar isKindOfClass:NSClassFromString(@"NSGlassEffectView")], radius);
}
void desktop_native_sidebar(float fraction, float reveal) {
  if (getenv("NV_CAPTURA_GL")) fraction=0;
  sidebarFraction=fraction; sidebarReveal=reveal;
}
int desktop_native_search(const char *query,float x,float y,float w,float h) {
  if(!nativeWindow) return 0;
  searchDrawn=YES;
  if(getenv("NV_CAPTURA_GL")) return 0;
  BOOL appearing=!searchField || searchField.hidden;
  if(!searchField) {
    searchField=[[NSSearchField alloc] init];
    searchField.delegate=[NVSearchTarget new];
    searchField.sendsSearchStringImmediately=YES;
    searchField.font=[NSFont systemFontOfSize:14];
    searchField.placeholderString=[NSString stringWithUTF8String:i18n("Buscar")];
    searchField.accessibilityLabel=searchField.placeholderString;
    [nativeWindow.contentView addSubview:searchField];
    printf("[desktop-search] control=NSSearchField\n"); fflush(stdout);
  }
  NSView *content=nativeWindow.contentView;
  NSRect bounds=content.bounds;
  searchField.frame=NSMakeRect(x*bounds.size.width,content.isFlipped?y*bounds.size.height:(1-y-h)*bounds.size.height,w*bounds.size.width,h*bounds.size.height);
  NSString *value=[NSString stringWithUTF8String:query];
  if(![searchField.stringValue isEqualToString:value]) searchField.stringValue=value;
  searchField.hidden=NO;
  if(appearing && nativeWindow.isKeyWindow && !getenv("NV_CAPTURA"))
    [nativeWindow makeFirstResponder:searchField];
  return 1;
}
void desktop_native_finish(void) {
  for(int i=0;i<3;i++) {
    if(!profileButtonUsed[i]) profileButtons[i].hidden=YES;
    profileButtonUsed[i]=NO;
  }
  if(!searchDrawn && !searchField.hidden) {
    if(searchField.currentEditor) [nativeWindow makeFirstResponder:nativeWindow.contentView];
    searchField.hidden=YES;
  }
  searchDrawn=NO;
  float fraction=sourceActive?0:sidebarFraction;
  if (!sidebar) return;
  if(fraction<=0 || sidebarReveal<.001f) {sidebar.hidden=YES;return;}
  [CATransaction begin]; [CATransaction setDisableActions:YES];
  NSRect bounds=nativeWindow.contentView.bounds;
  BOOL fullscreen=desktop_native_fullscreen();
  nativeWindow.contentView.layer.cornerRadius=fullscreen?0:16;
  nativeWindow.contentView.superview.layer.cornerRadius=fullscreen?0:16;
  CGFloat titleInset=fullscreen?8:36;
  NSRect frame=NSMakeRect(8-(1-sidebarReveal)*bounds.size.width*fraction,8,MAX(1,bounds.size.width*fraction-16),bounds.size.height-titleInset-8);
  sidebar.alphaValue=sidebarReveal;
  if(!NSEqualRects(sidebar.frame,frame)) sidebar.frame = frame;
  if(sidebar.hidden) sidebar.hidden=NO;
  sidebarControls.frame=sidebar.bounds;
  sidebarRows();
  if(sidebarLanguage!=ajustes_idioma()) {
    sidebarLanguage=ajustes_idioma();
    for(NSButton *row in rows) {
      row.title=[NSString stringWithUTF8String:i18n(menu_rotulo((int)row.tag))];
      row.image.accessibilityDescription=row.title;
    }
    const char *keys[]={"Navegar","Você","Serviços"};
    for(NSUInteger i=0;i<groupLabels.count;i++)groupLabels[i].stringValue=[NSString stringWithUTF8String:i18n(keys[i])];
  }
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
    paintRow(row,sidebarUnder(row));
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
    // No photo: the profile's own colour, like the app (menu.c corAvatar).
    unsigned hex=0x1E88E5; if(active && active->corHex[0]=='#') sscanf(active->corHex+1,"%6x",&hex);
    NSColor *avatarTint=[NSColor colorWithSRGBRed:(hex>>16&255)/255. green:(hex>>8&255)/255. blue:(hex&255)/255. alpha:1];
    NSMutableString *initials=[NSMutableString new];
    for(NSString *word in [profileTitle componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceCharacterSet]) {
      if(word.length) [initials appendString:[[word substringWithRange:[word rangeOfComposedCharacterSequenceAtIndex:0]] uppercaseString]];
      if(initials.length>=2) break;
    }
    profile.image=[NSImage imageWithSize:NSMakeSize(28,28) flipped:NO drawingHandler:^BOOL(NSRect rect) {
      [[NSBezierPath bezierPathWithOvalInRect:rect] addClip];
      if(avatar) [avatar drawInRect:rect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
      else {
        [avatarTint setFill]; NSRectFill(rect);
        NSDictionary *attributes=@{NSFontAttributeName:[NSFont systemFontOfSize:11 weight:NSFontWeightSemibold],NSForegroundColorAttributeName:NSColor.whiteColor};
        NSSize size=[initials sizeWithAttributes:attributes];
        [initials drawAtPoint:NSMakePoint((28-size.width)*.5,(28-size.height)*.5) withAttributes:attributes];
      }
      return YES;
    }];
  }
  NSRect profileFrame=NSMakeRect(10,14,w-20,36);if(!NSEqualRects(profile.frame,profileFrame))profile.frame=profileFrame;
  paintRow(profile,sidebarUnder(profile));
  if(profile.imagePosition!=(collapsed?NSImageOnly:NSImageLeft))profile.imagePosition=collapsed?NSImageOnly:NSImageLeft;
  const int *folders;int count=menu_desktop_streaming()?home_streaming_barra(&folders):0;
  while(services.count<(NSUInteger)count){
    NVSidebarButton *button=[NVSidebarButton new];button.bordered=NO;button.alignment=NSTextAlignmentLeft;
    button.font=sidebarFont(14,NO);button.target=button;button.action=@selector(activate:);
    button.tag=MENU_N+1+services.count;button.wantsLayer=YES;button.layer.cornerRadius=8;[serviceContent addSubview:button];[services addObject:button];
  }
  CGFloat serviceTop=h-12-9*35-84+(sportsEnabled?0:35);
  CGFloat serviceHeight=MIN(count*29,MAX(0,serviceTop-130));
  serviceScroll.frame=NSMakeRect(28,serviceTop-serviceHeight,w-38,serviceHeight);serviceScroll.hidden=!count||collapsed;
  serviceContent.frame=NSMakeRect(0,0,w-38,count*29);
  for(NSButton *button in services){
    int k=(int)[services indexOfObjectIdenticalTo:button];button.hidden=k>=count||collapsed;
    if(k<count){const ColFolder *folder=col_folder(folders[k]);NSString *t=folder?[NSString stringWithUTF8String:folder->title]:@"";if(![button.title isEqualToString:t])button.title=t;}
    button.frame=NSMakeRect(4,count*29-(k+1)*29,w-48,28);
    paintRow(button,sidebarUnder(button));
  }
  for(NSButton *row in rows) if(row.tag==MENU_FONTES){NSRect r=row.frame;r.origin.y-=serviceHeight;row.frame=r;}
  [CATransaction commit];
}
void desktop_native_appearance(const char *name) {
  if (getenv("NV_CAPTURA")) {
    NSAppearance *appearance=[NSAppearance appearanceNamed:!strcmp(name,"Aqua")?NSAppearanceNameAqua:NSAppearanceNameDarkAqua];
    NSApp.appearance=appearance;
    nativeWindow.appearance=appearance;
    nativeWindow.contentView.appearance=appearance;
    [nativeWindow.contentView setNeedsDisplay:YES];
    [sidebar setNeedsDisplay:YES];
    [sidebarControls setNeedsDisplay:YES];
    [nativeWindow displayIfNeeded];
  }
}
int desktop_native_fullscreen(void) {
  return captureFullscreen || (nativeWindow.styleMask&NSWindowStyleMaskFullScreen)!=0;
}
void desktop_native_capture_fullscreen(int width, int height) {
  if(!getenv("NV_CAPTURA") || !nativeWindow) return;
  captureFullscreen=YES;
  // Borderless full-size window behind the foreground app; no Space switch or activation.
  nativeWindow.styleMask=NSWindowStyleMaskBorderless;
  NSRect screen=nativeWindow.screen.frame;
  [nativeWindow setFrame:NSMakeRect(screen.origin.x,screen.origin.y,width,height) display:YES];
}
int desktop_native_dark(void) {
  return [[nativeWindow.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameDarkAqua, NSAppearanceNameAqua]] isEqualToString:NSAppearanceNameDarkAqua];
}
int desktop_native_swipes(void) { return NSEvent.isSwipeTrackingFromScrollEventsEnabled; }
#endif
