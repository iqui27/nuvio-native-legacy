// Faixa de filtros da Biblioteca cabe na tela (geometria dos alvos).
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


// FAIXA DE FILTROS DA BIBLIOTECA em 1920x1080, escala padrao (topo a 120%),
// fonte Inter, contagens grandes e o valor mais largo do seletor Origem
// ("Conta Nuvio"). Antes do conserto "Origem: Conta Nuvio" ia de x=1803 a
// x=2054: valor e foco cortados. Cada alvo da faixa (abas e chips) tem de caber
// na area util [NV_BIB_X, NV_BIB_DIR] — o anel de foco sai dai por dentro da tela.
//
//   bash tests/biblioteca_faixa.sh
#define N_ITENS 1400
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
    it->naColecao = i % 2;
    it->fontes = i % 4 == 0 ? 0 : i % 4;
  }
  cat_definir(v, N_ITENS);
}

// Maior x+w e menor x dos alvos da faixa (acima da grade). Devolve quantos.
static int faixa(float *xmin, float *xmax) {
  const PonteiroAlvo *v;
  int n = alvos(&v), c = 0;
  *xmin = 1e9f; *xmax = -1e9f;
  for (int i = 0; i < n; i++) {
    if (v[i].y > 330.0f) continue;
    c++;
    if (v[i].x < *xmin) *xmin = v[i].x;
    if (v[i].x + v[i].w > *xmax) *xmax = v[i].x + v[i].w;
  }
  return c;
}

static void confereFaixa(const char *quando) {
  float x0, x1;
  char msg[160];
  int c = faixa(&x0, &x1);
  printf("     %s: %d alvos, x=%.0f..%.0f\n", quando, c, x0, x1);
  snprintf(msg, sizeof msg, "%s: abas e 4 chips sao alvos", quando);
  CONFERE(c >= 9, msg);
  snprintf(msg, sizeof msg, "%s: nada passa da area util (%.0f)", quando, NV_BIB_DIR);
  CONFERE(x1 <= NV_BIB_DIR + 0.5f, msg);
  snprintf(msg, sizeof msg, "%s: nada antes da margem esquerda", quando);
  CONFERE(x0 >= NV_BIB_X - 0.5f, msg);
}

int main(void) {
  const char *quero = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  if (!quero || !*quero) { printf("RECUSADO: rode por tests/biblioteca_faixa.sh\n"); return 2; }
  dados_iniciar(quero);
  if (strcmp(dados_dir(), quero)) { printf("RECUSADO: dados_dir() nao e a pasta do teste\n"); return 2; }
  { char cam[700]; FILE *fa;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    fa = fopen(cam, "w");
    if (fa) { fprintf(fa, "idioma 0\n"); fclose(fa); } }   // fonte padrao: Inter
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: faixa da Biblioteca", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  extras_carregar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  ponteiro_teste_relogio(agora);
  ponteiro_teste_janela(1920, 1080);
  ponteiro_teste_toque(0);

  printf("== faixa da Biblioteca, 1920x1080, Origem ate Conta Nuvio\n");
  povoar();
  biblioteca_iniciar();
  mover(5, 5);
  quadros(70);
  confereFaixa("Origem: Todas");
  // Clica no ultimo chip da faixa (Origem): Todas > Trakt > Simkl > Conta Nuvio.
  for (int i = 1; i <= 3; i++) {
    const PonteiroAlvo *v;
    int n = alvos(&v), ult = -1;
    char q[64];
    for (int j = 0; j < n; j++)
      if (v[j].y < 330.0f && (ult < 0 || v[j].y > v[ult].y ||
          (v[j].y == v[ult].y && v[j].x > v[ult].x))) ult = j;
    assert(ult >= 0);
    clicar(cx(&v[ult]), cy(&v[ult])); quadros(70);
    acordar(); quadros(10);
    snprintf(q, sizeof q, "Origem %d/3", i);
    confereFaixa(q);
  }
  SDL_Quit();
  printf(falhas ? "\nbiblioteca_faixa: TEM FALHA\n" : "\nbiblioteca_faixa: tudo ok\n");
  return falhas ? 1 : 0;
}
