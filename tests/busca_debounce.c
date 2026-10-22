// BUSCA (#368): debounce da rede, fileiras do termo anterior sem piscar e UM
// cat_acrescentar_lote por refiltrar. busca.c e compilado com os simbolos de
// descoberta/catalogo redirecionados (-D...=teste_...) para estes stubs.
#include "busca.h"
#include "catalogo.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int busca_teste_itens(void);

// --- stubs -------------------------------------------------------------------
#define NALVOS 4
static int  nBuscar, nLote;
static char ultimoTermo[96];
static char termoResp[96];     // termo da "resposta" simulada
static int  respondeu[NALVOS]; // quais alvos ja responderam para termoResp
static int  nPorAlvo = 3;

void teste_desc_buscar(const char *t) {
  if (!strcmp(t, ultimoTermo)) return;   // como o real: termo igual nao refaz
  nBuscar++; snprintf(ultimoTermo, sizeof ultimoTermo, "%s", t);
}
int teste_desc_busca_n_alvos(void) { return NALVOS; }
int teste_desc_busca_alvo_n(int a, const char *t) {
  return (a >= 0 && a < NALVOS && respondeu[a] && !strcmp(t, termoResp)) ? nPorAlvo : 0;
}
int teste_desc_busca_alvo_item(int a, int i, CatItem *d) {
  memset(d, 0, sizeof *d);
  snprintf(d->imdb, sizeof d->imdb, "tt9%d%d%s", a, i, termoResp);
  snprintf(d->titulo, sizeof d->titulo, "Zzz %d %d", a, i);
  snprintf(d->tipo, sizeof d->tipo, "movie");
  return 1;
}
const char *teste_desc_busca_alvo_titulo(int a) { (void)a; return "Filmes"; }
const char *teste_desc_busca_alvo_addon(int a) { (void)a; return "Addon"; }
int teste_desc_busca_alvo_nuvio(int a) { (void)a; return 0; }
int teste_desc_busca_n_fontes(void) { return 2; }
int teste_desc_busca_base_oculta(const char *b) { (void)b; return 0; }
int teste_desc_busca_n(const char *t) {
  int a, s = 0;
  for (a = 0; a < NALVOS; a++) s += teste_desc_busca_alvo_n(a, t);
  return s;
}
int teste_desc_busca_chegou(const char *t) {
  int a;
  if (strcmp(t, termoResp)) return 0;
  for (a = 0; a < NALVOS; a++) if (respondeu[a]) return 1;
  return 0;
}
int cat_acrescentar_lote(const CatItem *v, int qtd, int *saidaIdx);
int teste_cat_lote(const CatItem *v, int qtd, int *saidaIdx) {
  nLote++;
  return cat_acrescentar_lote(v, qtd, saidaIdx);
}

// --- ajudantes ---------------------------------------------------------------
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  busca_evento(&e);
}
static void quadro(void) {
  SDL_PumpEvents();
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
}
static void espera(Uint32 ms) {
  Uint32 t0 = SDL_GetTicks();
  while (SDL_GetTicks() - t0 < ms) { quadro(); SDL_Delay(5); }
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *janela;
  SDL_GLContext gl;
  CatItem base;
  const char *m = "matrix";
  int i;
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("busca debounce", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(96);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  ajustes_iniciar();
  memset(&base, 0, sizeof base);
  snprintf(base.imdb, sizeof base.imdb, "tt0000001");
  snprintf(base.titulo, sizeof base.titulo, "Base");
  snprintf(base.tipo, sizeof base.tipo, "movie");
  cat_definir(&base, 1);
  busca_iniciar();
  quadro();

  // 1) "matrix" digitado depressa (6 teclas, bem dentro de 300 ms): UM pedido.
  for (i = 0; m[i]; i++) tecla(SDLK_a + (m[i] - 'a'));
  assert(!strcmp(busca_consulta(), "matrix"));
  for (i = 0; i < 5; i++) quadro();
  printf("apos teclas: desc_buscar=%d (esperado 0 dentro do debounce)\n", nBuscar);
  assert(nBuscar == 0);
  espera(380);
  printf("apos assentar: desc_buscar=%d termo=%s\n", nBuscar, ultimoTermo);
  assert(nBuscar == 1 && !strcmp(ultimoTermo, "matrix"));

  // 3) quatro alvos respondem no MESMO quadro: UM cat_acrescentar_lote.
  snprintf(termoResp, sizeof termoResp, "matrix");
  for (i = 0; i < NALVOS; i++) respondeu[i] = 1;
  nLote = 0;
  espera(200);
  printf("respostas de %d alvos: cat_acrescentar_lote=%d itens=%d\n", NALVOS, nLote, busca_teste_itens());
  assert(nLote == 1);
  assert(busca_teste_itens() == NALVOS * nPorAlvo);

  // 2) digita mais uma letra: as fileiras do termo anterior FICAM ate a 1a resposta.
  tecla(SDLK_s);
  for (i = 0; i < 3; i++) quadro();
  printf("novo termo sem resposta: itens=%d (esperado %d)\n", busca_teste_itens(), NALVOS * nPorAlvo);
  assert(busca_teste_itens() == NALVOS * nPorAlvo);
  espera(380);   // o pedido sai; ainda sem resposta
  assert(nBuscar == 2 && !strcmp(ultimoTermo, "matrixs"));
  assert(busca_teste_itens() == NALVOS * nPorAlvo);
  // Chega a 1a resposta do termo novo (so o alvo 0, com 3 itens): o velho some.
  snprintf(termoResp, sizeof termoResp, "matrixs");
  for (i = 0; i < NALVOS; i++) respondeu[i] = 0;
  respondeu[0] = 1;
  espera(200);
  printf("1a resposta do termo novo: itens=%d (esperado %d)\n", busca_teste_itens(), nPorAlvo);
  assert(busca_teste_itens() == nPorAlvo);

  // 4) RESPOSTA VAZIA (revisao 2.0.3 #5): "matrixs" tem 3 itens; digita "x" e o
  // termo novo fica SEM resposta (as fileiras antigas ficam; ultimoRemoto passa
  // a ver 0). Depois TODOS os alvos respondem com 0 itens: a contagem segue 0,
  // so a chegada muda — os cartoes antigos precisam sair, sem outra edicao.
  tecla(SDLK_x);
  espera(380);
  assert(nBuscar == 3 && !strcmp(ultimoTermo, "matrixsx"));
  espera(200);   // ultimoRemoto observa n == 0 com as fileiras velhas ainda la
  printf("termo novo pendente: itens=%d (esperado %d)\n", busca_teste_itens(), nPorAlvo);
  assert(busca_teste_itens() == nPorAlvo);
  snprintf(termoResp, sizeof termoResp, "matrixsx");
  nPorAlvo = 0;
  for (i = 0; i < NALVOS; i++) respondeu[i] = 1;
  for (i = 0; i < 90; i++) quadro();   // so quadros: nada mais muda
  printf("todos responderam 0: itens=%d (esperado 0)\n", busca_teste_itens());
  assert(busca_teste_itens() == 0);

  // Concluir/voz: sem debounce — ST_FIM nao e simulavel aqui; o caminho de
  // "mesmo termo" nao recria pedido.
  tecla(SDLK_BACKSPACE);
  espera(380);
  printf("fim: desc_buscar=%d\n", nBuscar);
  puts("OK busca_debounce");
  return 0;
}
