// #495: complete production Home and Add-ons events, with real spatial focus.
// Account/catalogue/rendering services are typed in-memory boundaries; no SDL,
// window, network or copied navigation policy is used.
#include "catalogo.h"
#include "colecoes.h"
#include "focus.h"
#include "trailercinema.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t SDL_Keycode;
typedef uint32_t Uint32;
enum { SDL_KEYDOWN=1, SDL_KEYUP, SDL_QUIT };
enum { SDLK_RETURN=10, SDLK_KP_ENTER, SDLK_SPACE, SDLK_UP, SDLK_DOWN,
       SDLK_LEFT, SDLK_RIGHT, SDLK_ESCAPE, SDLK_AC_BACK, SDLK_BACKSPACE,
       SDLK_q };
typedef struct { int type; struct { int repeat; struct { SDL_Keycode sym; } keysym; } key; } SDL_Event;
#define MAX_FIL 64
#define MAX_CARDS 33
#define NV_BORDA_AMP 20.0f
#define HOME_SAIR_MS 3000
#include "home_relogio_tipos.inc"

static Foco foco;
static Fileira fileiras[MAX_FIL];
static int focoHero, nFileiras, sair, pedidoRelogio, pedidoAbrir, pedidoTocar;
static int pedidoMenu, pedidoSocial, pedidoGuia, heroAutoDesligado;
static char guiaId[80];
static int okPressionando, okLongDisparado, okConsumirSoltura;
static Uint32 okDesde, heroUltTecla, sairPerguntadoEm;
static float okHold;
static AnimBorda bordaPag, bordaFil[MAX_FIL];
static TrailerCinema heroCinema;
static int heroLigado, catalogoN, heroPos, menuN;
static int menuTopo;
static int ajustes_menu_topo(void) { return menuTopo; }
static int homevazio_ativo(int rows) { return rows == 0; }
static int homevazio_evento(const SDL_Event *e) { (void)e; return 0; }
static float homeFileiraX(int r, int c) { (void)r; return c * 100.0f; }
static float larguraFil(int r) { (void)r; return 80.0f; }
static Uint32 agora=1000;
static Uint32 SDL_GetTicks(void) { return agora; }
static int ponteiro_ok_longo(void) { return 0; }
int ajustes_hero_ligado(void) { return heroLigado; }
int cat_n(void) { return catalogoN; }
int ajustes_cw_ok_toca(void) { return 0; }
static int heroPosDe(int i) { return i; }
static int heroIntencao(void) { return heroPos; }
static void heroPasso(int d) { heroPos += d; }
static void abrirMenuCartaz(void) { menuN++; }
static int amigosfil_indice_cat(int c) { (void)c; abort(); }
static int amigosfil_tecla(SDL_Keycode k, int *c) { (void)k; (void)c; abort(); }
static void amigosfil_ok(int c) { (void)c; abort(); }
static int amigosfil_pediu_ajustes(void) { abort(); }
static const CatItem *cat_item_exato(int i) { (void)i; abort(); }
const ColFolder *col_folder(int i) { (void)i; abort(); }
static void vertudo_colecao(const ColFolder *f) { (void)f; abort(); }
static void vertudo_abrir(const char *b,const char *t,const char *id,const char *tit) {
  (void)b;(void)t;(void)id;(void)tit;abort();
}
#ifdef NV_TOUCH_UI
static void toqueHeroCancelar(void) { }
static void toqueHomeRetomarFoco(void) { }
#endif
#include "home_relogio_evento.inc"

static int addonFoco, addonSair, addonPedido, addonMexeu, addonN;
static int addonAlternados;
static int addons_n(void) { return addonN; }
static int addons_alternar(int i) { (void)i; addonAlternados++; return 1; }
static void sincronizar(void) { abort(); }
#define foco addonFoco
#define sair addonSair
#define pediuRelogio addonPedido
#define mexeu addonMexeu
#include "home_relogio_addons.inc"
#undef foco
#undef sair
#undef pediuRelogio
#undef mexeu

static int checks, failures;
static void check(int ok,const char *what) {
  checks++; if(!ok) failures++;
  printf("%s: %s\n",ok ? "ok" : "FAIL",what);
}
static void reset(int hero,int rows) {
  memset(&foco,0,sizeof foco); memset(fileiras,0,sizeof fileiras);
  memset(&heroCinema,0,sizeof heroCinema); memset(&bordaPag,0,sizeof bordaPag);
  int cols[2]={4,4}; focus_iniciar(&foco,rows,cols);
  nFileiras=rows; focoHero=0; heroLigado=hero; catalogoN=rows*4;
  for(int r=0;r<rows;r++) { fileiras[r].tipo=FILEIRA_NORMAL; fileiras[r].n=4; }
  pedidoRelogio=pedidoAbrir=pedidoTocar=pedidoMenu=pedidoSocial=pedidoGuia=sair=0;
  okPressionando=okLongDisparado=okConsumirSoltura=0; okDesde=0; okHold=0;
  heroPos=menuN=heroAutoDesligado=menuTopo=0; sairPerguntadoEm=0;
}
static void homeKey(int type,SDL_Keycode key) {
  SDL_Event e={0}; e.type=type; e.key.keysym.sym=key; agora+=20; home_evento(&e);
}
static void addonKey(SDL_Keycode key) {
  SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=key; addonsui_evento(&e);
}
int main(void) {
  reset(1,2); foco.fileira=1; foco.coluna=2;
  homeKey(SDL_KEYDOWN,SDLK_UP);
  check(!pedidoRelogio && !focoHero && foco.fileira==0,"ordinary Up below top stays in Home rows");
  homeKey(SDL_KEYDOWN,SDLK_UP);
  check(!pedidoRelogio && focoHero,"first top-row Up reaches existing hero before clock");
  Foco before=foco; homeKey(SDL_KEYDOWN,SDLK_UP);
  check(pedidoRelogio && focoHero && !memcmp(&before,&foco,sizeof foco),"Up beyond hero requests clock without moving remembered row focus");
  check(!pedidoAbrir && !pedidoTocar && !pedidoMenu,"top request does not open title, playback or sidebar");
  homeKey(SDL_KEYUP,SDLK_UP);
  check(!pedidoRelogio,"next event clears Home clock request");
  homeKey(SDL_KEYDOWN,SDLK_DOWN);
  check(!focoHero && foco.fileira==0,"hero still returns to first row through ordinary Down");
  reset(0,1); foco.coluna=3; before=foco; homeKey(SDL_KEYDOWN,SDLK_UP);
  check(pedidoRelogio && !focoHero && !memcmp(&before,&foco,sizeof foco),"hero disabled reaches clock directly and preserves selected column");
  reset(1,0); before=foco; homeKey(SDL_KEYDOWN,SDLK_UP);
  check(pedidoRelogio && !focoHero && !memcmp(&before,&foco,sizeof foco),"empty catalogue has no invisible hero step before clock");
  reset(1,1); focoHero=1; heroCinema.oculta=1;
  homeKey(SDL_KEYDOWN,SDLK_UP);
  check(!heroCinema.oculta && !pedidoRelogio,"cinema first key restores UI before requesting clock");
  homeKey(SDL_KEYDOWN,SDLK_UP);
  check(pedidoRelogio,"clock becomes reachable after cinema UI is restored");
  reset(1,1); focoHero=1;
  homeKey(SDL_KEYUP,SDLK_RETURN);
  check(!pedidoAbrir && !pedidoTocar,"orphan OK release cannot open a title");
  homeKey(SDL_KEYDOWN,SDLK_LEFT);
  check(pedidoMenu && !pedidoRelogio,"existing left-edge sidebar request survives");
  reset(1,1); focoHero=1;
  homeKey(SDL_KEYDOWN,SDLK_RETURN); homeKey(SDL_KEYUP,SDLK_RETURN);
  check(pedidoAbrir && !pedidoRelogio,"normal short OK keeps opening the hero title");

  reset(1,1); focoHero=1; menuTopo=1;
  homeKey(SDL_KEYDOWN,SDLK_UP);
  check(pedidoMenu && !pedidoRelogio,"top menu keeps Up priority over clock");
  reset(1,2); foco.fileira=1; foco.coluna=2; foco.colunaLembrada[0]=0;
  homeKey(SDL_KEYDOWN,SDLK_UP);
  check(foco.fileira==0 && foco.coluna==2 && !pedidoRelogio,"vertical move preserves screen column instead of remembered destination");

  addonN=3; addonFoco=1; addonPedido=addonSair=addonMexeu=addonAlternados=0;
  addonKey(SDLK_UP);
  check(addonFoco==0 && !addonPedido,"Add-ons first Up reaches existing header row");
  addonKey(SDLK_UP);
  check(addonPedido && addonFoco==0 && !addonSair && !addonAlternados,"Add-ons Up beyond header requests clock without toggling a provider");
  addonKey(SDLK_DOWN);
  check(addonFoco==1 && !addonPedido,"Add-ons clears request and retains normal Down navigation");
  addonN=0; addonFoco=0; addonKey(SDLK_UP);
  check(addonPedido && !addonSair,"empty Add-ons screen also exposes clock at its top");
  printf("home_relogio: %d checks %s\n",checks,failures ? "FAILED" : "passed");
  return failures ? 1 : 0;
}
