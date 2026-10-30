// Biblioteca iOS: geometria da grade fechada (4x78) e aberta (8x92).
#include <math.h>
#include "biblioteca.h"
#include "ajustes.h"
#include "ponteiro.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "extras.h"
#include "layout.h"
#include "escala.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SDL_Window *janela;
static int falhas;
static float direita = NV_BIB_DIR;
#define CONFERE(c, msg) do { if (c) printf("ok   %s\n", msg); \
  else { printf("FALHA: %s (linha %d)\n", msg, __LINE__); falhas++; } } while (0)

static Uint32 relogio = 100000;
static Uint32 agora(void) { return relogio; }

// A tela sob teste: as tres entradas que o app.c chama.
typedef struct {
  void (*evento)(const SDL_Event *);
  void (*atualizar)(float, Uint32);
  void (*desenhar)(Uint32);
} Tela;
static const Tela BIB = { biblioteca_evento, biblioteca_atualizar, biblioteca_desenhar };
static const Tela *tela = &BIB;
static void entregar(const SDL_Event *e) { tela->evento(e); }
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    tela->atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ponteiro_quadro(relogio);
    tela->desenhar(SDL_GetTicks());
    ponteiro_desenhar();
    SDL_GL_SwapWindow(janela);
  }
}
static void mover(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION; e.motion.x = (int)x; e.motion.y = (int)y;
  relogio += 400;   // depois do freio do conteudo que anda (PONT_ASSENTA_MS)
  ponteiro_evento(&e, entregar);
}
// O gesto que devolve o cursor depois de uma seta: movimento continuo (menos
// de PONT_SETA_PAUSA_MS entre eventos) que vence o limiar de distancia.
static void acordar(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  relogio += 400;
  for (int i = 0; i < 6; i++) {
    e.motion.x = 5 + i * 40; e.motion.y = 5 + i * 40;
    relogio += 40;
    ponteiro_evento(&e, entregar);
  }
}
static void clicar(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
  e.button.x = (int)x; e.button.y = (int)y;
  relogio += 400;
  ponteiro_evento(&e, entregar);
  e.type = SDL_MOUSEBUTTONUP;
  relogio += 60;
  ponteiro_evento(&e, entregar);
}
static void okTecla(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; relogio += 400;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
  e.type = SDL_KEYUP;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
}
static void seta(SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; relogio += 400;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
  e.type = SDL_KEYUP;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
}
// O centro do alvo i da lista que o hit-test le.
static int alvos(const PonteiroAlvo **v) { return ponteiro_teste_lista(v); }
static float cx(const PonteiroAlvo *a) { return a->x + a->w * 0.5f; }
static float cy(const PonteiroAlvo *a) { return a->y + a->h * 0.5f; }


// BIBLIOTECA iOS (mock ios-08): iPhone fechado 466x678 pt = 4 colunas de 78 pt
// (vao 8, margem 40); display interno aberto 951x669 pt = 8 colunas de 92 pt com
// 24 pt de vao centradas, a lacuna 4/5 sobre a dobra (464-488 pt). O aberto e
// forcado por -DNV_BIB_AMPLO (o simulador nao mostra o display interno).
//
//   bash tests/biblioteca_retrato.sh
#define N_ITENS 60
static void povoar(void) {
  static CatItem v[N_ITENS];
  for (int i = 0; i < N_ITENS; i++) {
    CatItem *it = &v[i];
    memset(it, 0, sizeof *it);
    snprintf(it->titulo, sizeof it->titulo, "Titulo %d", i);
    snprintf(it->imdb, sizeof it->imdb, "tt91%05d", i);
    snprintf(it->tipo, sizeof it->tipo, "%s", i % 3 ? "movie" : "series");
    snprintf(it->genero, sizeof it->genero, "Movie · Drama");
    it->naLista = 1;
    it->listaFontes = 1u << AJ_SALVOS_LOCAL;
  }
  cat_definir(v, N_ITENS);
}
static void salvar(const char *nome, int w, int h) {
  unsigned char *pix = malloc((size_t)w * h * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pix && s);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (int y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
  assert(!IMG_SavePNG(s, nome));
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", nome);
}
// Cartazes = alvos grandes (altura > largura): x em pt.
static int cartazes(float pt, float *xs, int max) {
  const PonteiroAlvo *v;
  int n = alvos(&v), c = 0;
  for (int i = 0; i < n && c < max; i++) {
    if (v[i].h < v[i].w * 1.2f || v[i].w < 40.0f * pt) continue;
    xs[c++] = v[i].x;
  }
  return c;
}
static int cmp(const void *a, const void *b) { float d = *(const float *)a - *(const float *)b; return d < 0 ? -1 : d > 0; }

int main(int argc, char **argv) {
  int aberto = argc > 1 && !strcmp(argv[1], "aberto");
  const char *saida = argc > 2 ? argv[2] : NULL;
  // 3 px por pt: fechado 466x678 pt, aberto 951x669 pt.
  int w = aberto ? 951 * 3 : 466 * 3, h = aberto ? 669 * 3 : 678 * 3;
  const char *quero = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  if (!quero || !*quero) { printf("RECUSADO: rode por tests/biblioteca_retrato.sh\n"); return 2; }
  dados_iniciar(quero);
  { char cam[700]; FILE *fa;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    fa = fopen(cam, "w");
    if (fa) { fprintf(fa, "idioma 0\n"); fclose(fa); } }
  layout_tela_definir(w, h);
  layout_modo_definir(1);
  gfx_tamanho_alvo(w, h);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: Biblioteca iOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, w, h);
  gfx_tamanho_alvo(w, h);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  extras_carregar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  layout_modo_definir(1);
  gfx_tamanho_alvo(w, h);
  ponteiro_teste_relogio(agora);
  ponteiro_teste_janela(w, h);
  ponteiro_teste_toque(1);
  gfx_escala_ui_definir(1.0f);
  printf("== Biblioteca %s, %dx%d px, NV_TELA %.0fx%.0f\n", aberto ? "aberta" : "fechada", w, h, NV_TELA_W, NV_TELA_H);
  povoar();
  biblioteca_iniciar();
  quadros(90);
  {
    float pt = (float)NV_TELA_W / (aberto ? 951.0f : 466.0f);
    float xs[64];
    int n = cartazes(pt, xs, 64), col = aberto ? 8 : 4;
    float ptpx = (float)w / (aberto ? 951.0f : 466.0f) / pt;   // unidade -> pixel
    (void)ptpx;
    qsort(xs, n, sizeof *xs, cmp);
    { int u = 0;   // colunas distintas (varias linhas repetem o x)
      for (int i = 0; i < n; i++) if (!u || xs[i] - xs[u - 1] > 2.0f * pt) xs[u++] = xs[i];
      n = u; }
    printf("     %d cartazes, pt=%.4f un\n", n, pt);
    CONFERE(n >= col, "ha cartazes na grade");
    { float x0 = xs[0] / pt, x1 = xs[col - 1] / pt, passo = (x1 - x0) / (col - 1);
      printf("     x0=%.2f pt, passo=%.2f pt\n", x0, passo);
      if (aberto) {
        CONFERE(fabsf(x0 - 23.5f) < 0.6f, "aberto: margem 23,5 pt");
        CONFERE(fabsf(passo - 116.0f) < 0.6f, "aberto: passo 92+24 pt");
        CONFERE(fabsf((xs[3] / pt + 92.0f) - 463.5f) < 0.8f && fabsf(xs[4] / pt - 487.5f) < 0.8f,
                "aberto: lacuna 463,5-487,5 pt cobre a dobra 464-488");
      } else {
        CONFERE(fabsf(x0 - 40.0f) < 0.6f, "fechado: margem 40 pt");
        CONFERE(fabsf(passo - 86.0f) < 0.6f, "fechado: passo 78+8 pt");
        CONFERE(xs[col - 1] / pt + 78.0f <= 382.5f, "fechado: grade fica na area esquerda da barra");
      }
    }
    for (int i = 0; i < n; i++) { (void)i; }
  }
  if (!aberto && getenv("NUVIO_M2_CONTROLS")) {
    const PonteiroAlvo *v;
    int n = alvos(&v), found = 0;
    // The first four pickers must expose touch targets on non-iOS phones.
    for (int i = 0; i < n; i++) {
      if (v[i].a != 0 || v[i].w <= 0) continue;
      if (!v[i].focar) continue;
      v[i].focar(v[i].a, v[i].b);
      int row, col, action;
      biblioteca_teste_foco(&row, &col, &action);
      if (row == 1 && col == 0) {
        found = 1;
        for (int p = 1; p < 4; p++) {
          seta(SDLK_RIGHT);
          biblioteca_teste_foco(&row, &col, &action);
          CONFERE(row == 1 && col == p, "portrait: D-pad reaches every filter");
        }
        for (int p = 2; p >= 0; p--) {
          seta(SDLK_LEFT);
          biblioteca_teste_foco(&row, &col, &action);
          CONFERE(row == 1 && col == p, "portrait: D-pad returns through filters to Sort");
        }
        break;
      }
    }
    CONFERE(found, "portrait: Sort exposes a touch target");
  }
  if (saida) {
    char p[1024];
    snprintf(p, sizeof p, "%s/biblioteca-%s.png", saida, aberto ? "aberto" : "fechado");
    salvar(p, w, h);
  }
  SDL_Quit();
  printf(falhas ? "\nbiblioteca_retrato: TEM FALHA\n" : "\nbiblioteca_retrato: tudo ok\n");
  return falhas ? 1 : 0;
}
