// Ponteiro do Magic Remote: estado, hit-test e cursor. Ver ponteiro.h.
#include "ponteiro.h"
#include "gfx.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#define NV_PONT_WEBOS 1
#endif

// Scancodes sinteticos do SDL_webOS.h (SDK openlgtv). Transcritos porque o
// header do SDK nao e incluido no build do Mac nem no da Samsung.
#define PONT_SC_CURSOR_SHOW 484
#define PONT_SC_CURSOR_HIDE 485

#define PONT_MAX_ALVOS   512
// Parado este tempo, o cursor some. O do sistema no webOS dorme sozinho
// tambem (SDL_WEBOS_CURSOR_SLEEP_TIME); este e o do app.
#define PONT_DORME_MS    4000
// Janela em que um OK de tecla e um clique sao O MESMO aperto. Nao esta
// provado se o webOS manda os dois quando o cursor esta na tela; se mandar,
// sem isto o OK valeria duas vezes (abrir e ja reproduzir).
#define PONT_DEDUPE_MS   150
// Quanto o alvo sob o cursor tem de ficar parado antes de o hover poder trocar
// o foco. Ver `conteudoMexeuEm`.
#define PONT_ASSENTA_MS  150
#define PONT_RODA_MS      70

static PonteiroAlvo lista[2][PONT_MAX_ALVOS];
static int nLista[2];
static int escreve = 0;          // a que o desenho deste quadro preenche
static int pronto  = 1;          // a do quadro anterior, que o hit-test le

static float px = NV_TELA_W * 0.5f, py = NV_TELA_H * 0.5f;
static int visivel = 0;
static Uint32 ultimoMov = 0;
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

#ifdef NV_PONT_WEBOS
static SDL_bool (*cursorSistema)(SDL_bool) = NULL;
#endif

static Uint32 agoraMs(void) { return relogio ? relogio() : SDL_GetTicks(); }

void ponteiro_teste_relogio(Uint32 (*fn)(void)) { relogio = fn; }
void ponteiro_teste_janela(int w, int h) { janelaW = w; janelaH = h; }

float ponteiro_x(void) { return px; }
float ponteiro_y(void) { return py; }
int ponteiro_ativo(void) { return visivel; }

void ponteiro_iniciar(void) {
  nLista[0] = nLista[1] = 0;
  visivel = 0;
  memset(&hover, 0, sizeof hover);
#ifdef NV_PONT_WEBOS
  // RTLD_DEFAULT: o SDL ja esta carregado no processo; o que se quer saber e
  // se ESTA firmware o exporta.
  *(void **)(&cursorSistema) = dlsym(RTLD_DEFAULT, "SDL_webOSCursorVisibility");
  printf("[ponteiro] SDL_webOSCursorVisibility: %s\n",
         cursorSistema ? "presente" : "ausente");
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
  if (cursorSistema) cursorSistema(SDL_FALSE);
#endif
}

static void primeiro(int tipo, const char *nome, int x, int y) {
  if (tipo < 0 || tipo >= 8 || logouTipo[tipo]) return;
  logouTipo[tipo] = 1;
  printf("[ponteiro] primeiro evento tipo=%s x=%d y=%d -> logico %.0f,%.0f "
         "(janela %dx%d)\n", nome, x, y, px, py, janelaW, janelaH);
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
  if (px < 0) px = 0; if (px > NV_TELA_W - 1) px = NV_TELA_W - 1;
  if (py < 0) py = 0; if (py > NV_TELA_H - 1) py = NV_TELA_H - 1;
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
  SDL_Event t; SDL_zero(t);
  t.type = tipo; t.key.keysym.sym = k;
  t.key.state = tipo == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  entregar(&t);
}

static void mover(void) {
  const PonteiroAlvo *v = lista[pronto];
  int i = ponteiro_achar(v, nLista[pronto], px, py);
  if (i < 0 || mesmo(&hover, &v[i])) return;
  if (conteudoMexeuEm && agoraMs() - conteudoMexeuEm < PONT_ASSENTA_MS) return;
  guardar(&hover, &v[i]);
  if (v[i].focar) v[i].focar(v[i].a, v[i].b);
}

int ponteiro_evento(const SDL_Event *e, void (*entregar)(const SDL_Event *)) {
  Uint32 agora = agoraMs();
  switch (e->type) {
    case SDL_MOUSEMOTION:
      if (e->motion.which == SDL_TOUCH_MOUSEID) return 0;
      converter(e->motion.windowID, e->motion.x, e->motion.y);
      primeiro(0, "movimento", e->motion.x, e->motion.y);
      ultimoMov = agora;
      if (!visivel) { visivel = 1; hover.ok = 0; }
      mover();
      return 1;

    case SDL_MOUSEBUTTONDOWN: {
      if (e->button.which == SDL_TOUCH_MOUSEID) return 0;
      converter(e->button.windowID, e->button.x, e->button.y);
      primeiro(1, "clique", e->button.x, e->button.y);
      ultimoMov = agora;
      visivel = 1;
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
        ativarPendente.ok = 0;
        if (i >= 0 && v[i].ativar) { guardar(&ativarPendente, &v[i]); return 1; }
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
      if (e->button.which == SDL_TOUCH_MOUSEID) return 0;
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
          if (sc == PONT_SC_CURSOR_HIDE) { visivel = 1; esconder("sistema"); }
          else { visivel = 1; ultimoMov = agora; hover.ok = 0; }
        }
        return 1;
      }
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
          (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT))
        esconder("seta");
      return 0;
    }
    default:
      return 0;
  }
}

void ponteiro_quadro(Uint32 agora) {
  nLista[escreve] = 0;
  if (visivel && agora - ultimoMov > PONT_DORME_MS && !okPendente) esconder("parado");
}

// Fecha o quadro: a lista que o desenho acabou de montar passa a ser a que os
// eventos do PROXIMO laco consultam — e o que esta na tela quando a mao mexe.
static void fecharQuadro(void) {
  Uint32 agora = agoraMs();
  pronto = escreve;
  escreve ^= 1;
  nLista[escreve] = 0;
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
}

void ponteiro_alvo(float x, float y, float w, float h,
                   PonteiroFn focar, PonteiroFn ativar, int a, int b) {
  PonteiroAlvo *al;
  if (!visivel) return;
  if (w <= 0 || h <= 0 || nLista[escreve] >= PONT_MAX_ALVOS) return;
  al = &lista[escreve][nLista[escreve]++];
  al->x = x; al->y = y; al->w = w; al->h = h;
  al->focar = focar; al->ativar = ativar; al->a = a; al->b = b;
}

void ponteiro_camada(void) {
  if (!visivel) return;
  nLista[escreve] = 0;
}

void ponteiro_desenhar(void) {
  float g, d, sobre;
  if (!visivel) { fecharQuadro(); return; }
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
