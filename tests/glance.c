// PREVIA RAPIDA (glance): o modelo de navegacao, sem janela e sem rede.
//
// O OK curto num cartaz abre a previa em vez da pagina do titulo (2.1,
// decisoes 9-11 do dono). Aqui se cobra o que o prototipo
// (docs/prototypes/quick-glance) prometia: setas trocam de titulo sem fechar,
// cima/baixo andam nas acoes, trocar de titulo volta o foco ao botao primario,
// Voltar fecha SEM abrir a pagina, e "Detalhes" abre a pagina do titulo pelo
// caminho original da tela (a callback `detalhes`).
//
//   bash tests/glance.sh
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "catalogo.h"
#include "glance.h"
#include "layout.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "home.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdlib.h>

static int shot;
static CatItem shotItens[5];

static int nChamadasDetalhes, ultimoDetalhes = -1, ultimoFoco = -1;
static int N = 5;

static int fTotal(void) { return N; }
static int fItem(int i, CatItem *o) {
  if (i < 0 || i >= N) return 0;
  if (shot) { *o = shotItens[i]; return 1; }
  memset(o, 0, sizeof *o);
  snprintf(o->titulo, sizeof o->titulo, "Titulo %d", i);
  snprintf(o->imdb, sizeof o->imdb, "tt000000%d", i);
  snprintf(o->tipo, sizeof o->tipo, i % 2 ? "series" : "movie");
  return 1;
}
static void fFoco(int i) { ultimoFoco = i; }
static void fDetalhes(int i) { nChamadasDetalhes++; ultimoDetalhes = i; }
static const GlanceFonte fonte = { fTotal, fItem, fFoco, fDetalhes };

static void tecla(int k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  glance_evento(&e);
}
static void soltar(int k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYUP;
  e.key.keysym.sym = k;
  glance_evento(&e);
}

static int falhas;
#define CHECA(c) do { if (!(c)) { fprintf(stderr, "  FALHOU: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)

static void capturar(const char *png) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pix && s);
  for (int i = 0; i < 60; i++) {
    tex_bombear(8); tex_novo_quadro(); txt_novo_quadro(); gfx_novo_quadro();
    glClearColor(0.025f, 0.025f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glance_atualizar(1.0f / 60, SDL_GetTicks()); glance_desenhar(SDL_GetTicks());
    SDL_Delay(8);
  }
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (int y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, png) == 0);
  free(pix); SDL_FreeSurface(s);
}
static void capturas(const char *prefixo) {
  char png[800];
  CatFileira fil = {0};
  dados_iniciar("deploy/app/art"); ajustes_dir(dados_dir());
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Quick glance", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  assert(tex_iniciar(48)); gfx_icones_dir("deploy/app/art");
  assert(home_iniciar("deploy/app/art"));
  N = 5;
  for (int i = 0; i < N; i++) {
    fItem(i, &shotItens[i]);
    snprintf(shotItens[i].titulo, sizeof shotItens[i].titulo, "%s", i == 2 ? "Ruptura" : i == 3 ? "A Ilha do Farol" : "Cidade Cinza");
    snprintf(shotItens[i].meta, sizeof shotItens[i].meta, "%s", i == 2 ? "2022 · 2 temporadas" : "2024 · 118 min");
    snprintf(shotItens[i].sinopse, sizeof shotItens[i].sinopse, "%s", "Funcionários de uma empresa têm as memórias do trabalho separadas das da vida pessoal. Quando um colega reaparece do lado de fora, Mark começa a desconfiar do que faz lá dentro.");
    snprintf(shotItens[i].backdrop, sizeof shotItens[i].backdrop, "deploy/app/art/%02d.jpg", i + 3);
    snprintf(shotItens[i].poster, sizeof shotItens[i].poster, "deploy/app/art/poster/%02d.jpg", i + 3);
    shotItens[i].nota = 87;
  }
  snprintf(shotItens[2].tipo, sizeof shotItens[2].tipo, "series");
  shotItens[2].progresso = 34; shotItens[2].restanteMin = 24; shotItens[2].naLista = 1;
  fil.n = N; snprintf(fil.titulo, sizeof fil.titulo, "Em alta");
  cat_definir_tudo(shotItens, N, &fil, 1); shot = 1;
  glance_abrir(&fonte, 2);
  snprintf(png, sizeof png, "%s-continuar.png", prefixo); capturar(png);
  tecla(SDLK_RIGHT); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(png, sizeof png, "%s-salvar.png", prefixo); capturar(png);
  tecla(SDLK_ESCAPE); assert(!glance_aberto());
  tex_encerrar(); SDL_DestroyWindow(w); SDL_Quit();
}

int main(int argc, char **argv) {
  CHECA(!glance_aberto());
  glance_abrir(&fonte, 2);
  CHECA(glance_aberto());
  CHECA(glance_indice() == 2);
  CHECA(glance_botao() == GLANCE_BT_PRIMARIO);

  // Baixo anda nas acoes; cima volta; nao passa dos extremos.
  tecla(SDLK_DOWN); CHECA(glance_botao() == GLANCE_BT_DETALHES);
  tecla(SDLK_DOWN); CHECA(glance_botao() == GLANCE_BT_SALVAR);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  CHECA(glance_botao() == GLANCE_BT_VISTO);
  tecla(SDLK_UP); CHECA(glance_botao() == GLANCE_BT_SALVAR);

  // Direita troca de titulo, devolve o foco ao botao primario e avisa a grade.
  tecla(SDLK_RIGHT);
  CHECA(glance_indice() == 3);
  CHECA(glance_botao() == GLANCE_BT_PRIMARIO);
  CHECA(ultimoFoco == 3);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  CHECA(glance_indice() == 4);            // preso no ultimo
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  CHECA(glance_indice() == 0);            // preso no primeiro
  CHECA(ultimoFoco == 0);

  // O KEYUP do OK que abriu a previa nao faz nada (a tela abriu no toque).
  soltar(SDLK_RETURN);
  CHECA(glance_aberto() && nChamadasDetalhes == 0);

  // OK em Detalhes abre a pagina pelo caminho original e fecha a previa.
  tecla(SDLK_RIGHT); tecla(SDLK_DOWN);
  CHECA(glance_botao() == GLANCE_BT_DETALHES);
  tecla(SDLK_RETURN);
  CHECA(nChamadasDetalhes == 1 && ultimoDetalhes == 1);
  CHECA(!glance_aberto());

  // Voltar fecha sem abrir pagina nenhuma.
  glance_abrir(&fonte, 4);
  tecla(SDLK_ESCAPE);
  CHECA(!glance_aberto());
  CHECA(nChamadasDetalhes == 1);

  // Lista de um so item (Home, Busca, Biblioteca): setas laterais nao saem.
  N = 1;
  glance_abrir(&fonte, 0);
  tecla(SDLK_RIGHT); tecla(SDLK_LEFT);
  CHECA(glance_aberto() && glance_indice() == 0);
  tecla(SDLK_AC_BACK);
  CHECA(!glance_aberto());

  // Fonte vazia ou indice fora: nao abre (a tela cai no caminho de antes).
  N = 0;
  glance_abrir(&fonte, 0);
  CHECA(!glance_aberto());

  // Primary action leaves a one-shot playback intent for the correct title.
  N = 5; glance_abrir(&fonte, 2); tecla(SDLK_RETURN);
  CatItem escolhido, outro;
  fItem(2, &escolhido); fItem(3, &outro);
  CHECA(!glance_pediu_tocar(&outro));
  CHECA(glance_pediu_tocar(&escolhido));
  CHECA(!glance_pediu_tocar(&escolhido));
  // LG Back arrives by scancode with sym=0.
  glance_abrir(&fonte, 1);
  SDL_Event back = {0}; back.type = SDL_KEYDOWN; back.key.keysym.scancode = NV_SCANCODE_BACK;
  glance_evento(&back); CHECA(!glance_aberto());

  if (falhas) { fprintf(stderr, "glance: %d falha(s)\n", falhas); return 1; }
  if (argc > 1) capturas(argv[1]);
  printf("glance: ok\n");
  return 0;
}
