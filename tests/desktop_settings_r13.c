// Desktop Settings (r13 review): pointer hover/click on a row, Cmd+, routing and
// the Mac density gated on layout_desktop_ativo(). Full desktop build, hidden GL
// window (the real draw registers the targets), isolated storage, no network.
// app.c is built with player_aberto -> nvt_player_aberto so the test can open a
// fake player. Every check prints PASS/FAIL; exit 1 on FAIL.

#include "../src/ajustes.c"
#include "app.h"
#include "nvtmp.h"
#include <assert.h>
#include <sys/wait.h>

static void isolamentoCaptura(const char *dir) {
  // Fork before SDL starts threads; rejected paths must exit before any fallback.
  for (int missing = 0; missing < 2; missing++) {
    pid_t child = fork(); assert(child >= 0);
    if (!child) {
      assert(setenv("NV_CAPTURA", "1", 1) == 0);
      if (missing) assert(unsetenv("NUVIO_DADOS") == 0);
      else {
        char invalid[1200]; snprintf(invalid, sizeof invalid, "%s/not-a-directory/child", dir);
        char file[1200]; snprintf(file, sizeof file, "%s/not-a-directory", dir);
        FILE *f = fopen(file, "w"); assert(f && fclose(f) == 0);
        assert(setenv("NUVIO_DADOS", invalid, 1) == 0);
      }
      dados_iniciar(NULL);
      _exit(0);
    }
    int status; assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 78);
  }
  puts("PASS: capture rejects missing/unwritable isolation without data fallback");
}

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

// Palco (ajustesblades, wide layouts incl. 1440x900): the category list is a
// blade, every group in it is a topic row and the on/off rows live in the topic
// blade. One click on the category opens both (depth 2, its first topic).
static void abrirTopico(void) {
  const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v), sec = secAtual;
  for (int k = 0; k < n; k++) if (v[k].focar == triPontSecao && v[k].a == sec) {
    float x = v[k].x + v[k].w * .5f, y = v[k].y + v[k].h * .5f;
    mouse(SDL_MOUSEBUTTONDOWN, x, y); relogio += 60; mouse(SDL_MOUSEBUTTONUP, x, y); quadros(40);
    break;
  }
  confere(secAtual == sec && triProfundidade() == 2 && triTopico(triSubmenu),
          "desktop: one click on a Palco category opens it and its first topic blade");
}

static void hoverEClique(void) {
  int i, op, antes, sec;
  float rol;
  const PonteiroAlvo *a;
  if (triBladeLayout()) abrirTopico();
  else if (!triProfundidade()) { triFocarSecao(secAtual); triEntrar(secAtual); quadros(60); }
  for (int t = secIni[secAtual] + 1; linhaInterruptor() < 0 && t < secFim(secAtual); t++)
    if (triTopico(t)) { focar(t); triSubEntrar(t); quadros(20); }
  sec = secAtual; rol = scrollY;
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
static void controlesNativos(void) {
#ifdef NV_DESKTOP_NATIVE_CONTROLS
  unsigned char on[3], off[3];
  float scale = gfx_escala();
  janela(1440, 900); gfx_escala_sair(1);
  gfx_novo_quadro(); glClear(GL_COLOR_BUFFER_BIT);
  desktop_native_control(1000, 0, "", 1, 1, 100, 50, 108, 48, 1);
  int read = gfx_tela_px(120.0f / NV_TELA_W, 74.0f / NV_TELA_H, on);
  glClear(GL_COLOR_BUFFER_BIT);
  desktop_native_control(1000, 0, "", 0, 1, 100, 50, 108, 48, 1);
  read &= gfx_tela_px(120.0f / NV_TELA_W, 74.0f / NV_TELA_H, off);
  confere(read && on[2] > on[0] + 40 && abs(on[2] - off[2]) > 30,
          "AppKit switch has visible blue pixels in GL readback and changes on toggle");
  gfx_escala_sair(scale);
#endif
}
static void pranchaDesktop(void) {
  janela(1440, 900);
  float s = ajustes_tamanho_ajustes() * .75f;
  GfxRect sections = triSecR(), list = triIlha2R(), inspector = triInspR();
  confere(!triBladeLayout() && triTemInsp(), "approved Mac board keeps three columns at every depth");
  confere(fabsf(sections.x * s - 304) < 1 && fabsf(sections.y * s - 156) < 1 &&
          fabsf(sections.w * s - 226) < 1 && fabsf(inspector.w * s - 270) < 1 &&
          fabsf(list.x * s - 546) < 1 && fabsf((list.y + list.h) * s - 864) < 1,
          "approved 1440x900 board: section, list and inspector bounds");
  confere(fabsf(AJ2_LN_H * s - 38) < 1, "desktop rows keep the board's 38pt rhythm");
}
static void densidade(void) {
  float tv, s, d;
  janela(900, 700);
  tv = escalaTv(); if (ajRetrato()) tv *= 1.25f;
  s = ajustes_tamanho_ajustes();
  confere(!layout_desktop_ativo() && !memcmp(&s, &tv, sizeof s) && TRI_SEC_W == 400.0f && TRI_INSP_W == 480.0f,
          "900x700 (below breakpoint): TV scale and column widths, byte-identical");
  confere(!ajDesktopTextoTv(NULL) && !ajDesktopTextoTv("x webOS") && ajudaOpcao(AJ_AUDIO_NORMALIZAR)[0] &&
          strstr(ajudaOpcao(AJ_RETOMADA_MORNA), "Android"),
          "900x700: TV-platform descriptions untouched");
  janela(1440, 900);
  d = escalaTv() * NV_DESKTOP_AJUSTES_DENSIDADE; if (ajRetrato()) d *= 1.25f;
  s = ajustes_tamanho_ajustes();
  confere(layout_desktop_ativo() && !memcmp(&s, &d, sizeof s) && fabsf(TRI_SEC_W * s - 904.0f / 3) < .01f && fabsf(TRI_INSP_W * s - 360) < .01f,
          "1440x900 (desktop): Mac density and wider columns");
  confere(!ajudaOpcao(AJ_AUDIO_NORMALIZAR)[0] && !uxBeneficio(AJ_AUDIO_NORMALIZAR)[0] && ajudaOpcao(AJ_QUALIDADE)[0],
          "1440x900: descriptions naming webOS/Tizen/Android are hidden, others kept");
  // Only the sentence naming the platform goes; the rest stays (pt and en).
  { const char *m;
    int ok, idioma = valor[AJ_IDIOMA];
    valor[AJ_IDIOMA] = IDIOMA_PT + 1; m = i18n(ajudaOpcao(AJ_RETOMADA_MORNA));
    ok = !strncmp(m, "Ligado por padrão", 18) && strstr(m, "Retomar.") && !strstr(m, "Android");
    valor[AJ_IDIOMA] = IDIOMA_EN + 1;
    m = i18n(ajudaOpcao(AJ_RETOMADA_MORNA));
    ok = ok && !strncmp(m, "On by default", 13) && !strstr(m, "Android");
    valor[AJ_IDIOMA] = idioma;
    confere(ok, "1440x900: only the sentence naming Android leaves the Warm resume description"); }
  // "Applies to" never says TV on a computer (pt and en).
  { int tv = 0, idioma = valor[AJ_IDIOMA], k;
    for (k = 0; k < 2; k++) {
      valor[AJ_IDIOMA] = k ? IDIOMA_EN + 1 : IDIOMA_PT + 1;
      for (int op = 0; op < AJ_N; op++) tv += strstr(i18n(uxEscopo(op)), "TV") != NULL;
      tv += strstr(i18n(uxEscopoValor(AJ_TEMA, AJ_TEMA_DINAMICA)), "TV") != NULL;
    }
    valor[AJ_IDIOMA] = idioma;
    confere(!tv, "desktop: Applies to is device-neutral (no TV)"); }
}

int main(void) {
  char dir[1024];
  SDL_Window *win;
  snprintf(dir, sizeof dir, "%s/nuvio-desktop-settings-r13-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  isolamentoCaptura(dir);
  assert(setenv("NV_CAPTURA", "1", 1) == 0);
  SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
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
  pranchaDesktop();
  controlesNativos();
  janela(1440, 900);
  triIniciar(); focarSecao(0); quadros(40);
  hoverEClique();
  cmdVirgula();
  if (!falhas) puts("PASS: desktop Settings r13: hover, click, Cmd+, and density gate");
  return falhas ? 1 : 0;
}
