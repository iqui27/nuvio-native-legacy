#include "../src/ajustes.c"
#include "nvtmp.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <unistd.h>

static const char *dirPng;   // argv[1]: saves a PNG per scene (review evidence)
static SDL_Window *win;
static int telaW = 1920, telaH = 1080;
static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }
static void key(SDL_Keycode k) { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; ajustes_evento(&e); }
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); gfx_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    ponteiro_quadro(relogio);
    ajustes_atualizar(1.0f / 60, relogio); ajustes_desenhar(relogio);
    ponteiro_desenhar(); relogio += 16;
  }
}
static void png(const char *nome) {
  unsigned char *px; SDL_Surface *sf; char path[1024];
  if (!dirPng) return;
  px = malloc((size_t)telaW * telaH * 4); sf = SDL_CreateRGBSurfaceWithFormat(0, telaW, telaH, 32, SDL_PIXELFORMAT_RGBA32);
  assert(px && sf);
  glReadPixels(0, 0, telaW, telaH, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (int y = 0; y < telaH; y++) memcpy((char *)sf->pixels + y * sf->pitch, px + (size_t)(telaH - 1 - y) * telaW * 4, (size_t)telaW * 4);
  snprintf(path, sizeof path, "%s/%s.png", dirPng, nome);
  assert(IMG_SavePNG(sf, path) == 0); SDL_FreeSurface(sf); free(px);
}
// One square hidden window (1920 x 1920): the layout only moves the viewport,
// so portrait draws on a 1080 x 1920 target without resizing the window
// (SDL applies a resize of a hidden window late and the scissor lagged).
static void tela(int w, int h) {
  telaW = w; telaH = h;
  layout_tela_definir(w, h);   // first: gfx_tamanho_alvo reads NV_TELA_W/H for its projection
  glViewport(0, 0, w, h); gfx_tamanho_alvo(w, h); ponteiro_teste_janela(w, h);
}
int main(int argc, char **argv) {
  char dir[1024];
  if (argc > 1) dirPng = argv[1];
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-tri-p1-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Settings P1", 0, 0, 1920, 1920, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win)); glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64); IMG_Init(IMG_INIT_PNG);
  gfx_icones_dir("deploy/app/art"); ajustes_recursos("deploy/app/art");
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); player_dir(dir); fontefalha_dir(dir);
  ajustes_iniciar(); assert(gravar());
  layout_modo_definir(0); layout_tela_definir(1920, 1080);
  ponteiro_iniciar(); ponteiro_teste_toque(1); ponteiro_teste_relogio(agora); ponteiro_teste_janela(1920, 1080);
  quadros(5);
  for (int portrait = 0; portrait < 2; portrait++) {
    layout_modo_definir(portrait); tela(portrait ? 1080 : 1920, portrait ? 1920 : 1080);
    const char *names[] = {"main", "home", "appearance", "submenu", "toggle", "slider", "picker", "action", "reading"};
    int ops[] = {-1, -1, -1, AJ_HERO, AJ_PAUSA_OVERLAY, AJ_LEG1_TAMANHO, AJ_AUD_LINGUA, AJ_FIL_ORDEM, AJ_VERSAO_I};
    for (int scene = 0; scene < 9; scene++) {
      uxCancelar(); triIniciar(); scrollY = velY = 0;
      if (scene == 0) triFocarSecao(0);
      else if (scene == 1 || scene == 2) { triFocarSecao(scene == 1 ? 4 : 5); triEntrar(secAtual); }
      else focarOpcao(ops[scene]);
      quadros(90);
      if (portrait && scene == 0) {
        const PonteiroAlvo *targets;
        int count = ponteiro_teste_lista(&targets), found = 0;
        assert(fabsf(triSecR().x + triSecR().w - NV_VTELA_W) < 0.1f);
        for (int i = 0; i < count; i++) if (targets[i].ativar == triPontSecaoAbrir && targets[i].a == 0) {
          assert(fabsf(targets[i].x + targets[i].w - NV_LAYOUT_REAL_W) < 1.0f);
          targets[i].ativar(0, 0); found = 1; break;
        }
        assert(found && triIlha == TRI_LISTA && !focoIndice);
        quadros(60);
        count = ponteiro_teste_lista(&targets); found = 0;
        for (int i = 0; i < count; i++) if (targets[i].ativar == triPontSubVoltar) {
          targets[i].ativar(0, 0); found = 1; break;
        }
        assert(found && triIlha == TRI_SECOES && focoIndice);
        quadros(60);
      }
      if (portrait && scene == 1) {
        GfxRect rows = triListaR();
        assert(fabsf(rows.x + 18.0f - triIlha2R().x) < 0.1f);
        assert(fabsf(rows.x + rows.w - 18.0f - NV_VTELA_W) < 0.1f);
      }
      char name[100]; snprintf(name, sizeof name, "%s-%s", portrait ? "phone" : "tv", names[scene]); png(name);
    }
  }
#ifdef AJ_TRI_POLISH
  layout_modo_definir(0); tela(1920,1080); triIniciar(); triFocarSecao(4); triEntrar(4);
  int topic = focoItem;
  valor[AJ_ANIM] = 0;
  assert(triTopico(topic)); key(SDLK_RIGHT); assert(triSubmenu == topic && focoOp >= 0 && paginaA == 0);
  quadros(1); assert(paginaA > 0 && paginaA < 1); quadros(30); assert(paginaA == 1);
  int first = focoItem; key(SDLK_DOWN); assert(focoItem > first);
  key(SDLK_ESCAPE); assert(triSubmenu < 0 && focoItem == topic && triIlha == TRI_LISTA);
  key(SDLK_RETURN); assert(triSubmenu == topic); key(SDLK_LEFT); assert(triSubmenu < 0 && focoItem == topic);
  key(SDLK_ESCAPE); assert(triIlha == TRI_SECOES && triSecFoco == 4);
  key(SDLK_DOWN); assert(triSecFoco == 5); key(SDLK_RIGHT); assert(triIlha == TRI_LISTA);
  focarOpcao(AJ_HERO); assert(triSubmenu >= 0 && visivel(focoItem) && triMostrar(focoItem));
  key(SDLK_ESCAPE); assert(triSubmenu < 0 && TELA[focoItem].tipo == IT_ROT);
  key(SDLK_RETURN); assert(triSubmenu >= 0);
  focarOpcao(AJ_AUD_LINGUA); int owner = triSubmenu;
  uxAbrirEditor(AJ_AUD_LINGUA); key(SDLK_ESCAPE);
  assert(!uxEditor && triSubmenu == owner && focoOp == AJ_AUD_LINGUA);
  key(SDLK_ESCAPE); assert(triSubmenu < 0 && focoItem == owner);
  // Hidden GL draws register the real clickable topic and Back targets.
  triFocarSecao(4); triEntrar(4); quadros(60);
  const PonteiroAlvo *targets; int count = ponteiro_teste_lista(&targets), found = 0;
  for (int i = 0; i < count; i++) if (targets[i].focar == ajPonteiroItem && targets[i].a == focoItem) {
    targets[i].focar(targets[i].a, 0); key(SDLK_RETURN); found = 1; break;
  }
  assert(found && triSubmenu >= 0); quadros(60);
  count = ponteiro_teste_lista(&targets); found = 0;
  for (int i = 0; i < count; i++) if (targets[i].ativar == triPontSubVoltar) {
    targets[i].ativar(0, 0); found = 1; break;
  }
  assert(found && triSubmenu < 0);
  layout_modo_definir(1); tela(1080,1920); focarOpcao(AJ_LEG1_TAMANHO); quadros(90);
  assert(triIlha == TRI_LISTA && !focoIndice && triSubmenu >= 0);
  puts("PASS: submenu enter/Back/Left, editor cancellation, exact return row, category focus chain, deep links, pointer targets, portrait pages, 220ms animation");
#endif
  return 0;
}
