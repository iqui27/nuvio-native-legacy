// Ponteiro do Magic Remote: estado, hit-test e cursor. Ver ponteiro.h.
#include "ponteiro.h"
#include "gfx.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#if (!defined(__APPLE__) && !defined(NV_LINUX_DESKTOP) && !defined(__EMSCRIPTEN__) && !defined(NV_TPK) && !defined(NV_ANDROID)) || \
    defined(NV_PONT_WEBOS_TESTE)
#include <dlfcn.h>
#define NV_PONT_WEBOS 1
#endif

// Scancodes sinteticos do SDL_webOS.h (SDK openlgtv). Transcritos porque o
// header do SDK nao e incluido no build do Mac nem no da Samsung.
#define PONT_SC_CURSOR_SHOW 484
#define PONT_SC_CURSOR_HIDE 485

#define PONT_MAX_ALVOS   512
// Parado este tempo, o cursor some — no webOS o do sistema junto, pedido por
// SDL_webOSCursorVisibility (ver ponteiro_quadro). O SDL_webOS.h tambem tem o
// hint SDL_WEBOS_CURSOR_SLEEP_TIME, mas sem unidade documentada e sem medida
// na TV; nao e usado.
#define PONT_DORME_MS    4000
// Janela em que um OK de tecla e um clique sao O MESMO aperto. Nao esta
// provado se o webOS manda os dois quando o cursor esta na tela; se mandar,
// sem isto o OK valeria duas vezes (abrir e ja reproduzir).
#define PONT_DEDUPE_MS   150
// Quanto o alvo sob o cursor tem de ficar parado antes de o hover poder trocar
// o foco. Ver `conteudoMexeuEm`.
#define PONT_ASSENTA_MS  150
#define PONT_RODA_MS      70
// DEPOIS DE UMA SETA o cursor so volta com um gesto de verdade. Apertar a seta
// balanca o Magic Remote; esse tremor chegava como movimento, reacendia o
// cursor e o hover refocava o alvo sob ele (no player, o de tela cheia, que
// reabre a barra). Movimento nos primeiros PONT_SETA_JANELA_MS e ignorado; depois
// dele o cursor precisa se afastar PONT_SETA_LIMIAR px (logicos, em linha reta)
// do ponto onde a mao voltou a mexer. Uma pausa maior que PONT_SETA_PAUSA_MS
// recomeca a conta: tremor esparso nao soma.
#define PONT_SETA_JANELA_MS 350
#define PONT_SETA_LIMIAR    32.0f
#define PONT_SETA_PAUSA_MS  300
#define PONT_TOQUE_LIMIAR   32.0f  // deslocamento logico maximo de um tap
#define PONT_DEDOS_MAX      16

static PonteiroAlvo lista[2][PONT_MAX_ALVOS];
static int nLista[2];
static PonteiroRolagemFn listaRolagem[2];
#ifdef NV_TOUCH_UI
typedef struct { float largura; PonteiroBordaFn ativar; } Borda;
static Borda listaBorda[2], toqueBorda;
static int toqueQuadroPronto;
#define PONT_BORDA_PUXAR 72.0f
#endif
static int escreve = 0;          // a que o desenho deste quadro preenche
static int pronto  = 1;          // a do quadro anterior, que o hit-test le
static int quadroSessaoPendente; // descarta entrada ate a nova tela publicar alvos

static float px = NV_TELA_BASE_W * 0.5f, py = NV_TELA_BASE_H * 0.5f;
static int visivel = 0;
static Uint32 ultimoMov = 0;
// The last TOUCH is activity too: see ponteiro_last_motion.
static Uint32 toqueUltimo = 0;
static int janelaW = 0, janelaH = 0;
static Uint32 (*relogio)(void) = NULL;

// Identidade do alvo que o hover focou por ultimo. Um alvo e "o mesmo" entre
// quadros pelas funcoes e pelos dois inteiros — o retangulo muda (rolagem,
// escala de foco) e nao serve de chave.
typedef struct { PonteiroFn focar, ativar; int a, b; int ok; float cx, cy; } Ident;
static Ident hover;
// O CONTEUDO ANDA SOB O CURSOR PARADO. Focar uma fileira da home rola a pagina
// para ela; o card seguinte sobe para debaixo do cursor e, no proximo
// micro-movimento da mao, seria focado — que rola de novo. Sem freio a pagina
// desce sozinha. Enquanto o alvo focado pelo hover estiver se mexendo na tela
// (rolagem, expansao), o hover nao troca de alvo.
static Uint32 conteudoMexeuEm = 0;

static int okPendente = 0;          // KEYDOWN RETURN entregue, falta o KEYUP
static int voltarPendente = 0;
static Ident ativarPendente;
static Uint32 cliqueEm = 0, okTeclaEm = 0;
static int engolirCliqueSolto = 0, engolirOkSolto = 0;
static Uint32 rodaEm = 0;

static int logouTipo[8];

// Escondido por uma seta: o movimento tem de vencer a janela e o limiar.
static int escondidoSeta = 0;
static Uint32 setaEm = 0, setaMovEm = 0;
static float setaAx, setaAy;
static int setaAncora = 0;
// Nos que pedimos ao compositor para esconder a seta dele (webOS).
static int sistemaEscondido = 0;

#ifdef NV_ANDROID
static int toqueDisponivel = 1;
#else
static int toqueDisponivel;
#endif
typedef struct { SDL_TouchID toque; SDL_FingerID dedo; } Dedo;
static Dedo dedos[PONT_DEDOS_MAX];
static int nDedos, dedosExcedentes, toqueCancelado;
static float toqueX, toqueY;
static Ident toqueAlvo;
static int toqueSemAlvos;
static Uint32 toqueDesde;
static float toqueUltX, toqueUltY;
static int toqueLongo, okLongoToque;
static int toqueParouInercia;
static PonteiroRolagemFn toqueRolagem;

static void pararInercia(void);
static void pararRolagem(void);
static int arrModo;
static void cancelarToque(void) {
  pararRolagem();
  pararInercia();
  arrModo = 0;
  nDedos = dedosExcedentes = 0;
  toqueCancelado = 1;
  toqueAlvo.ok = 0;
  toqueLongo = 0;
#ifdef NV_TOUCH_UI
  toqueBorda = (Borda){0};
#endif
}

#ifdef NV_PONT_WEBOS
static SDL_bool (*cursorSistema)(SDL_bool) = NULL;
#endif

static Uint32 agoraMs(void) { return relogio ? relogio() : SDL_GetTicks(); }

void ponteiro_teste_relogio(Uint32 (*fn)(void)) { relogio = fn; }
void ponteiro_teste_janela(int w, int h) { janelaW = w; janelaH = h; }
void ponteiro_teste_toque(int ligado) { toqueDisponivel = ligado != 0; cancelarToque(); }
#ifdef NV_PONT_WEBOS
void ponteiro_teste_cursor_sistema(SDL_bool (*fn)(SDL_bool)) { cursorSistema = fn; }
#endif

// Costura de teste, so leitura, INTENCIONAL no release: nenhum codigo de src/ a chama (so tests/menuep.c, ponteiro_telas.c).
int ponteiro_teste_lista(const PonteiroAlvo **v) { if (v) *v = lista[pronto]; return nLista[pronto]; }

float ponteiro_x(void) { return px; }
float ponteiro_y(void) { return py; }
int ponteiro_ativo(void) { return visivel || toqueDisponivel; }

// See ponteiro.h. A touch counts as activity too: a finger on the screen is also a
// hand on the remote, and without this the bar vanished under a resting finger.
Uint32 ponteiro_last_motion(void) {
  return toqueUltimo > ultimoMov ? toqueUltimo : ultimoMov;
}

// DIAGNOSTICO DE ENTRADA (#99, segunda volta). Na C9 chegaram os avisos
// 484/485 e a rodinha, e NENHUM movimento nem clique. Para saber o que o SDL
// da LG entrega de fato, todo evento que nao e tecla sai no log por 30 s
// depois do arranque e por 30 s depois de cada 484 — com teto, para nao afogar.
static Uint32 diagAte = 30000;
static int diagN;
static const char *nomeTipo(Uint32 t) {
  switch (t) {
    case SDL_MOUSEMOTION: return "MOUSEMOTION";
    case SDL_MOUSEBUTTONDOWN: return "MOUSEBUTTONDOWN";
    case SDL_MOUSEBUTTONUP: return "MOUSEBUTTONUP";
    case SDL_MOUSEWHEEL: return "MOUSEWHEEL";
    case SDL_FINGERDOWN: return "FINGERDOWN";
    case SDL_FINGERUP: return "FINGERUP";
    case SDL_FINGERMOTION: return "FINGERMOTION";
    case SDL_WINDOWEVENT: return "WINDOWEVENT";
    case SDL_SYSWMEVENT: return "SYSWMEVENT";
    case SDL_TEXTINPUT: return "TEXTINPUT";
    case SDL_TEXTEDITING: return "TEXTEDITING";
    case SDL_KEYDOWN: return "KEYDOWN";
    case SDL_KEYUP: return "KEYUP";
    default: return "?";
  }
}
void ponteiro_diag(const SDL_Event *e) {
  Uint32 agora = SDL_GetTicks();
  if (e->type == SDL_KEYDOWN &&
      (int)e->key.keysym.scancode == PONT_SC_CURSOR_SHOW) diagAte = agora + 30000;
  if (agora > diagAte || diagN >= 400) return;
  if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
    int sc = (int)e->key.keysym.scancode;
    if (sc != PONT_SC_CURSOR_SHOW && sc != PONT_SC_CURSOR_HIDE &&
        e->key.keysym.sym != SDLK_RETURN) return;
  }
  if (e->type == SDL_MOUSEMOTION) {
    static int nMov;
    if (++nMov > 30) return;
  }
  diagN++;
  switch (e->type) {
    case SDL_MOUSEMOTION:
      printf("[ponteiro-diag] t=%u MOUSEMOTION win=%u which=%u x=%d y=%d rel=%d,%d estado=%u\n",
             agora, e->motion.windowID, e->motion.which, e->motion.x, e->motion.y,
             e->motion.xrel, e->motion.yrel, e->motion.state); break;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
      printf("[ponteiro-diag] t=%u %s win=%u which=%u botao=%d x=%d y=%d\n",
             agora, nomeTipo(e->type), e->button.windowID, e->button.which,
             e->button.button, e->button.x, e->button.y); break;
    case SDL_MOUSEWHEEL: {
      int mx = -1, my = -1; Uint32 b = SDL_GetMouseState(&mx, &my);
      SDL_Window *f = SDL_GetMouseFocus();
      printf("[ponteiro-diag] t=%u MOUSEWHEEL win=%u which=%u x=%d y=%d estado_mouse=%d,%d botoes=%u foco=%p\n",
             agora, e->wheel.windowID, e->wheel.which, e->wheel.x, e->wheel.y,
             mx, my, b, (void *)f); break; }
    case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION:
      printf("[ponteiro-diag] t=%u %s x=%.3f y=%.3f\n", agora, nomeTipo(e->type),
             e->tfinger.x, e->tfinger.y); break;
    case SDL_WINDOWEVENT:
      printf("[ponteiro-diag] t=%u WINDOWEVENT sub=%d d1=%d d2=%d\n", agora,
             e->window.event, e->window.data1, e->window.data2); break;
    case SDL_KEYDOWN: case SDL_KEYUP: {
      int mx = -1, my = -1; SDL_GetMouseState(&mx, &my);
      printf("[ponteiro-diag] t=%u %s sc=%d sym=%d estado_mouse=%d,%d\n", agora,
             nomeTipo(e->type), (int)e->key.keysym.scancode, (int)e->key.keysym.sym, mx, my);
      break; }
    default:
      printf("[ponteiro-diag] t=%u tipo=0x%x (%s)\n", agora, e->type, nomeTipo(e->type));
  }
  fflush(stdout);
}

void ponteiro_iniciar(void) {
  cancelarToque();
  quadroSessaoPendente = 0;
  nLista[0] = nLista[1] = 0;
  listaRolagem[0] = listaRolagem[1] = NULL;
#ifdef NV_TOUCH_UI
  listaBorda[0] = listaBorda[1] = (Borda){0};
  toqueQuadroPronto = 0;
#endif
  visivel = 0;
  escondidoSeta = 0; sistemaEscondido = 0;
  memset(&hover, 0, sizeof hover);
#ifndef NV_ANDROID
  toqueDisponivel = SDL_GetNumTouchDevices() > 0;
#endif
  { SDL_Window *w = SDL_GL_GetCurrentWindow();
    int ww = 0, wh = 0, dw = 0, dh = 0;
    if (w) { SDL_GetWindowSize(w, &ww, &wh); SDL_GL_GetDrawableSize(w, &dw, &dh); }
    printf("[ponteiro] janela=%dx%d drawable=%dx%d toque=%d\n", ww, wh, dw, dh,
           SDL_GetNumTouchDevices()); }
#ifdef NV_PONT_WEBOS
  // dlopen(NULL) = o proprio processo: o SDL ja esta carregado, o que se quer
  // saber e se ESTA firmware o exporta. (RTLD_DEFAULT pediria _GNU_SOURCE.)
  { void *eu = dlopen(NULL, RTLD_NOW);
    if (eu) *(void **)(&cursorSistema) = dlsym(eu, "SDL_webOSCursorVisibility"); }
  printf("[ponteiro] ShowCursor=%d\n", SDL_ShowCursor(SDL_QUERY));
  printf("[ponteiro] SDL_webOSCursorVisibility: %s\n",
         cursorSistema ? "presente" : "ausente");
  { SDL_bool (*painel)(int *, int *) = NULL;
    void *eu = dlopen(NULL, RTLD_NOW);
    int pw = 0, ph = 0;
    if (eu) *(void **)(&painel) = dlsym(eu, "SDL_webOSGetPanelResolution");
    if (painel) painel(&pw, &ph);
    printf("[ponteiro] painel=%dx%d\n", pw, ph); }
  fflush(stdout);
#endif
}

static void esconder(const char *porque) {
  if (!visivel) return;
  visivel = 0;
  hover.ok = 0;
  (void)porque;
#ifdef NV_PONT_WEBOS
  // Mesmo gesto do RetroArch: seta apertada, cursor do sistema fora tambem.
  // Vale para o "parado" tambem: sem isto a seta do sistema ficaria na tela
  // com o hover ja desligado.
  if (cursorSistema) { cursorSistema(SDL_FALSE); sistemaEscondido = 1; }
#endif
}

// O cursor volta (movimento que venceu o limiar, clique, 484 legitimo).
static void reaparecer(void) {
  escondidoSeta = 0;
  if (!visivel) { visivel = 1; hover.ok = 0; }
#ifdef NV_PONT_WEBOS
  // Fomos nos que escondemos a seta do sistema: devolve-la. (Se o compositor ja
  // a mostrou sozinho, pedir de novo nao muda nada.)
  if (sistemaEscondido && cursorSistema) cursorSistema(SDL_TRUE);
#endif
  sistemaEscondido = 0;
}

// Movimento enquanto escondido pela seta: ainda e tremor?
static int tremorDaSeta(Uint32 agora) {
  if (!escondidoSeta) return 0;
  if (agora - setaEm < PONT_SETA_JANELA_MS) return 1;
  if (!setaAncora || agora - setaMovEm > PONT_SETA_PAUSA_MS) {
    setaAncora = 1; setaAx = px; setaAy = py;
  }
  setaMovEm = agora;
  { float dx = px - setaAx, dy = py - setaAy;
    return dx * dx + dy * dy < PONT_SETA_LIMIAR * PONT_SETA_LIMIAR; }
}

static void primeiro(int tipo, const char *nome, int x, int y) {
  if (tipo < 0 || tipo >= 8 || logouTipo[tipo]) return;
  logouTipo[tipo] = 1;
  printf("[ponteiro] primeiro evento tipo=%s x=%d y=%d -> logico %.0f,%.0f\n",
         nome, x, y, px, py);
  fflush(stdout);
}

// Janela -> logico. O layout e SEMPRE 1920x1080; a janela no Mac pode ser
// menor que isso (tela pequena) e na TV e 1920x1080 mesmo.
static void converter(Uint32 janelaId, int x, int y) {
  int w = janelaW, h = janelaH;
  if (w <= 0 || h <= 0) {
    SDL_Window *win = SDL_GetWindowFromID(janelaId);
    if (!win) win = SDL_GetMouseFocus();
    if (win) SDL_GetWindowSize(win, &w, &h);
    if (w <= 0 || h <= 0) { w = (int)NV_TELA_W; h = (int)NV_TELA_H; }
  }
  px = (float)x * NV_TELA_W / (float)w;
  py = (float)y * NV_TELA_H / (float)h;
  if (px < 0) px = 0;
  if (px > NV_TELA_W - 1) px = NV_TELA_W - 1;
  if (py < 0) py = 0;
  if (py > NV_TELA_H - 1) py = NV_TELA_H - 1;
}

int ponteiro_achar(const PonteiroAlvo *v, int n, float x, float y) {
  for (int i = n - 1; i >= 0; i--)
    if (x >= v[i].x && x < v[i].x + v[i].w && y >= v[i].y && y < v[i].y + v[i].h)
      return i;
  return -1;
}

static int mesmo(const Ident *id, const PonteiroAlvo *al) {
  return id->ok && id->focar == al->focar && id->ativar == al->ativar &&
         id->a == al->a && id->b == al->b;
}
static void guardar(Ident *id, const PonteiroAlvo *al) {
  id->ok = 1; id->focar = al->focar; id->ativar = al->ativar;
  id->a = al->a; id->b = al->b;
  id->cx = al->x + al->w * 0.5f; id->cy = al->y + al->h * 0.5f;
}

static void tecla(void (*entregar)(const SDL_Event *), Uint32 tipo, SDL_Keycode k) {
  if (!entregar) return; // um callback pode ter encerrado a abertura no KEYDOWN
  SDL_Event t; SDL_zero(t);
  t.type = tipo; t.key.keysym.sym = k;
  t.key.state = tipo == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  entregar(&t);
}

// Rastro para conferir na TV (/tmp/nuvio.log) sem afogar o log: as primeiras
// trocas de foco e cliques da sessao, com o alvo e quantos havia na lista.
static int nRastro;
static void rastro(const char *o_que, const PonteiroAlvo *al, int n) {
  if (nRastro >= 40) return;
  nRastro++;
  if (al) printf("[ponteiro] %s a=%d b=%d em %.0f,%.0f (alvo %.0f,%.0f %.0fx%.0f; %d alvos)\n",
                 o_que, al->a, al->b, px, py, al->x, al->y, al->w, al->h, n);
  else    printf("[ponteiro] %s sem alvo em %.0f,%.0f (%d alvos)\n", o_que, px, py, n);
  fflush(stdout);
}

static void mover(void) {
  const PonteiroAlvo *v = lista[pronto];
  int i = ponteiro_achar(v, nLista[pronto], px, py);
  if (i < 0 || mesmo(&hover, &v[i])) return;
  if (conteudoMexeuEm && agoraMs() - conteudoMexeuEm < PONT_ASSENTA_MS) return;
  guardar(&hover, &v[i]);
  if (v[i].focar) { rastro("foco", &v[i], nLista[pronto]); v[i].focar(v[i].a, v[i].b); }
}

static int dedoIndice(const SDL_TouchFingerEvent *e) {
  for (int i = 0; i < nDedos; i++)
    if (dedos[i].toque == e->touchId && dedos[i].dedo == e->fingerId) return i;
  return -1;
}

static int converterToque(const SDL_TouchFingerEvent *e) {
  if (!isfinite(e->x) || !isfinite(e->y)) return 0;
  // SDL ja normalizou pela janela. O viewport ocupa a superficie inteira;
  // DPI e janela menor nao mudam a coordenada no layout 1920x1080.
  px = fminf(fmaxf(e->x, 0.0f) * NV_TELA_W, NV_TELA_W - 1.0f);
  py = fminf(fmaxf(e->y, 0.0f) * NV_TELA_H, NV_TELA_H - 1.0f);
  return 1;
}

// ARRASTAR (#216, segunda volta). Um dedo que passa do limiar deixa de ser
// toque e vira um destes dois gestos:
//   - ROLAGEM: o eixo e decidido no limiar (o maior deslocamento) e cada
//     PONT_PASSO_* px logicos andados vira UMA seta, como a rodinha — a tela
//     rola pelo mesmo caminho das setas, sem saber que houve dedo. Sentido
//     "natural" do celular: dedo para cima = conteudo sobe = seta para BAIXO.
//     Soltar com velocidade continua andando (inercia), freando sozinho.
//     Uma camada com ponteiro_rolagem pode capturar: recebe os trechos do
//     dedo e a inercia diretamente, sem converter distancia em teclas.
//   - ARRASTO DE ALVO: alvo marcado por ponteiro_alvo_arrastavel (a barra de
//     tempo do player) recebe o `ativar` a cada movimento, com ponteiro_x()
//     atualizado e ponteiro_toque() = 1. Nao rola nada.
#define PONT_PASSO_V      150.0f   // px logicos por seta, vertical
#define PONT_PASSO_H      210.0f   // px logicos por seta, horizontal
#define PONT_INERCIA_MIN  0.45f    // px/ms para a soltura ganhar inercia
#define PONT_INERCIA_PARA 0.06f    // px/ms abaixo disto a inercia acaba
#define PONT_INERCIA_TAU  260.0f   // ms: constante do freio exponencial
#define PONT_INERCIA_MAXP 2        // setas por quadro, no maximo
enum { ARR_NADA = 0, ARR_ROLA, ARR_ALVO, ARR_CONTINUO, ARR_CONSUMIDO, ARR_BORDA };
static int arrEixoY;
static float arrAcum, arrUltX, arrUltY, arrVel;
static Uint32 arrUltMs;
static PonteiroAlvo arrAlvo;
static int inercia;
static float inVel, inAcum;
static int inEixoY;
static Uint32 inUltMs;
static PonteiroRolagemFn arrRolagem, inRolagem;
static float inX, inY;
static void (*entregarToque)(const SDL_Event *);
static int porToque;   // 1 durante focar/ativar disparados por dedo

int ponteiro_toque(void) { return porToque; }
int ponteiro_tem_toque(void) { return toqueDisponivel; }
int ponteiro_ok_longo(void) { return okLongoToque; }

static void alvoPorToque(const PonteiroAlvo *al, int focar, int ativar) {
  porToque = 1;
  if (focar && al->focar) al->focar(al->a, al->b);
  if (ativar && al->ativar) al->ativar(al->a, al->b);
  porToque = 0;
}

// Anda o acumulado em setas. Positivo = o dedo andou para a direita/baixo.
static float rolarPassos(float acum, int eixoY, int teto) {
  float passo = eixoY ? PONT_PASSO_V : PONT_PASSO_H;
  int n = 0;
  while ((acum >= passo || acum <= -passo) && (!teto || n < teto)) {
    SDL_Keycode k = eixoY ? (acum > 0 ? SDLK_UP : SDLK_DOWN)
                          : (acum > 0 ? SDLK_LEFT : SDLK_RIGHT);
    acum += acum > 0 ? -passo : passo;
    if (entregarToque) {
      tecla(entregarToque, SDL_KEYDOWN, k);
      tecla(entregarToque, SDL_KEYUP, k);
    }
    n++;
  }
  return acum;
}

static int rolagemEntregar(PonteiroRolagemFn fn, int fase, int eixoY,
                           float delta, float velocidade, float x, float y) {
  PonteiroRolagem e = { fase, eixoY, delta, velocidade, x, y };
  int salvo = porToque, r;
  if (!fn) return 0;
  porToque = 1; r = fn(&e); porToque = salvo;
  return r;
}

static void encerrarInercia(int fase) {
  PonteiroRolagemFn fn = inRolagem;
  inRolagem = NULL;
  inercia = 0; inVel = inAcum = 0.0f;
  rolagemEntregar(fn, fase, inEixoY, 0.0f, 0.0f, inX, inY);
}
static void pararInercia(void) { encerrarInercia(PONT_ROL_CANCELAR); }
static void pararRolagem(void) {
  PonteiroRolagemFn fn = arrRolagem;
  arrRolagem = NULL;
  rolagemEntregar(fn, PONT_ROL_CANCELAR, arrEixoY, 0.0f, 0.0f, toqueX, toqueY);
}

static void invalidarToque(void) {
  if (inRolagem) pararInercia();
  if (!nDedos || toqueLongo) return;
  toqueCancelado = 1; pararRolagem(); arrModo = ARR_NADA; pararInercia();
}

void ponteiro_cancelar_toque(void) { cancelarToque(); }

void ponteiro_abrir_sessao(void) {
  // CANCELAR ainda vai para a tela que sai. Ela continua dona do gesto aqui;
  // so depois o app limpa seus offsets e troca a identidade do perfil.
  cancelarToque();
  memset(dedos, 0, sizeof dedos);
  toqueAlvo = hover = ativarPendente = (Ident){0};
  toqueRolagem = NULL; toqueSemAlvos = 0;
  toqueX = toqueY = toqueUltX = toqueUltY = 0.0f; toqueDesde = 0;
  toqueParouInercia = okLongoToque = 0;
  arrEixoY = 0; arrAcum = arrUltX = arrUltY = arrVel = 0.0f; arrUltMs = 0;
  memset(&arrAlvo, 0, sizeof arrAlvo);
  inVel = inAcum = inX = inY = 0.0f; inUltMs = 0; inEixoY = 0;
  entregarToque = NULL;
  okPendente = voltarPendente = engolirCliqueSolto = engolirOkSolto = 0;
  cliqueEm = okTeclaEm = rodaEm = conteudoMexeuEm = 0;
  escondidoSeta = setaAncora = 0; setaEm = setaMovEm = 0;
  nLista[0] = nLista[1] = 0;
  memset(lista, 0, sizeof lista);
  listaRolagem[0] = listaRolagem[1] = NULL;
  quadroSessaoPendente = 1;
#ifdef NV_TOUCH_UI
  listaBorda[0] = listaBorda[1] = toqueBorda = (Borda){0};
  toqueQuadroPronto = 0;
#endif
  // Nao toca toqueDisponivel, janelaW/H, relogio nem a API do cursor do
  // compositor: isto e uma fronteira de entrada, nao um novo dispositivo.
}

int ponteiro_cancelar_alvo(PonteiroFn focar) {
  if (!focar || nDedos != 1 || toqueCancelado || toqueLongo ||
      arrModo != ARR_NADA || !toqueAlvo.ok || toqueAlvo.focar != focar) return 0;
  cancelarToque();
  return 1;
}

// Ate o limiar nenhum OK foi entregue: arrastar, outro dedo ou uma camada
// nova podem cancelar sem transformar a soltura num clique. No limiar o
// gesto fica consumido e entrega um par completo pela mesma rota do teclado.
static void longoToque(Uint32 agora) {
  int i;
  PonteiroAlvo al;
  if (nDedos != 1 || dedosExcedentes || toqueCancelado || toqueLongo || toqueParouInercia ||
      arrModo != ARR_NADA || !entregarToque) return;
  i = ponteiro_achar(lista[pronto], nLista[pronto], toqueUltX, toqueUltY);
  if (i >= 0 && mesmo(&toqueAlvo, &lista[pronto][i])) {
    al = lista[pronto][i];
    if (!al.segurar && (!al.focar || al.ativar || al.arrasta)) return;
  } else if (toqueSemAlvos && nLista[pronto] == 0) {
    memset(&al, 0, sizeof al);
  } else {
    // Sair um pouco do retangulo ainda pode virar arrasto. So uma identidade
    // que sumiu da camada cancela o gesto; iniciar no vazio tambem pode rolar.
    if (toqueAlvo.ok) {
      for (i = 0; i < nLista[pronto]; i++)
        if (mesmo(&toqueAlvo, &lista[pronto][i])) return;
      toqueCancelado = 1;
    } else if (toqueSemAlvos) toqueCancelado = 1;
    return;
  }
  if (agora - toqueDesde < NV_HOLD_MS) return;
  toqueLongo = 1;
  px = toqueUltX; py = toqueUltY;
  if (al.segurar) { porToque = 1; al.segurar(al.a, al.b); porToque = 0; }
  else alvoPorToque(&al, 1, 0);
  okLongoToque = 1;
  tecla(entregarToque, SDL_KEYDOWN, SDLK_RETURN);
  tecla(entregarToque, SDL_KEYUP, SDLK_RETURN);
  okLongoToque = 0;
}

static int eventoToque(const SDL_Event *e, void (*entregar)(const SDL_Event *)) {
  const SDL_TouchFingerEvent *t = &e->tfinger;
  int dedo = dedoIndice(t);
  Uint32 agora = agoraMs();
  toqueDisponivel = 1;
  toqueUltimo = agora ? agora : 1;
  entregarToque = entregar;
  if (e->type == SDL_FINGERDOWN) {
    if (dedo >= 0) return 1;
    // Dedo novo segura a lista que ainda corria: e o gesto de parar a rolagem.
    { int parou = inercia && inRolagem;
      pararInercia();
      if (!nDedos) toqueParouInercia = parou; }
    if (nDedos == PONT_DEDOS_MAX) { dedosExcedentes++; invalidarToque(); return 1; }
    dedos[nDedos++] = (Dedo){t->touchId, t->fingerId};
    if (nDedos > 1 || dedosExcedentes) { invalidarToque(); return 1; }
    toqueCancelado = !converterToque(t);
    toqueX = px; toqueY = py;
    toqueUltX = px; toqueUltY = py; toqueDesde = agora; toqueLongo = 0;
    arrUltX = px; arrUltY = py; arrUltMs = agora; arrVel = 0.0f; arrAcum = 0.0f;
    arrModo = ARR_NADA;
    toqueAlvo.ok = 0;
    toqueRolagem = listaRolagem[pronto];
#ifdef NV_TOUCH_UI
    toqueBorda = listaBorda[pronto];
    if (toqueX >= toqueBorda.largura) toqueBorda.ativar = NULL;
#endif
    toqueSemAlvos = nLista[pronto] == 0;
    if (!toqueCancelado) {
      int i = ponteiro_achar(lista[pronto], nLista[pronto], px, py);
      if (i >= 0) { guardar(&toqueAlvo, &lista[pronto][i]); arrAlvo = lista[pronto][i]; }
    }
    // Nao foca nem entrega OK no DOWN: arrastar nao pode abrir um titulo.
    return 1;
  }
  if (dedo < 0) {
    if (e->type == SDL_FINGERUP && dedosExcedentes) dedosExcedentes--;
    return 1;
  }
  if (!converterToque(t)) invalidarToque();
  else if (!toqueCancelado && !toqueLongo && nDedos == 1 && !dedosExcedentes) {
    toqueUltX = px; toqueUltY = py;
    float dx = px - toqueX, dy = py - toqueY;
    if (arrModo == ARR_NADA &&
        dx * dx + dy * dy > PONT_TOQUE_LIMIAR * PONT_TOQUE_LIMIAR) {
      if (toqueAlvo.ok && arrAlvo.arrasta) {
        arrModo = ARR_ALVO;
        alvoPorToque(&arrAlvo, 1, 0);
#ifdef NV_TOUCH_UI
      } else if (toqueBorda.ativar && toqueBorda.ativar == listaBorda[pronto].ativar &&
                 dx > fabsf(dy) * 1.5f) {
        arrModo = ARR_BORDA;
#endif
      } else {
#ifdef NV_TOUCH_UI
        // Mobile: with nothing scrollable here the drag stays consumed and
        // never becomes remote navigation or a release click. TV mode keeps
        // the drag-as-arrows behaviour.
        arrModo = layout_modo_mobile() ? ARR_CONSUMIDO : ARR_ROLA;
#else
        arrModo = ARR_ROLA;
#endif
        arrEixoY = fabsf(dy) >= fabsf(dx);
        arrAcum = 0.0f;
        arrUltX = toqueX; arrUltY = toqueY;
        if (toqueRolagem && toqueRolagem == listaRolagem[pronto]) {
          arrRolagem = toqueRolagem;
          if (rolagemEntregar(arrRolagem, PONT_ROL_INICIO, arrEixoY, 0.0f, 0.0f, toqueX, toqueY) &&
              !toqueCancelado && nDedos == 1) arrModo = ARR_CONTINUO;
          else arrRolagem = NULL;
        }
      }
    }
    if (arrModo == ARR_ROLA) {
      float d = arrEixoY ? py - arrUltY : px - arrUltX;
      Uint32 dt = agora - arrUltMs;
      arrAcum = rolarPassos(arrAcum + d, arrEixoY, 0);
      // Velocidade suavizada: o ultimo trecho pesa mais, mas um evento
      // isolado (dois no mesmo ms) nao vira um pico infinito.
      if (dt > 0) arrVel = 0.6f * (d / (float)dt) + 0.4f * arrVel;
      arrUltX = px; arrUltY = py; arrUltMs = agora;
    } else if (arrModo == ARR_CONTINUO) {
      float d = arrEixoY ? py - arrUltY : px - arrUltX;
      Uint32 dt = agora - arrUltMs;
      if (d != 0.0f) {
        if (dt > 0) arrVel = 0.6f * (d / (float)dt) + 0.4f * arrVel;
        arrUltX = px; arrUltY = py; arrUltMs = agora;
        rolagemEntregar(arrRolagem, PONT_ROL_MOVER, arrEixoY, d, arrVel, toqueX, toqueY);
      }
    } else if (arrModo == ARR_ALVO) {
      alvoPorToque(&arrAlvo, 0, 1);
#ifdef NV_TOUCH_UI
    } else if (arrModo == ARR_BORDA && dx >= PONT_BORDA_PUXAR && dx > fabsf(dy) * 1.5f) {
      PonteiroBordaFn fn = toqueBorda.ativar;
      arrModo = ARR_CONSUMIDO;
      if (fn && fn == listaBorda[pronto].ativar) {
        porToque = 1; fn(); porToque = 0;
      }
#endif
    }
    longoToque(agora);
  }
  if (e->type != SDL_FINGERUP) return 1;
  // Um segundo dedo cancela o gesto inteiro, mesmo se ele sair primeiro.
  if (nDedos == 1 && !dedosExcedentes && !toqueCancelado && !toqueLongo && arrModo == ARR_ROLA) {
    // Parado antes de soltar (mais de 90 ms sem andar) nao tem inercia.
    if (fabsf(arrVel) >= PONT_INERCIA_MIN && agora - arrUltMs < 90) {
      inercia = 1; inVel = arrVel; inAcum = arrAcum; inEixoY = arrEixoY; inUltMs = agora;
    }
  } else if (nDedos == 1 && !dedosExcedentes && !toqueCancelado && !toqueLongo && arrModo == ARR_CONTINUO) {
    PonteiroRolagemFn fn = arrRolagem;
    float vel = agora - arrUltMs < 90 ? arrVel : 0.0f;
    int continuar = fabsf(vel) >= PONT_INERCIA_MIN;
    arrRolagem = NULL;
    if (continuar) {
      inercia = 1; inRolagem = fn; inVel = vel; inEixoY = arrEixoY;
      inUltMs = agora; inX = toqueX; inY = toqueY;
    }
    rolagemEntregar(fn, PONT_ROL_SOLTAR, arrEixoY, 0.0f, vel, toqueX, toqueY);
    if (!continuar) rolagemEntregar(fn, PONT_ROL_FIM, arrEixoY, 0.0f, 0.0f, toqueX, toqueY);
  } else if (nDedos == 1 && !dedosExcedentes && !toqueCancelado && !toqueLongo &&
             !toqueParouInercia && arrModo == ARR_NADA) {
    int i = ponteiro_achar(lista[pronto], nLista[pronto], px, py);
    PonteiroAlvo al;
    if (i >= 0 && mesmo(&toqueAlvo, &lista[pronto][i])) {
      al = lista[pronto][i];
      rastro("toque", &al, nLista[pronto]);
      alvoPorToque(&al, 1, 1);
      if (!al.ativar && al.focar) {
        tecla(entregar, SDL_KEYDOWN, SDLK_RETURN);
        tecla(entregar, SDL_KEYUP, SDLK_RETURN);
      }
    } else if (toqueSemAlvos && nLista[pronto] == 0) {
      tecla(entregar, SDL_KEYDOWN, SDLK_RETURN);
      tecla(entregar, SDL_KEYUP, SDLK_RETURN);
    }
  }
  dedo = dedoIndice(t);  // o callback pode ter reiniciado a tela/entrada
  if (dedo < 0) return 1;
  memmove(dedos + dedo, dedos + dedo + 1, (size_t)(--nDedos - dedo) * sizeof *dedos);
  if (!nDedos && !dedosExcedentes) { toqueAlvo.ok = 0; arrModo = ARR_NADA; }
  return 1;
}

// Inercia da rolagem: chamada por quadro (ponteiro_quadro).
static void inerciaQuadro(Uint32 agora) {
  Uint32 dt;
  if (!inercia) return;
  dt = agora - inUltMs;
  if (dt > 100) dt = 100;   // quadro travado nao vira um salto de dez setas
  inUltMs = agora;
  if (inRolagem) {
    PonteiroRolagemFn fn = inRolagem;
    float d = inVel * (float)dt;
    inVel *= expf(-(float)dt / PONT_INERCIA_TAU);
    if (!rolagemEntregar(fn, PONT_ROL_INERCIA, inEixoY, d, inVel, inX, inY) && inRolagem == fn)
      encerrarInercia(PONT_ROL_FIM);
    else if (inRolagem == fn && fabsf(inVel) < PONT_INERCIA_PARA) encerrarInercia(PONT_ROL_FIM);
    return;
  }
  inAcum += inVel * (float)dt;
  inVel *= expf(-(float)dt / PONT_INERCIA_TAU);
  inAcum = rolarPassos(inAcum, inEixoY, PONT_INERCIA_MAXP);
  if (fabsf(inVel) < PONT_INERCIA_PARA) pararInercia();
}

int ponteiro_evento(const SDL_Event *e, void (*entregar)(const SDL_Event *)) {
  Uint32 agora = agoraMs();
  if (quadroSessaoPendente &&
      (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONDOWN ||
       e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEWHEEL ||
       e->type == SDL_FINGERDOWN || e->type == SDL_FINGERMOTION || e->type == SDL_FINGERUP))
    return 1;
  switch (e->type) {
    case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
#ifdef NV_TOUCH_UI
      if (e->tfinger.touchId == SDL_MOUSE_TOUCHID) return 1;
      // Um dedo substitui o cursor do mouse, sem desativar os alvos de toque.
      esconder("toque");
      if (!toqueQuadroPronto) return 1;
#endif
      return eventoToque(e, entregar);
    case SDL_APP_WILLENTERBACKGROUND:
      cancelarToque();
      return 0;
    case SDL_WINDOWEVENT:
#ifdef NV_TOUCH_UI
      if (e->window.event == SDL_WINDOWEVENT_RESIZED ||
          e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) toqueQuadroPronto = 0;
#endif
      if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
          e->window.event == SDL_WINDOWEVENT_RESIZED ||
          e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) cancelarToque();
      return 0;
    case SDL_MOUSEMOTION:
      if (e->motion.which == SDL_TOUCH_MOUSEID) return 1;
      converter(e->motion.windowID, e->motion.x, e->motion.y);
      primeiro(0, "movimento", e->motion.x, e->motion.y);
      if (tremorDaSeta(agora)) return 1;
      ultimoMov = agora;
      reaparecer();
      mover();
      return 1;

    case SDL_MOUSEBUTTONDOWN: {
      if (e->button.which == SDL_TOUCH_MOUSEID) return 1;
      invalidarToque();
      converter(e->button.windowID, e->button.x, e->button.y);
      primeiro(1, "clique", e->button.x, e->button.y);
      ultimoMov = agora;
      reaparecer();
      if (e->button.button == SDL_BUTTON_RIGHT) {
        // Nao ha botao direito no Magic Remote; no Mac ele e o Voltar, que e
        // o que falta para testar sem teclado.
        tecla(entregar, SDL_KEYDOWN, SDLK_AC_BACK); voltarPendente = 1;
        return 1;
      }
      if (e->button.button != SDL_BUTTON_LEFT) return 1;
      if (okTeclaEm && agora - okTeclaEm < PONT_DEDUPE_MS) { engolirCliqueSolto = 1; return 1; }
      cliqueEm = agora;
      { const PonteiroAlvo *v = lista[pronto];
        int n = nLista[pronto];
        int i = ponteiro_achar(v, n, px, py);
        rastro("clique", i >= 0 ? &v[i] : NULL, n);
        ativarPendente.ok = 0;
        if (i >= 0 && v[i].ativar) { guardar(&ativarPendente, &v[i]); return 1; }
        // Alvo sem nenhuma das duas funcoes e um ANTEPARO: o corpo de uma folha
        // absorve o clique no vazio em vez de deixa-lo cair no fundo que fecha.
        if (i >= 0 && !v[i].focar) return 1;
        if (i >= 0) {
          if (!mesmo(&hover, &v[i])) guardar(&hover, &v[i]);
          if (v[i].focar) v[i].focar(v[i].a, v[i].b);
        }
        // Sem alvo: so vale como OK numa camada que nao registra nada. Numa
        // que registra, clicar no vazio nao pode disparar o item em foco.
        if (i >= 0 || n == 0) {
          tecla(entregar, SDL_KEYDOWN, SDLK_RETURN);
          okPendente = 1;
        } }
      return 1;
    }

    case SDL_MOUSEBUTTONUP:
      if (e->button.which == SDL_TOUCH_MOUSEID) return 1;
      converter(e->button.windowID, e->button.x, e->button.y);
      if (voltarPendente && e->button.button == SDL_BUTTON_RIGHT) {
        voltarPendente = 0; tecla(entregar, SDL_KEYUP, SDLK_AC_BACK); return 1;
      }
      if (e->button.button != SDL_BUTTON_LEFT) return 1;
      if (engolirCliqueSolto) { engolirCliqueSolto = 0; return 1; }
      if (okPendente) { okPendente = 0; tecla(entregar, SDL_KEYUP, SDLK_RETURN); }
      if (ativarPendente.ok) {
        const PonteiroAlvo *v = lista[pronto];
        int i = ponteiro_achar(v, nLista[pronto], px, py);
        Ident id = ativarPendente;
        ativarPendente.ok = 0;
        if (i >= 0 && mesmo(&id, &v[i])) v[i].ativar(v[i].a, v[i].b);
      }
      return 1;

    case SDL_MOUSEWHEEL: {
      if (e->wheel.which == SDL_TOUCH_MOUSEID) return 1;
      invalidarToque();
      int dy = e->wheel.y, dx = e->wheel.x;
      primeiro(2, "rodinha", dx, dy);
      if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { dy = -dy; dx = -dx; }
      // Um passo por dente, com freio: o trackpad do Mac manda dezenas de
      // eventos por gesto e cada um viraria uma seta.
      if (agora - rodaEm < PONT_RODA_MS) return 1;
      if (!dy && !dx) return 1;
      rodaEm = agora;
      { SDL_Keycode k = dy > 0 ? SDLK_UP : dy < 0 ? SDLK_DOWN
                      : dx > 0 ? SDLK_RIGHT : SDLK_LEFT;
        tecla(entregar, SDL_KEYDOWN, k);
        tecla(entregar, SDL_KEYUP, k); }
      return 1;
    }

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      int sc = (int)e->key.keysym.scancode;
      SDL_Keycode k = e->key.keysym.sym;
      if (sc == PONT_SC_CURSOR_SHOW || sc == PONT_SC_CURSOR_HIDE) {
        // Aviso do sistema, nao tecla: nenhuma tela deve ve-lo.
        if (e->type == SDL_KEYDOWN) {
          primeiro(sc == PONT_SC_CURSOR_SHOW ? 3 : 4,
                   sc == PONT_SC_CURSOR_SHOW ? "cursor-mostrou" : "cursor-escondeu",
                   sc, 0);
          if (sc == PONT_SC_CURSOR_HIDE) {
            visivel = 1; esconder("sistema");
            // O sistema ja escondeu a dele: nada a devolver depois.
            sistemaEscondido = 0;
          } else if (escondidoSeta && agora - setaEm < PONT_SETA_JANELA_MS) {
            // O compositor reacendeu a seta dele com o tremor de quem apertou
            // a seta: continua escondido do nosso lado e pede para apagar a dele
            // de novo. O movimento que vencer o limiar a devolve.
#ifdef NV_PONT_WEBOS
            if (cursorSistema) { cursorSistema(SDL_FALSE); sistemaEscondido = 1; }
#endif
          } else if (escondidoSeta) {
            // 484 LONGE DA SETA (#204): a mao pegou o controle. Apagar a seta do
            // sistema aqui travou o ponteiro na TV do relato (PowerVR BXE-4-32,
            // 1,2 GB; log da 1.6.3 a 1.6.5): cada 484 era respondido com outro
            // apagar (485 uns 40 ms depois), nenhum movimento chegava com ela
            // apagada, o limiar nunca vencia — 12 tentativas numa sessao, 0
            // cliques. Na 1.6.1 (sem este ramo) o 485 vinha 0,5 a 17 s depois.
            // A seta do sistema fica; o nosso hover segue esperando o limiar.
#ifdef NV_PONT_WEBOS
            if (sistemaEscondido && cursorSistema) cursorSistema(SDL_TRUE);
#endif
            sistemaEscondido = 0;
          } else { reaparecer(); ultimoMov = agora; hover.ok = 0; }
        }
        return 1;
      }
      if (e->type == SDL_KEYDOWN) invalidarToque();
      if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (e->type == SDL_KEYDOWN) {
          if (e->key.repeat) return engolirOkSolto;
          if (okPendente || (cliqueEm && agora - cliqueEm < PONT_DEDUPE_MS)) {
            engolirOkSolto = 1; return 1;
          }
          okTeclaEm = agora;
          return 0;
        }
        if (engolirOkSolto) { engolirOkSolto = 0; return 1; }
        return 0;
      }
      if (e->type == SDL_KEYDOWN &&
          (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT)) {
        // Toda seta rearma a janela, mesmo com o cursor ja escondido: quem
        // navega de seta em seta nao pode ver o cursor voltar entre elas.
        esconder("seta");
        escondidoSeta = 1; setaEm = agora; setaAncora = 0;
      }
      return 0;
    }
    default:
      return 0;
  }
}

void ponteiro_quadro(Uint32 agora) {
  nLista[escreve] = 0;
  listaRolagem[escreve] = NULL;
#ifdef NV_TOUCH_UI
  listaBorda[escreve] = (Borda){0};
#endif
  if (!inRolagem) inerciaQuadro(agora);
  // No webOS o relogio proprio so vale com SDL_webOSCursorVisibility: e ela
  // que apaga a seta do sistema junto (esconder). Sem ela, desligar o hover
  // aqui deixaria a seta na tela sem funcionar; ai fica o sono do sistema,
  // que avisa com o 485.
#ifdef NV_PONT_WEBOS
  if (!cursorSistema) return;
#endif
  if (visivel && agora - ultimoMov > PONT_DORME_MS && !okPendente) esconder("parado");
}

// Fecha o quadro: a lista que o desenho acabou de montar passa a ser a que os
// eventos do PROXIMO laco consultam — e o que esta na tela quando a mao mexe.
static void fecharQuadro(void) {
  Uint32 agora = agoraMs();
  pronto = escreve;
  escreve ^= 1;
  quadroSessaoPendente = 0;
  nLista[escreve] = 0;
  listaRolagem[escreve] = NULL;
#ifdef NV_TOUCH_UI
  listaBorda[escreve] = (Borda){0};
  toqueQuadroPronto = 1;
  if (nDedos && toqueBorda.ativar && toqueBorda.ativar != listaBorda[pronto].ativar) invalidarToque();
#endif
  if (nDedos && toqueRolagem && toqueRolagem != listaRolagem[pronto]) invalidarToque();
  if (inRolagem) {
    if (inRolagem != listaRolagem[pronto]) pararInercia();
    else inerciaQuadro(agora);
  }
  // O alvo que o hover focou ainda esta onde estava?
  if (visivel && hover.ok) {
    const PonteiroAlvo *v = lista[pronto];
    int achou = 0;
    for (int i = nLista[pronto] - 1; i >= 0; i--)
      if (mesmo(&hover, &v[i])) {
        float cx = v[i].x + v[i].w * 0.5f, cy = v[i].y + v[i].h * 0.5f;
        if (fabsf(cx - hover.cx) > 3.0f || fabsf(cy - hover.cy) > 3.0f)
          conteudoMexeuEm = agora;
        hover.cx = cx; hover.cy = cy;
        achou = 1;
        break;
      }
    // Sumiu (rolou para fora, uma folha abriu por cima): um freio so, e a
    // identidade vai embora — guardada, ela travaria o hover para sempre.
    if (!achou) { conteudoMexeuEm = agora; hover.ok = 0; }
  }
  longoToque(agora);
}

void ponteiro_alvo(float x, float y, float w, float h,
                   PonteiroFn focar, PonteiroFn ativar, int a, int b) {
  PonteiroAlvo *al;
  if (!ponteiro_ativo()) return;
  if (w <= 0 || h <= 0 || nLista[escreve] >= PONT_MAX_ALVOS) return;
  al = &lista[escreve][nLista[escreve]++];
  // Alvo registrado de dentro de uma camada ampliada (gfx_escala): a lista
  // guarda a tela REAL, a mesma em que o cursor anda.
  { float e = gfx_escala(); x *= e; y *= e; w *= e; h *= e; }
  al->x = x; al->y = y; al->w = w; al->h = h;
  al->focar = focar; al->ativar = ativar; al->a = a; al->b = b;
  al->arrasta = 0;
  al->segurar = NULL;
}

void ponteiro_alvo_faixa(float x, float y, float w, float h, float y0, float y1,
                         PonteiroFn focar, PonteiroFn ativar, int a, int b) {
  if (!ponteiro_ativo()) return;
  if (y < y0) { h -= y0 - y; y = y0; }
  if (y + h > y1) h = y1 - y;
  ponteiro_alvo(x, y, w, h, focar, ativar, a, b);
}

void ponteiro_alvo_arrastavel(void) {
  if (nLista[escreve] > 0) lista[escreve][nLista[escreve] - 1].arrasta = 1;
}

void ponteiro_alvo_segurar(PonteiroFn focar) {
  if (nLista[escreve] > 0) lista[escreve][nLista[escreve] - 1].segurar = focar;
}

void ponteiro_camada(void) {
  if (!ponteiro_ativo()) return;
  nLista[escreve] = 0;
  listaRolagem[escreve] = NULL;
#ifdef NV_TOUCH_UI
  listaBorda[escreve] = (Borda){0};
#endif
}

void ponteiro_rolagem(PonteiroRolagemFn fn) {
  if (!ponteiro_ativo()) return;
#ifdef NV_TOUCH_UI
  // TV keeps drag-as-arrows: no scroll capture outside Mobile mode.
  if (!layout_modo_mobile()) return;
#endif
  listaRolagem[escreve] = fn;
}
#ifdef NV_TOUCH_UI
void ponteiro_borda_esquerda(float largura, PonteiroBordaFn ativar) {
  if (ponteiro_ativo() && layout_modo_mobile()) listaBorda[escreve] = (Borda){fmaxf(0.0f, largura) * gfx_escala(), ativar};
}
int ponteiro_borda_registrada(void) { return layout_modo_mobile() && listaBorda[escreve].ativar != NULL; }
void ponteiro_borda_ativar(void) {
  if (!layout_modo_mobile() || !toqueQuadroPronto) return;
  PonteiroBordaFn fn = listaBorda[pronto].ativar;
  if (fn) fn();
}
#endif

void ponteiro_desenhar(void) {
  float g, d, sobre;
  if (!visivel) { fecharQuadro(); return; }
#ifdef NV_PONT_WEBOS
  // A seta no webOS e a do sistema (main.c deixa SDL_ShowCursor ligado: sem
  // ele o SDL da LG nao entrega movimento). Desenhar outra seria cursor duplo.
  fecharQuadro();
  return;
#endif
  // O cursor nao pode herdar o recorte nem o fade de grupo de quem desenhou
  // por ultimo (as fileiras da home deixam os dois ligados).
  g = gfx_opacidade_grupo;
  gfx_opacidade_grupo = 1.0f;
  gfx_sem_recorte();
  sobre = ponteiro_achar(lista[escreve], nLista[escreve], px, py) >= 0 ? 1.0f : 0.0f;
  d = 26.0f + 6.0f * sobre;
  gfx_cor((GfxRect){ px - d * 0.5f - 3.0f, py - d * 0.5f - 1.0f, d + 6.0f, d + 6.0f },
          0.5f, 0, 0, 0, 0.45f);
  gfx_cor((GfxRect){ px - d * 0.5f, py - d * 0.5f, d, d }, 0.5f, 1, 1, 1, 0.96f);
  d -= 10.0f;
  gfx_cor((GfxRect){ px - d * 0.5f, py - d * 0.5f, d, d }, 0.5f,
          0.10f, 0.11f, 0.13f, 0.30f + 0.5f * sobre);
  gfx_opacidade_grupo = g;
  fecharQuadro();
}

