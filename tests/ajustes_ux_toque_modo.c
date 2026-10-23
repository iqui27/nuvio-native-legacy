// Mobile targets/editing must not change TV state, even after a mode switch.
#include "text.h"
#include "gfx.h"
static int medirDica(TxtEstilo estilo, const char *s);
static void recorteTeste(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
static void semRecorteTeste(void) {}
#define txt_largura medirDica
#define gfx_recorte recorteTeste
#define gfx_sem_recorte semRecorteTeste
#include "../src/ajustes.c"
#undef txt_largura
#undef gfx_recorte
#undef gfx_sem_recorte
#include <assert.h>

static int falhas, dicaToque, dicaSetas;
#define CONFERE(c, msg) do { if (!(c)) { puts("FALHA: " msg); falhas++; } } while (0)
static int medirDica(TxtEstilo estilo, const char *s) {
  if (!strcmp(s, "Toque")) dicaToque++;
  if (!strcmp(s, "↑ ↓")) dicaSetas++;
  return txt_largura(estilo, s);
}
static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }
static void entregar(const SDL_Event *e) { (void)e; }
static void quadroFileiras(void) {
  ponteiro_quadro(relogio);
  desenhaFileiras();
  ponteiro_desenhar();
  relogio += 400;
}
static void clique(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
  e.button.x = (int)x; e.button.y = (int)y;
  ponteiro_evento(&e, entregar);
  e.type = SDL_MOUSEBUTTONUP; relogio += 60;
  ponteiro_evento(&e, entregar);
}
#ifdef NV_TOUCH_UI
static void fileiras(int mobile) {
  const PonteiroAlvo *v;
  layout_modo_definir(mobile);
  fil_esquecer();
  fil_definir_perfil(mobile + 1);
  fil_registrar("test_a", "A", "Test", "movie", 1);
  fil_registrar("test_b", "B", "Test", "movie", 1);
  assert(fil_n() == 2 && fil_estado(0) == FIL_NA_HOME && fil_estado(1) == FIL_NA_HOME);
  filAberta = 1; filAba = 0; filNaBarra = 0;
  filFoco = 1; filCampo = 0; filPegou = 0; filTopo = 0;
  eventoFileiras(SDLK_RETURN);  // remote OK picks A in either mode
  assert(filPegou == 1);
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  dicaToque = dicaSetas = 0;
  quadroFileiras();
  AjFilMedidas m = ajFilMedir();
  float x = m.cx + 10, y = m.ilha.y + 183 + ajFilToqueTopo(2) + AJF_ROW_H * 0.5f;
  int n = ponteiro_teste_lista(&v);
  if (mobile) {
    CONFERE(n > 0, "Mobile: registra alvos de Fileiras");
    CONFERE(dicaToque && !dicaSetas, "Mobile: conserva dicas de toque");
  } else {
    CONFERE(n == 0, "TV: Fileiras nao registra alvos Mobile");
    CONFERE(!dicaToque && dicaSetas, "TV: conserva dicas do controle");
  }
  if (mobile) {
    int alvo = ponteiro_achar(v, n, x, y);
    assert(alvo >= 0 && v[alvo].a == 2 && v[alvo].ativar == ajFilToqueSoltar);
  }
  clique(x, y);
  if (mobile) {
    CONFERE(!strcmp(fil_chave(0), "test_b") && !strcmp(fil_chave(1), "test_a") && filPegou == 0,
            "Mobile: clicar B move A e solta");
    eventoFileiras(SDLK_RETURN);
    ponteiro_iniciar(); ponteiro_teste_toque(1);
    quadroFileiras();             // targets left over from Mobile
    layout_modo_definir(0);
    clique(x, y);
    CONFERE(!strcmp(fil_chave(0), "test_b") && filPegou == 1,
            "TV: alvo remanescente de Mobile nao solta nem reordena");
  } else {
    CONFERE(!strcmp(fil_chave(0), "test_a") && !strcmp(fil_chave(1), "test_b") && filPegou == 1,
            "TV: clicar B nao reordena nem solta A");
    ajFilToqueSoltar(2, 0);
    CONFERE(!strcmp(fil_chave(0), "test_a") && filPegou == 1, "TV: callback de soltar e inerte");
  }
  filPegou = 0; filFoco = 1; filCampo = 0;
  layout_modo_definir(0);
  ajFilToqueFocar(2, 3); ajFilToqueAba(1, 0);
  CONFERE(filFoco == 1 && filCampo == 0 && filAba == 0, "TV: callbacks de foco e aba sao inertes");
}
#endif

// A TV-mode phone still needs real targets to select Mobile: the closed app
// sidebar registers a target after Settings, so the empty-layer Enter fallback
// is unavailable. Exercise the production editor and pointer dispatch together.
#ifdef NV_TOUCH_UI
static void alvoLateral(int a, int b) { (void)a; (void)b; assert(!"clicou a barra lateral"); }
static void quadroInterface(void) {
  ponteiro_teste_janela((int)NV_TELA_W, (int)NV_TELA_H);
  ponteiro_quadro(relogio);
  ponteiro_camada(); ajToqueCamada();
  ajPontEditor = aj2EditorPainel();   // the editor has finished its entrance
  if (ajPontEditor) {
    GfxRect m = aj2EditorR();
    // Match ajustes_ux_interacao.c: keep real choices and footer targets,
    // without the GPU preview column in this windowless module test.
    if (m.w > 1100.0f) m.w = 1100.0f;
    AJ_ESCALA_INI();
    aj2EditorEscolha(m);
    AJ_ESCALA_FIM();
  }
  ponteiro_alvo(0, 0, 28, NV_TELA_H, NULL, alvoLateral, 0, 0);
  ponteiro_desenhar(); relogio += 400;
}
static int alvoInterface(PonteiroFn focar, PonteiroFn ativar, int a, float *x, float *y) {
  const PonteiroAlvo *v;
  int n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) {
    if (v[i].focar != focar || v[i].ativar != ativar || v[i].a != a) continue;
    *x = v[i].x + v[i].w * 0.5f; *y = v[i].y + v[i].h * 0.5f;
    assert(*x >= 0 && *x < NV_LAYOUT_REAL_W && *y >= 0 && *y < NV_LAYOUT_REAL_H);
    assert(ponteiro_achar(v, n, *x, *y) == i);
    return 1;
  }
  return 0;
}
static void clicarInterface(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_FINGERDOWN; e.tfinger.touchId = 1; e.tfinger.fingerId = 1;
  e.tfinger.x = x / NV_TELA_W; e.tfinger.y = y / NV_TELA_H;
  ponteiro_evento(&e, ajustes_evento);
  e.type = SDL_FINGERUP; relogio += 60;
  ponteiro_evento(&e, ajustes_evento);
  relogio += 400;
}
#endif
static void clicarMouseInterface(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
  e.button.x = (int)x; e.button.y = (int)y;
  ponteiro_evento(&e, ajustes_evento);
  e.type = SDL_MOUSEBUTTONUP; relogio += 60;
  ponteiro_evento(&e, ajustes_evento);
  relogio += 400;
}
#ifndef NV_TOUCH_UI
// The cursor comes back with a continuous movement (as in tests/menuep.c):
// without it ponteiro_ativo() is false and the desktop frame registers nothing.
static void acordarInterface(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  relogio += 400;
  for (int i = 0; i < 6; i++) {
    e.motion.x = 1700 + i * 30; e.motion.y = 20 + i * 4;
    relogio += 40;
    ponteiro_evento(&e, ajustes_evento);
  }
}
#endif
static char *arquivoAjustes(void) {
  char caminho[700];
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(caminho, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); assert(n >= 0);
  rewind(f); char *s = malloc((size_t)n + 1); assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); s[n] = 0; fclose(f);
  return s;
}
#ifdef NV_TOUCH_UI
static int abrirInterface(void) {
  ajustes_iniciar(); focarOpcao(AJ_INTERFACE_MODO); uxAbrirEditor(AJ_INTERFACE_MODO);
  CONFERE(uxEditor == 1 && uxOp == AJ_INTERFACE_MODO, "Modo da interface: abre para editar");
  CONFERE(!ajEmLinha(AJ_INTERFACE_MODO) && aj2EditorPainel(),
          "Modo da interface: usa o seletor com alvos proprios tambem em TV");
  if (!aj2EditorPainel()) return 0;
  quadroInterface();
  float x, y;
  CONFERE(alvoInterface(NULL, ajToqueConfirmar, 0, &x, &y), "Modo da interface: registra Confirmar");
  CONFERE(alvoInterface(NULL, ajToqueCancelar, 0, &x, &y), "Modo da interface: registra Cancelar");
  return 1;
}
static void interfacePorToque(void) {
  const char *dir = getenv("NUVIO_DADOS");
  float x, y;
  ajustes_dir(dir);
  valor[AJ_INTERFACE_MODO] = 0; interfaceAplicar(); assert(gravar());
  layout_tela_definir(1080, 2340);   // phone drawable, saved TV canvas
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  assert(!layout_modo_mobile());
  if (!abrirInterface()) return;

  // Cancel after moving the pending choice, with no write to the settings file.
  char *antes = arquivoAjustes();
  uxEvento(SDLK_DOWN); assert(uxPendente == 1 && valor[AJ_INTERFACE_MODO] == 0);
  quadroInterface();
  assert(alvoInterface(NULL, ajToqueCancelar, 0, &x, &y)); clicarInterface(x, y);
  char *depois = arquivoAjustes();
  CONFERE(!uxEditor && !layout_modo_mobile() && valor[AJ_INTERFACE_MODO] == 0,
          "TV: Cancelar descarta Mobile pendente");
  CONFERE(!strcmp(antes, depois), "TV: Cancelar nao grava a escolha pendente");
  free(antes); free(depois);

  if (!abrirInterface()) return;
  assert(alvoInterface(ajPontEdValor, NULL, 1, &x, &y));
  clicarInterface(x, y);    // focus Mobile + pointer-generated Enter, real handler
  CONFERE(!uxEditor && layout_modo_mobile() && valor[AJ_INTERFACE_MODO] == 1,
          "TV no celular: tocar Mobile aplica a escolha apesar do alvo lateral");
  depois = arquivoAjustes();
  CONFERE(strstr(depois, "\ninterfaceModoLocal 1\n") != NULL, "Mobile fica gravado neste aparelho");
  free(depois);
  valor[AJ_INTERFACE_MODO] = 0; layout_modo_definir(0); ajustes_dir(dir);
  CONFERE(valor[AJ_INTERFACE_MODO] == 1 && layout_modo_mobile(), "Mobile sobrevive a nova leitura dos ajustes");

  if (!abrirInterface()) return;
  uxEvento(SDLK_UP); assert(uxPendente == 0 && valor[AJ_INTERFACE_MODO] == 1);
  quadroInterface();
  assert(alvoInterface(NULL, ajToqueConfirmar, 0, &x, &y)); clicarInterface(x, y);
  CONFERE(!uxEditor && !layout_modo_mobile() && valor[AJ_INTERFACE_MODO] == 0,
          "Mobile: tocar Confirmar aplica TV pelo mesmo seletor");
  valor[AJ_INTERFACE_MODO] = 1; layout_modo_definir(1); ajustes_dir(dir);
  CONFERE(valor[AJ_INTERFACE_MODO] == 0 && !layout_modo_mobile(), "TV tambem fica salva na nova leitura");

  // Auto is a real third saved choice, not a one-time replacement by TV/Mobile.
  if (!abrirInterface()) return;
  uxEvento(SDLK_DOWN); uxEvento(SDLK_DOWN);
  assert(uxPendente == 2 && valor[AJ_INTERFACE_MODO] == 0);
  quadroInterface();
  assert(alvoInterface(NULL, ajToqueCancelar, 0, &x, &y)); clicarInterface(x, y);
  CONFERE(!uxEditor && valor[AJ_INTERFACE_MODO] == 0 && !layout_modo_mobile(),
          "TV: Cancelar descarta Automatico pendente");
  if (!abrirInterface()) return;
  assert(alvoInterface(ajPontEdValor, NULL, 2, &x, &y)); clicarInterface(x, y);
  CONFERE(!uxEditor && valor[AJ_INTERFACE_MODO] == 2,
          "TV: tocar Automatico confirma a terceira escolha");
  depois = arquivoAjustes();
  CONFERE(strstr(depois, "\ninterfaceModoLocal 2\n") != NULL,
          "Automatico fica gravado sem substituir a preferencia pelo modo resolvido");
  free(depois);
  valor[AJ_INTERFACE_MODO] = 1; layout_modo_definir(1); ajustes_dir(dir);
  CONFERE(valor[AJ_INTERFACE_MODO] == 2 && layout_modo_mobile() == ajustes_interface_mobile(),
          "Automatico sobrevive a nova leitura e reaplica a politica do aparelho");
}
#endif

// #205: the footer hint chips ("OK Confirmar", "Voltar Cancelar") are tappable
// in TV mode too. The taps go through ponteiro + ajustes_evento and confirm
// or cancel the pending edit, exactly like the remote keys. The frames run
// the REAL ajustes_atualizar + ajustes_desenhar in a hidden GL window, so the
// pointer layer the production code leaves behind is what the taps hit —
// the hand-built frame of the first version missed the Mobile regression.
static SDL_Window *janRodape;
static SDL_GLContext glRodape;
static void quadroRodape(void) {
  ponteiro_teste_janela((int)NV_TELA_W, (int)NV_TELA_H);
  ponteiro_quadro(relogio);
  ponteiro_camada();
  // Como na tela assentada: as linhas ja registraram alvos, entao um clique
  // fora dos chips NAO pode cair no OK de camada vazia.
  ponteiro_alvo(0, 0, (float)NV_TELA_W, (float)NV_TELA_H, NULL, NULL, 0, 0);
  // O rodape e medido na tela virtual dos Ajustes, como no desenho de verdade.
  { AjDica d[3] = { { "← →", "Escolher" }, { "OK", "Confirmar" }, { "Voltar", "Cancelar" } };
    GfxRect fr = aj2ListaR();
    AJ_ESCALA_INI();
    aj2RodapeA3(fr, d, 3, 1.0f, 1);
    AJ_ESCALA_FIM(); }
  ponteiro_desenhar();
  relogio += 400;
}
static int alvoRodape(PonteiroFn focar, PonteiroFn ativar, float *x, float *y) {
  const PonteiroAlvo *v;
  int n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) {
    if (v[i].focar != focar || v[i].ativar != ativar) continue;
    *x = v[i].x + v[i].w * 0.5f; *y = v[i].y + v[i].h * 0.5f;
    return 1;
  }
  return 0;
}
#ifdef NV_TOUCH_UI
static void rodapePorToque(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char *antes, *depois;
  float x, y;
  valor[AJ_INTERFACE_MODO] = 0; interfaceAplicar();
  layout_tela_definir(1080, 2340);   // phone drawable, saved TV canvas
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  assert(!layout_modo_mobile());
  ajustes_dir(dir);
  valor[AJ_VIDRO] = 0; assert(gravar());
  uxAviso[0] = 0;

  // Inline editing of a switch row (TV mode): the footer keeps the TV hint chips.
  focarOpcao(AJ_VIDRO); uxAbrirEditor(AJ_VIDRO);
  CONFERE(uxEditor == 1 && ajEditandoLinha(AJ_VIDRO), "Vidro: edita na linha, chips no rodape");
  uxEvento(SDLK_RIGHT); assert(uxPendente == 1 && valor[AJ_VIDRO] == 0);
  quadroRodape();
  CONFERE(alvoRodape(aj2DicaOk, NULL, &x, &y), "Rodape TV: o chip OK registra alvo");
  antes = arquivoAjustes();
  if (alvoRodape(aj2DicaOk, NULL, &x, &y)) clicarInterface(x, y);   // tap on the OK chip = OK on the focused row
  CONFERE(!uxEditor && valor[AJ_VIDRO] == 1, "Toque no chip OK confirma a edicao");
  depois = arquivoAjustes();
  CONFERE(strcmp(antes, depois) != 0 && strstr(depois, "vidroLocal") != NULL, "Chip OK grava o valor pendente");
  free(antes); free(depois);

  // The Back chip cancels without touching the saved value.
  if (valor[AJ_VIDRO] != 1) { valor[AJ_VIDRO] = 1; assert(gravar()); }   // estado da confirmacao acima
  focarOpcao(AJ_VIDRO); uxAbrirEditor(AJ_VIDRO); assert(uxEditor == 1);
  uxEvento(SDLK_LEFT); assert(uxPendente == 0);
  uxAviso[0] = 0;   // o "Ajuste salvo" anterior nao pode comer o rodape
  quadroRodape();
  CONFERE(alvoRodape(NULL, aj2DicaVoltar, &x, &y), "Rodape TV: o chip Voltar registra alvo");
  antes = arquivoAjustes();
  if (alvoRodape(NULL, aj2DicaVoltar, &x, &y)) clicarInterface(x, y);
  CONFERE(!uxEditor && valor[AJ_VIDRO] == 1, "Toque no chip Voltar cancela");
  depois = arquivoAjustes();
  CONFERE(!strcmp(antes, depois), "Chip Voltar nao grava nada");
  free(antes); free(depois);

  // The same OK chip by mouse click (Magic Remote / desktop).
  valor[AJ_VIDRO] = 0; assert(gravar());
  focarOpcao(AJ_VIDRO); uxAbrirEditor(AJ_VIDRO);
  uxEvento(SDLK_RIGHT); assert(uxPendente == 1 && valor[AJ_VIDRO] == 0);
  uxAviso[0] = 0;
  quadroRodape();
  CONFERE(alvoRodape(aj2DicaOk, NULL, &x, &y), "Rodape TV: o chip OK registra alvo (mouse)");
  if (alvoRodape(aj2DicaOk, NULL, &x, &y)) clicarMouseInterface(x, y);
  CONFERE(!uxEditor && valor[AJ_VIDRO] == 1, "Clique do mouse no chip OK confirma a edicao");
  valor[AJ_VIDRO] = 0; gravar();
}

#endif
#ifndef NV_TOUCH_UI
// P1: the same footer replay runs on a non-touch desktop/TV build — a click
// on the Voltar chip cancels the pending edit instead of confirming it.
static void rodapePorClique(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char *antes;
  float x, y;
  ajustes_dir(dir);
  valor[AJ_VIDRO] = 0; assert(gravar());
  ponteiro_iniciar();
  focarOpcao(AJ_VIDRO); uxAbrirEditor(AJ_VIDRO);
  CONFERE(uxEditor == 1 && ajEditandoLinha(AJ_VIDRO), "Vidro: edita na linha");
  uxEvento(SDLK_RIGHT); assert(uxPendente == 1 && valor[AJ_VIDRO] == 0);
  uxAviso[0] = 0;
  acordarInterface();
  quadroRodape();
  CONFERE(alvoRodape(NULL, aj2DicaVoltar, &x, &y), "Rodape desktop: o chip Voltar registra alvo");
  antes = arquivoAjustes();
  if (alvoRodape(NULL, aj2DicaVoltar, &x, &y)) clicarMouseInterface(x, y);
  CONFERE(!uxEditor && valor[AJ_VIDRO] == 0, "Clique no chip Voltar cancela");
  CONFERE(!strcmp(antes, arquivoAjustes()), "Chip Voltar nao grava nada");
  free(antes);
  uxCancelar();

  // P2: the saved notice fills the footer width — the footer hides the chips,
  // so the replay must leave no OK/Voltar target behind the notice text.
  snprintf(uxAviso, sizeof uxAviso, "%s",
           "Ajuste salvo nesta TV. Este aviso aqui e comprido de proposito, para nao caber na faixa do rodape ao lado das dicas do controle, e assim empurrar os chips para fora");
  uxAvisoAte = relogio + 4500;
  valor[AJ_TAMANHO_AJUSTES] = 2;   // 100%: a faixa do rodape encolhe e o aviso empurra os chips
  focarOpcao(AJ_VIDRO); uxAbrirEditor(AJ_VIDRO);
  CONFERE(uxEditor == 1 && ajEditandoLinha(AJ_VIDRO), "Aviso: Vidro abre na linha");
  for (int i = 0; i < 6; i++) quadroRodape();   // o rodape por inteiro rasteriza o aviso
  { const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v), chips = 0;
    for (int i = 0; i < n; i++)
      if (v[i].focar == aj2DicaOk || v[i].ativar == aj2DicaVoltar) chips++;
    CONFERE(chips == 0, "Rodape desktop: o aviso esconde os chips e nao sobra alvo invisivel"); }
  uxCancelar(); uxAviso[0] = 0;
}
#endif

int main(void) {
#ifdef NV_TOUCH_UI
  Aj2Pose p = {0};
  p.lA = 1.0f; p.lS = 1.0f;
  uxIndice = 2; uxEditor = 1; uxOp = AJ_TEMA; uxRestaurar = 0; uxAvisoRisco = 0;
  assert(ajEditandoLinha(AJ_TEMA));
  layout_modo_definir(0);
  assert(!ajInlineAtivo(0, &p));
  layout_modo_definir(1);
  assert(ajInlineAtivo(0, &p));
  assert(!ajInlineAtivo(1, &p));   // a modal still wins in Mobile
  puts("PASS: inline editor is Mobile only");
#endif
  // A hidden GL window, as in tests/ajustes_shot.c: the footer frames below run
  // the real ajustes_desenhar, and its background needs a GL context. txt first,
  // before anything measures a line (text.c only tries each family once).
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janRodape = SDL_CreateWindow("Nuvio: rodape dos Ajustes", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, 1920, 1080,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janRodape);
  glRodape = SDL_GL_CreateContext(janRodape); assert(glRodape);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  dados_iniciar(getenv("NUVIO_DADOS"));
  gfx_opacidade_grupo = 0;  // exercise registration and hint measurement, skip pixels
  ajustes_iniciar();
  layout_tela_definir(1920, 1080);
  ponteiro_teste_relogio(agora); ponteiro_teste_janela(1920, 1080);
#ifdef NV_TOUCH_UI
  fileiras(0); fileiras(1);
  if (!falhas) puts("PASS: Fileiras targets, callbacks and hints are Mobile only");
  interfacePorToque();
  if (!falhas) puts("PASS: interface selector confirms and saves TV/Mobile by touch");
  rodapePorToque();
  if (!falhas) puts("PASS: footer hint chips confirm and cancel by touch");
#else
  rodapePorClique();
  if (!falhas) puts("PASS: footer hint chips cancel by click and the saved notice hides them");
#endif
  return falhas ? 1 : 0;
}
