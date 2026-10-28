// iOS host for the Nuvio C core (milestone 1 of docs/reviews/ios/PLANO.md).
//
// main() here is the real process entry: it prepares the sandbox folders and
// the environment the core expects (NUVIO_DADOS, NUVIO_LOG, HOME), points
// argv[1] at the art folder inside the bundle and hands control to SDL's UIKit
// runner, which calls the core's main() (renamed SDL_main by SDL_main.h).
// Video is not wired yet: video.c compiles its Apple stub, as on the Mac.
#define SDL_MAIN_HANDLED
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ios.h"

extern int SDL_main(int argc, char *argv[]);
unsigned nv_gl_fb_tela;

int ios_interface_mobile_auto(void) {
  const char *modo = getenv("NUVIO_INTERFACE_AUTO");
  return modo && !strcmp(modo, "1");
}

// iOS 27 requires the scene life cycle. SDL 2.30 drives the app through the
// old UIApplicationDelegate and app-level notifications, which still fire
// under scenes; this delegate only makes sure SDL's window belongs to the
// scene (the patched SDL_uikitwindow.m does it at creation; this covers a
// window created before the scene connected).
@interface NuvioSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (nonatomic, strong) UIWindow *window;
@end
@implementation NuvioSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
  if (![scene isKindOfClass:UIWindowScene.class]) return;
  for (UIWindow *w in UIApplication.sharedApplication.windows)
    if (!w.windowScene) w.windowScene = (UIWindowScene *)scene;
  printf("[ios] scene connected %.0fx%.0f\n", ((UIWindowScene *)scene).coordinateSpace.bounds.size.width,
         ((UIWindowScene *)scene).coordinateSpace.bounds.size.height);
  fflush(stdout);
}
@end

static NSString *pasta(NSSearchPathDirectory d, NSString *sub) {
  NSString *base = [NSSearchPathForDirectoriesInDomains(d, NSUserDomainMask, YES) firstObject];
  NSString *p = sub ? [base stringByAppendingPathComponent:sub] : base;
  [[NSFileManager defaultManager] createDirectoryAtPath:p withIntermediateDirectories:YES attributes:nil error:nil];
  return p;
}

int ios_insets(float *left, float *top, float *right, float *bottom) {
  __block UIEdgeInsets ins = UIEdgeInsetsZero;
  __block int ok = 0;
  void (^ler)(void) = ^{
    for (UIScene *s in UIApplication.sharedApplication.connectedScenes) {
      if (![s isKindOfClass:UIWindowScene.class]) continue;
      for (UIWindow *w in ((UIWindowScene *)s).windows) {
        if (!w.isKeyWindow) continue;
        ins = w.safeAreaInsets; ok = 1; return;
      }
    }
  };
  if (NSThread.isMainThread) ler(); else dispatch_sync(dispatch_get_main_queue(), ler);
  if (!ok) return 0;
  if (left) *left = (float)ins.left;
  if (top) *top = (float)ins.top;
  if (right) *right = (float)ins.right;
  if (bottom) *bottom = (float)ins.bottom;
  return 1;
}

char *nv_host_http(const char *verbo, const char *url, const char *cabs,
                   const char *corpo, int prazoMs, int *status, int *retryAfter, long *n,
                   char *erro, size_t nErro) {
  if (status) *status = 0;
  if (retryAfter) *retryAfter = 0;
  if (n) *n = 0;
  if (erro && nErro) erro[0] = 0;
  NSURL *u = [NSURL URLWithString:[NSString stringWithUTF8String:url ? url : ""]];
  if (!u) { if (erro) snprintf(erro, nErro, "url invalida"); return NULL; }
  NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:u];
  req.HTTPMethod = [NSString stringWithUTF8String:verbo && *verbo ? verbo : "GET"];
  req.timeoutInterval = (prazoMs > 0 ? prazoMs : 20000) / 1000.0;
  req.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
  if (cabs) {
    for (NSString *linha in [[NSString stringWithUTF8String:cabs] componentsSeparatedByString:@"\n"]) {
      NSRange dp = [linha rangeOfString:@":"];
      if (dp.location == NSNotFound) continue;
      NSString *k = [[linha substringToIndex:dp.location] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
      NSString *v = [[linha substringFromIndex:dp.location + 1] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
      if (k.length) [req setValue:v forHTTPHeaderField:k];
    }
  }
  if (corpo) req.HTTPBody = [NSData dataWithBytes:corpo length:strlen(corpo)];

  static NSURLSession *sessao;
  static dispatch_once_t uma;
  dispatch_once(&uma, ^{
    NSURLSessionConfiguration *c = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    c.HTTPMaximumConnectionsPerHost = 6;
    c.waitsForConnectivity = NO;
    sessao = [NSURLSession sessionWithConfiguration:c];
  });

  __block NSData *dados = nil;
  __block NSHTTPURLResponse *resp = nil;
  __block NSError *falha = nil;
  dispatch_semaphore_t fim = dispatch_semaphore_create(0);
  [[sessao dataTaskWithRequest:req completionHandler:^(NSData *d, NSURLResponse *r, NSError *e) {
    dados = d; falha = e;
    if ([r isKindOfClass:NSHTTPURLResponse.class]) resp = (NSHTTPURLResponse *)r;
    dispatch_semaphore_signal(fim);
  }] resume];
  dispatch_semaphore_wait(fim, DISPATCH_TIME_FOREVER);

  if (falha || !resp) {
    if (erro) snprintf(erro, nErro, "%s", falha ? falha.localizedDescription.UTF8String : "sem resposta");
    return NULL;
  }
  if (status) *status = (int)resp.statusCode;
  NSString *ra = resp.allHeaderFields[@"Retry-After"];
  if (retryAfter && ra) *retryAfter = ra.intValue;
  size_t tam = dados ? dados.length : 0;
  char *r = (char *)malloc(tam + 1);
  if (!r) { if (erro) snprintf(erro, nErro, "sem memoria"); return NULL; }
  if (tam) memcpy(r, dados.bytes, tam);
  r[tam] = 0;
  if (n) *n = (long)tam;
  return r;
}

int main(int argc, char *argv[]) {
  @autoreleasepool {
    NSString *dados = pasta(NSApplicationSupportDirectory, @"Nuvio");
    NSString *logs = pasta(NSApplicationSupportDirectory, @"Nuvio/log");
    NSString *cache = pasta(NSCachesDirectory, @"Nuvio");
    setenv("HOME", NSHomeDirectory().UTF8String, 1);
    setenv("NUVIO_DADOS", dados.UTF8String, 1);
    setenv("NUVIO_CACHE", cache.UTF8String, 1);
    setenv("NUVIO_LOG", [logs stringByAppendingPathComponent:@"nuvio.log"].UTF8String, 1);
    setenv("NUVIO_LOG_ANTERIOR", [logs stringByAppendingPathComponent:@"nuvio-anterior.log"].UTF8String, 1);
    setenv("NUVIO_INTERFACE_AUTO", "1", 1);   // Settings > Interface mode > Auto picks Mobile here
    setenv("TMPDIR", NSTemporaryDirectory().UTF8String, 1);

    // Every orientation on both displays; the inner display ignores this list
    // for layout anyway and the core already handles SIZE_CHANGED (NV_TOUCH_UI).
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

    NSString *arte = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"art"];
    static char *args[3];
    args[0] = argv[0];
    args[1] = strdup(arte.UTF8String);
    args[2] = NULL;
    (void)argc;
    return SDL_UIKitRunApp(2, args, SDL_main);
  }
}
