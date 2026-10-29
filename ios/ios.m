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
#import <mach/mach.h>
#include "menu.h"
#include "detail.h"
#include "idioma.h"
#include "layout.h"

extern int SDL_main(int argc, char *argv[]);
unsigned nv_gl_fb_tela;

int ios_interface_mobile_auto(void) {
  const char *modo = getenv("NUVIO_INTERFACE_AUTO");
  return modo && !strcmp(modo, "1");
}

static void ios_religar(void);
static UIWindow *ios_janela(void);

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
  // UIApplication.windows lists only windows already in a scene, so SDL's
  // window, made before the scene connected, is attached by reference.
  UIWindow *w = ios_janela();
  if (w && !w.windowScene) {
    w.windowScene = (UIWindowScene *)scene;
    [w makeKeyAndVisible];
  }
  ios_religar();   // a GL buffer tied before the scene existed is black
  printf("[ios] scene connected %.0fx%.0f\n", ((UIWindowScene *)scene).coordinateSpace.bounds.size.width,
         ((UIWindowScene *)scene).coordinateSpace.bounds.size.height);
  fflush(stdout);
}
@end


// ---------------------------------------------------------------- the bar
// iOS 27.1 lays bars out vertically only for container-provided bars, so SDL's
// view controller lives inside a UITabBarController -> UINavigationController
// -> NuvioPainel chain (docs/reviews/ios/PLANO.md, milestone 1). The owner's
// pick for the side region is 1 + 3 + 6: the system bar, morphing per screen,
// with a now-playing dock (that one waits for video, milestone 3).
//
// COLLAPSED BY DEFAULT. While content scrolls and on every navigation the tab
// capsule folds to the active tab; only a tap on it expands the other four.
// The system's own minimize (tabBarMinimizeBehavior) does not apply to the
// vertical bar, so the fold is compactTabIdentifiers = { active tab }.
// The art runs under the bar; layout.c gets the safe-area insets so chrome
// and text keep out.
static UIViewController *sdlVC;
static UITabBarController *abas;
static int destinoPedido = -1, acaoPedida = -1, abasAbertas;
static char telaAnt[32];
static const int TAB_DESTINO[] = { MENU_INICIO, MENU_BUSCAR, MENU_GUIA, MENU_BIBLIOTECA, MENU_PERFIL };
static NSString *const TAB_ICONE[] = { @"house", @"magnifyingglass", @"tv", @"books.vertical", @"person.crop.circle" };
#define N_ABAS (int)(sizeof TAB_DESTINO / sizeof *TAB_DESTINO)

int ios_destino_pedido(void) {
  int d = destinoPedido;
  destinoPedido = -1;
  if (d >= 0) printf("[ios] aba pediu destino %d\n", d);
  return d;
}
int ios_acao_pedida(void) { int a = acaoPedida; acaoPedida = -1; return a; }

static void teclaVoltar(void) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.state = SDL_PRESSED;
  e.key.keysym.sym = SDLK_AC_BACK; e.key.keysym.scancode = SDL_SCANCODE_AC_BACK;
  SDL_PushEvent(&e);
  e.type = SDL_KEYUP; e.key.state = SDL_RELEASED;
  SDL_PushEvent(&e);
}

static void abasDobrar(int abrir) {
  if (!abas || abasAbertas == abrir || !abas.selectedTab) return;
  abasAbertas = abrir;
  NSArray<NSString *> *ids = abrir ? nil : @[ abas.selectedTab.identifier ];
  // iOS 27 animates the change as one update; iOS 26 (deployment target)
  // just sets it. Vertical bars only exist from 27.1, so 26 loses nothing.
  if (@available(iOS 27.0, *)) [abas performBatchUpdates:^{ abas.compactTabIdentifiers = ids; }];
  else abas.compactTabIdentifiers = ids;
}

// The screens the tabs lead to (and the sidebar over them): tabs, no Back.
static int ehRaiz(const char *tela) {
  static const char *const RAIZ[] = { "home", "busca", "guia", "biblioteca", "perfil", "explorar", "ajustes", "agenda", "esportes", "menu" };
  for (size_t i = 0; i < sizeof RAIZ / sizeof *RAIZ; i++)
    if (!strcmp(tela, RAIZ[i])) return 1;
  return 0;
}

@interface NuvioPainel : UIViewController <UIGestureRecognizerDelegate>
- (void)religar;
@end

static NuvioPainel *painelAtual(void) {
  UINavigationController *n = (UINavigationController *)abas.selectedViewController;
  return [n.topViewController isKindOfClass:NuvioPainel.class] ? (NuvioPainel *)n.topViewController : nil;
}

@implementation NuvioPainel
- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.blackColor;
  // Scrolling folds the capsule. The core scrolls in GL, so the host only
  // watches the finger; the touches still reach SDL.
  UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(arrastou:)];
  pan.cancelsTouchesInView = NO;
  pan.delaysTouchesBegan = NO;
  pan.delaysTouchesEnded = NO;
  pan.delegate = self;
  [self.view addGestureRecognizer:pan];
  // The scene may connect after SDL made its window: the real scale arrives
  // as a trait change.
  [self registerForTraitChanges:@[ UITraitDisplayScale.class ] withAction:@selector(religar)];
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)g shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)o { return YES; }
- (void)arrastou:(UIPanGestureRecognizer *)g {
  if (g.state == UIGestureRecognizerStateChanged) abasDobrar(0);
}
- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self adotar];
}
- (void)adotar {
  if (!sdlVC || sdlVC.parentViewController == self) return;
  [sdlVC willMoveToParentViewController:nil];
  [sdlVC.view removeFromSuperview];
  [sdlVC removeFromParentViewController];
  [self addChildViewController:sdlVC];
  sdlVC.view.frame = self.view.bounds;
  sdlVC.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:sdlVC.view];
  [sdlVC didMoveToParentViewController:self];
  [self religar];
}
// SDL ties the GL color buffer to the layer only when the size changes; a new
// parent keeps the size, and a buffer tied while the layer was off-window
// stays black. Rebound by hand once the view is on screen.
- (void)religar {
  UIView *v = sdlVC.view;
  if (v.superview != self.view || !v.window.windowScene || ![v respondsToSelector:@selector(updateFrame)]) return;
  // The core asks for SDL_WINDOW_ALLOW_HIGHDPI; SDL read the scale before the
  // window joined the scene (1x). Native pixels, from the trait.
  CGFloat escala = self.traitCollection.displayScale;
  if (escala > 0 && v.contentScaleFactor != escala) v.contentScaleFactor = escala;
  [v performSelector:@selector(updateFrame)];
}
- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self religar];
}
- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self religar];
  [self insets];
}
- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self insets];
}
- (void)insets {
  if (sdlVC.parentViewController != self) return;
  CGSize t = self.view.bounds.size;
  UIEdgeInsets i = self.view.safeAreaInsets;
  if (t.width <= 0 || t.height <= 0) return;
  static UIEdgeInsets ant;
  if (UIEdgeInsetsEqualToEdgeInsets(i, ant)) return;
  ant = i;
  printf("[ios] insets e=%.0f t=%.0f d=%.0f b=%.0f pt de %.0fx%.0f\n", i.left, i.top, i.right, i.bottom, t.width, t.height);
  layout_insets_definir(i.left / t.width, i.top / t.height, i.right / t.width, i.bottom / t.height);
}
- (void)voltar { teclaVoltar(); abasDobrar(0); }
- (void)acao:(UIBarButtonItem *)b { acaoPedida = (int)b.tag; abasDobrar(0); }
- (UIBarButtonItem *)item:(NSString *)titulo icone:(NSString *)icone acao:(int)acao {
  UIBarButtonItem *b = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:icone] style:UIBarButtonItemStylePlain
                                                       target:self action:@selector(acao:)];
  b.title = titulo;   // the title backs the overflow menu and accessibility
  b.tag = acao;
  return b;
}
// Option 3 of the side-region mockup: the bar's items follow the screen.
- (void)morfar:(const char *)tela {
  int raiz = ehRaiz(tela);
  int imersiva = !strcmp(tela, "login") || !strcmp(tela, "escolha-perfil") || !strcmp(tela, "player") || !strcmp(tela, "?");
  UINavigationItem *n = self.navigationItem;
  if (!strcmp(tela, "detalhe")) {
    n.trailingItemGroups = @[ [UIBarButtonItemGroup fixedGroupWithRepresentativeItem:nil items:@[
      [self item:@(i18n("Fontes")) icone:@"play.rectangle.on.rectangle" acao:DETAIL_BARRA_FONTES],
      [self item:@(i18n("Trailer")) icone:@"film" acao:DETAIL_BARRA_TRAILER],
      [self item:@(i18n("Assistido")) icone:@"eye" acao:DETAIL_BARRA_VISTO],
      [self item:@(i18n("Biblioteca")) icone:@"plus" acao:DETAIL_BARRA_LISTA] ]] ];
  } else if (!strcmp(tela, "biblioteca")) {
    // The chips leave the canvas on the phone; the core opens its sheets (ids 10/11).
    n.trailingItemGroups = @[ [UIBarButtonItemGroup fixedGroupWithRepresentativeItem:nil items:@[
      [self item:@(i18n("Ordenar")) icone:@"arrow.up.arrow.down" acao:10],
      [self item:@(i18n("Filtrar")) icone:@"line.3.horizontal.decrease" acao:11] ]] ];
  } else {
    n.trailingItemGroups = @[];
  }
  if (!raiz && !imersiva) {
    UIBarButtonItem *v = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"chevron.backward"]
                                                          style:UIBarButtonItemStylePlain target:self action:@selector(voltar)];
    v.title = @(i18n("Voltar"));
    n.leftBarButtonItems = @[ v ];
  } else {
    n.leftBarButtonItems = @[];
  }
  [self.navigationController setNavigationBarHidden:((raiz && strcmp(tela, "biblioteca")) || imersiva) animated:YES];
  [abas setTabBarHidden:imersiva animated:YES];
  abasDobrar(0);   // navigation always folds the capsule to the active tab
}
@end

@interface NuvioAbas : UITabBarController <UITabBarControllerDelegate>
@end
@implementation NuvioAbas
// A tap on the folded capsule (the active tab) is the explicit interaction
// that opens it; a tap on another tab goes there and folds it again.
- (BOOL)tabBarController:(UITabBarController *)t shouldSelectTab:(UITab *)tab {
  if (tab == t.selectedTab) {
    if (!strcmp(telaAnt, "detalhe")) {
      NSUInteger i = [t.tabs indexOfObject:tab];
      if (i < (NSUInteger)N_ABAS) destinoPedido = TAB_DESTINO[i];
      abasDobrar(0);
    } else abasDobrar(!abasAbertas);
    return NO;
  }
  return YES;
}
- (void)tabBarController:(UITabBarController *)t didSelectTab:(UITab *)tab previousTab:(UITab *)ant {
  NSUInteger i = [t.tabs indexOfObject:tab];
  if (i < (NSUInteger)N_ABAS) destinoPedido = TAB_DESTINO[i];
  [painelAtual() adotar];
  abasAbertas = 1;
  abasDobrar(0);
}
@end

static void ios_religar(void) { [painelAtual() religar]; }
static UIWindow *janelaSDL;
static UIWindow *ios_janela(void) { return janelaSDL; }

void ios_hospedar(void *janela) {
  UIWindow *w = (__bridge UIWindow *)janela;
  if (!w) return;
  janelaSDL = w;
  if (!abas) {
    NSMutableArray *tabs = [NSMutableArray array];
    for (int i = 0; i < N_ABAS; i++) {
      [tabs addObject:[[UITab alloc] initWithTitle:@(i18n(menu_rotulo(TAB_DESTINO[i])))
                                             image:[UIImage systemImageNamed:TAB_ICONE[i]]
                                        identifier:[NSString stringWithFormat:@"nv-%d", TAB_DESTINO[i]]
                            viewControllerProvider:^UIViewController *(UITab *t) {
        UINavigationController *n = [[UINavigationController alloc] initWithRootViewController:[NuvioPainel new]];
        n.navigationBarHidden = YES;
        return n;
      }]];
    }
    abas = [[NuvioAbas alloc] initWithTabs:tabs];
    abas.delegate = (NuvioAbas *)abas;
    abas.view.backgroundColor = UIColor.blackColor;
    abasAbertas = 1;
  }
  // SDL re-roots its own controller whenever it attaches a new GL view.
  if (w.rootViewController != abas) {
    sdlVC = w.rootViewController;
    w.rootViewController = abas;
    [painelAtual() adotar];
    abasDobrar(0);
    printf("[ios] SDL dentro da barra do sistema (%d abas)\n", N_ABAS);
  }
}

void ios_quadro(const char *tela, int detalhe, int destino) {
  if (!abas || !tela) return;
  // A tab tap waiting for the core is newer than the destination it reports.
  if (destinoPedido < 0)
  for (int i = 0; i < N_ABAS; i++)
    if (TAB_DESTINO[i] == destino && abas.selectedTab != abas.tabs[i]) {
      abasDobrar(1);   // a filtered-out tab must be back to be selected
      abas.selectedTab = abas.tabs[i];   // programmatic: no delegate, no request back
      [painelAtual() adotar];
      abasDobrar(0);
    }
  if (detalhe && ehRaiz(tela)) tela = "detalhe";   // a title opened from Search, Library...
  if (!strncmp(tela, telaAnt, sizeof telaAnt)) return;
  snprintf(telaAnt, sizeof telaAnt, "%s", tela);
  [painelAtual() morfar:tela];
}

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

#include <limits.h>
#include <errno.h>
#include "http.inc"

long ios_ram_mb(void) {
  return (long)(NSProcessInfo.processInfo.physicalMemory / (1024ull * 1024ull));
}

// phys_footprint is what jetsam judges, not resident_size.
double ios_rss_mb(void) {
  task_vm_info_data_t v;
  mach_msg_type_number_t n = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&v, &n) != KERN_SUCCESS) return 0.0;
  return (double)v.phys_footprint / 1048576.0;
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
