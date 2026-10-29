// Real GL regression and screenshots for accent focus, independent of glass.
#include <SDL2/SDL_image.h>
#include "../src/ajustes.c"
#include "nvtmp.h"
#include "plrui.h"
#include "salvospainel.h"
#include "pessoas.h"
#include "socialvis.h"
#include "detail.h"
#include "menu.h"
#include <assert.h>
#include <unistd.h>

#define W 1920
#define H 1080
static unsigned char pixels[W * H * 4], flipped[W * H * 4];
static const char *out;
static Uint32 clock_ms = 1000;
Uint32 accent_ticks(void) { return clock_ms; }
time_t accent_time(time_t *out_time) { time_t now=1800000000; if (out_time) *out_time=now; return now; }
// Local contacts only; no server or account needed for the real sidebar.
int recomenda_contatos(RecContato *dst, int max) {
  int n = socialvis_n_amigos(); if (n > max) n = max;
  for (int i = 0; i < n; i++) {
    const SvAmigo *a = socialvis_amigo(i); memset(&dst[i],0,sizeof dst[i]);
    snprintf(dst[i].id,sizeof dst[i].id,"%s",a->id);
    snprintf(dst[i].nome,sizeof dst[i].nome,"%s",a->nome);
  }
  return n;
}
static void save(const char *screen, int glass, int accent, int enabled) {
  char path[1200];
  glFinish();
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < H; y++) memcpy(flipped + y * W * 4, pixels + (H - 1 - y) * W * 4, W * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(flipped, W, H, 32, W * 4, SDL_PIXELFORMAT_RGBA32);
  snprintf(path, sizeof path, "%s/%s-%s-%s-%s.png", out, screen, glass ? "glass" : "solid", accent ? "orange" : "default", enabled ? "on" : "off");
  assert(s && IMG_SavePNG(s, path) == 0); SDL_FreeSurface(s);
}
static void clear(void) { gfx_novo_quadro(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); glClearColor(.08f,.085f,.10f,1); glClear(GL_COLOR_BUFFER_BIT); }
static void pixel(unsigned char *p) { glReadPixels(700, H - 346, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, p); }
static void frame(int screen) {
  Uint32 now = clock_ms += 17;
  clear();
  if (screen == 0) { home_atualizar(1.f/60, now); home_desenhar(now); menu_atualizar(1.f/60, now); menu_desenhar(now); }
  if (screen == 1) { home_desenhar(now); spainel_atualizar(1.f/60, now); spainel_desenhar(now); }
  if (screen == 2) { ajustes_atualizar(1.f/60, now); ajustes_desenhar(now); }
  if (screen == 3) { detail_atualizar(1.f/60, now); detail_desenhar(now); }
  if (screen == 4) { pessoas_atualizar(1.f/60, now); pessoas_desenhar(now); }
}
int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1],"--compare")) {
    // Compare the focus rendering itself; whole screens load art asynchronously.
    const char *screens[]={"focus"};
    for (int s=0;s<1;s++) for (int g=0;g<2;g++) for (int a=0;a<2;a++) {
      char path[1200]; SDL_Surface *im[2];
      for (int j=0;j<2;j++) {
        snprintf(path,sizeof path,"%s/%s-%s-%s-off.png",argv[j+2],screens[s],g?"glass":"solid",a?"orange":"default");
        SDL_Surface *raw=IMG_Load(path); assert(raw); im[j]=SDL_ConvertSurfaceFormat(raw,SDL_PIXELFORMAT_RGBA32,0); SDL_FreeSurface(raw); assert(im[j]);
      }
      int differences=0;
      for (int y=0;y<H;y++) for (int x=0;x<W*4;x++) differences+=((unsigned char *)im[0]->pixels)[y*im[0]->pitch+x]!=((unsigned char *)im[1]->pixels)[y*im[1]->pitch+x];
      printf("OFF %s glass=%d accent=%d: %d changed bytes\n",screens[s],g,a,differences);
      assert(!differences); SDL_FreeSurface(im[0]); SDL_FreeSurface(im[1]);
    }
    return 0;
  }
  out = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/rev/amigosacento/after";
  dados_iniciar(getenv("NUVIO_DADOS"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Accent focus regression", 0, 0, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w)); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, W, H); gfx_tamanho_alvo(W,H);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  ajustes_iniciar(); ajustes_recursos("deploy/app/art");
  assert(home_iniciar("deploy/app/art")); menu_iniciar();
  recomenda_responder_aparecer(1); recomenda_responder_alcance(1); socialvis_demo(4);
  static CatItem items[6]; static CatFileira row;
  for (int i=0;i<6;i++) {
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt900000%d",i);
    snprintf(items[i].titulo,sizeof items[i].titulo,"Example %d",i+1);
    snprintf(items[i].tipo,sizeof items[i].tipo,"movie");
    snprintf(items[i].backdrop,sizeof items[i].backdrop,"deploy/app/art/%02d.jpg",i);
    snprintf(items[i].poster,sizeof items[i].poster,"deploy/app/art/poster/%02d.jpg",i);
  }
  snprintf(row.chave,sizeof row.chave,"pop_movie"); snprintf(row.titulo,sizeof row.titulo,"Popular"); row.n=6;
  cat_definir_tudo(items,6,&row,1);
  AddonRemoto addon = {"Local fixture", "http://127.0.0.1:1/manifest.json", 1}; addons_definir_lista(&addon,1);
  // Warm local art before comparing deterministic frames.
  for (int i=0;i<6;i++) { tex_obter(items[i].backdrop); tex_obter(items[i].poster); }
  for (int i=0;i<120;i++) { tex_bombear(10); SDL_Delay(2); }
  for (int glass = 0; glass < 2; glass++) for (int accent = 0; accent < 2; accent++) for (int enabled = 0; enabled < 2; enabled++) {
    valor[AJ_VIDRO] = glass ? 0 : 1; valor[AJ_TEMA] = accent ? 5 : 0; valor[AJ_FOCO_ACENTO] = enabled ? 0 : 1;
    valor[AJ_ANIM] = 1;
    const char *names[] = {"home", "friends", "settings", "detail", "people"};
    for (int screen = 0; screen < 5; screen++) {
      spainel_fechar(); menu_fechar(); detail_fechar_seco();
      if (screen == 0) { menu_abrir(); }
      if (screen == 1) { spainel_abrir(); spainel_ir_aba(2); SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; spainel_evento(&e); spainel_evento(&e); }
      if (screen == 2) { focarOpcao(AJ_FOCO_ACENTO); ajcTesteT = 1.0f; }
      if (screen == 3) { HomeItem item = {0}; item.indice = 0; item.rect = (GfxRect){300,300,400,225}; item.arte = "deploy/app/art/00.jpg"; item.titulo = "Fallout"; item.genero = "Drama"; item.meta = "2024"; detail_abrir(&item); }
      if (screen == 4) pessoas_abrir();
      for (int i = 0; i < 90; i++) { SDL_PumpEvents(); frame(screen); SDL_Delay(5); }
      save(names[screen],glass,accent,enabled);
    }
  }
  // The disabled helper must leave the legacy focus unchanged.
  GfxRect r = {400,300,600,90};
  for (int glass = 0; glass < 2; glass++) for (int accent = 0; accent < 2; accent++) {
    unsigned char old[3], current[3];
    valor[AJ_VIDRO] = glass ? 0 : 1; valor[AJ_FOCO_ACENTO] = 1; valor[AJ_TEMA] = accent ? 5 : 0;
    clear(); plrui_linha_foco(r,22,1); pixel(old);
    save("focus",glass,accent,0);
    clear(); assert(!gfx_foco_placa(r,22.f/r.h,1,1)); plrui_linha_foco(r,22,1); pixel(current);
    assert(!memcmp(old,current,3));
  }
  valor[AJ_VIDRO] = 1; valor[AJ_TEMA] = 5; valor[AJ_FOCO_ACENTO] = 0;
  clear(); plrui_linha_foco(r,22,1);
  unsigned char p[3]; pixel(p);
  printf("glass off + toggle on: plate RGB %d,%d,%d\n",p[0],p[1],p[2]); fflush(stdout);
  {
    assert(p[0] > p[1] + 12 && p[1] > p[2] + 3);
    assert(ajustes_foco_acento()); assert(!inativa(AJ_FOCO_ACENTO)); assert(uxRequisito(AJ_FOCO_ACENTO) == -1);
    // Light text remains readable on every fixed accent in the solid material.
    for (int theme=0;theme<AJ_N_TEMAS_OPC;theme++) if (TEMA_ACENTO[theme].fam) {
      valor[AJ_TEMA]=theme; clear(); assert(gfx_foco_placa(r,22.f/r.h,1,1)); pixel(p);
      float y=0;
      const float weights[3]={.2126f,.7152f,.0722f};
      for (int c=0;c<3;c++) { float v=p[c]/255.f; y+=weights[c]*(v<=.04045f?v/12.92f:powf((v+.055f)/1.055f,2.4f)); }
      assert((.9f+.05f)/(y+.05f)>=4.5f);
    }
  }
  puts("amigos_acento: finished"); return 0;
}
