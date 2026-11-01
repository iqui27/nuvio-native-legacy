// Desktop Settings (r13 review): pointer hover/click on a row, Cmd+, routing and
// the Mac density gated on layout_desktop_ativo(). Full desktop build, hidden GL
// window (the real draw registers the targets), isolated storage, no network.
// app.c is built with player_aberto -> nvt_player_aberto so the test can open a
// fake player. Every check prints PASS/FAIL; exit 1 on FAIL.

#include "../src/ajustes.c"
#include "app.h"
#include "nvtmp.h"
#include <assert.h>

static int falhas, playerFalso;
int nvt_player_aberto(void) { return playerFalso; }
static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }
static void confere(int ok, const char *msg) { printf("%s: %s\n", ok ? "PASS" : "FAIL", msg); falhas += !ok; }
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); gfx_novo_quadro(); tex_novo_quadro();
    glClear(GL_COLOR_BUFFER_BIT);
    ponteiro_quadro(relogio);
    ajustes_atualizar(1.0f / 60, relogio); ajustes_desenhar(relogio);
    ponteiro_desenhar(); relogio += 16;
  }
}
// Window in points; the pointer maps window -> layout 1:1.
static void janela(int w, int h) {
  layout_janela_definir(w, h); layout_tela_definir(w, h);
  glViewport(0, 0, w, h); gfx_tamanho_alvo(w, h);
  ponteiro_teste_janela((int)NV_TELA_W, (int)NV_TELA_H);
}
static void mouse(Uint32 tipo, float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo;
  if (tipo == SDL_MOUSEMOTION) { e.motion.x = (int)x; e.motion.y = (int)y; }
  else { e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)x; e.button.y = (int)y; }
  ponteiro_evento(&e, ajustes_evento);
}
static const PonteiroAlvo *alvoLinha(int i) {
  const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v);
  for (int k = 0; k < n; k++) if (v[k].focar == ajPonteiroItem && v[k].a == i) return &v[k];
  return NULL;
}
// The first visible on/off row of the open category that is not focused.
static int linhaInterruptor(void) {
  for (int i = secIni[secAtual] + 1; i < secFim(secAtual); i++)
    if (TELA[i].tipo == IT_OPC && i != focoItem && ehInterruptor(TELA[i].op) && !inativa(TELA[i].op) && alvoLinha(i)) return i;
  return -1;
}

static void hoverEClique(void) {
  int i, op, antes, sec = secAtual; float rol = scrollY;
  const PonteiroAlvo *a;
  // Since 502a111b5 the rows live in topic submenus, and the Palco shows the
  // list only after entering the category: enter it, then open topics in
  // order, as a click on the topic row would, until one shows an on/off row.
  if (!triProfundidade()) { triFocarSecao(secAtual); triEntrar(secAtual); quadros(60); }
  for (int t = secIni[secAtual] + 1; linhaInterruptor() < 0 && t < secFim(secAtual); t++)
    if (triTopico(t)) { focar(t); triSubEntrar(t); quadros(20); sec = secAtual; rol = scrollY; }
  i = linhaInterruptor();
  confere(i >= 0, "desktop: Playback has a pointer target on an on/off row");
  if (i < 0) return;
  op = TELA[i].op; a = alvoLinha(i); antes = valor[op];
  mouse(SDL_MOUSEMOTION, a->x + a->w * .5f, a->y + a->h * .5f); quadros(20);
  confere(focoItem == i && animItem[i] > .5f, "hover highlights the row under the pointer");
  confere(valor[op] == antes && secAtual == sec && fabsf(scrollY - rol) < .5f,
          "hover changes no value, category or scroll");
  a = alvoLinha(i);
  confere(a != NULL, "hovered row keeps its target");
  if (!a) return;
  mouse(SDL_MOUSEBUTTONDOWN, a->x + a->w * .5f, a->y + a->h * .5f); relogio += 60;
  mouse(SDL_MOUSEBUTTONUP, a->x + a->w * .5f, a->y + a->h * .5f); quadros(8);
  confere(valor[op] == !antes && !uxEditor, "one click on an on/off row flips it in place");
}

static void cmdVirgula(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_COMMA; e.key.keysym.mod = KMOD_LGUI;
  menu_ir(MENU_INICIO); menu_fechar();
  app_evento(&e);
  confere(menu_destino() == MENU_AJUSTES, "Cmd+, opens Settings");
  menu_ir(MENU_INICIO); menu_fechar();
  playerFalso = 1; app_evento(&e); playerFalso = 0;
  confere(menu_destino() == MENU_INICIO, "Cmd+, is ignored while the player is open");
  teclado_abrir("x", "", 8); app_evento(&e);
  confere(menu_destino() == MENU_INICIO, "Cmd+, is ignored while the keyboard is open");
}

// The TV formula of ajustes_tamanho_ajustes, without the desktop factor.
static float escalaTv(void) {
  static const float F[] = { 0.8f, 0.9f, 1.0f };
  int v = valor[AJ_TAMANHO_AJUSTES];
  float s = v >= 0 && v < 3 ? F[v] : 1.0f;
  if (gfx_escala_ui_base() < 1) s *= gfx_escala_ui_base();
  s *= gfx_escala_txt();
  return s;
}
static void densidade(void) {
  float tv, s, d;
  janela(900, 700);
  tv = escalaTv(); if (ajRetrato()) tv *= 1.25f;
  s = ajustes_tamanho_ajustes();
  confere(!layout_desktop_ativo() && !memcmp(&s, &tv, sizeof s) && TRI_SEC_W == 400.0f && TRI_INSP_W == 480.0f,
          "900x700 (below breakpoint): TV scale and column widths, byte-identical");
  confere(!ajDesktopTextoTv(NULL) && !ajDesktopTextoTv("x webOS") && ajudaOpcao(AJ_AUDIO_NORMALIZAR)[0],
          "900x700: TV-platform descriptions untouched");
  janela(1440, 900);
  d = escalaTv() * NV_DESKTOP_AJUSTES_DENSIDADE; if (ajRetrato()) d *= 1.25f;
  s = ajustes_tamanho_ajustes();
  confere(layout_desktop_ativo() && !memcmp(&s, &d, sizeof s) && TRI_SEC_W == 470.0f && TRI_INSP_W == 560.0f,
          "1440x900 (desktop): Mac density and wider columns");
  confere(!ajudaOpcao(AJ_AUDIO_NORMALIZAR)[0] && !uxBeneficio(AJ_AUDIO_NORMALIZAR)[0] && ajudaOpcao(AJ_QUALIDADE)[0],
          "1440x900: descriptions naming webOS/Tizen/Android are hidden, others kept");
}

int main(void) {
  char dir[1024];
  SDL_Window *win;
  snprintf(dir, sizeof dir, "%s/nuvio-desktop-settings-r13-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Desktop Settings r13", 0, 0, 1440, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); ajustes_recursos("deploy/app/art");
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); player_dir(dir); fontefalha_dir(dir);
  ajustes_iniciar(); assert(gravar());
  ponteiro_iniciar(); ponteiro_teste_relogio(agora);
  densidade();
  janela(1440, 900);
  triIniciar(); focarSecao(0); quadros(40);
  hoverEClique();
  cmdVirgula();
  if (!falhas) puts("PASS: desktop Settings r13: hover, click, Cmd+, and density gate");
  return falhas ? 1 : 0;
}
