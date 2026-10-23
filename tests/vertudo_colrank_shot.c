// CAPTURA DO RANKING DA COLECAO E DO TOPO DA GRADE FOCADA (job 205-colrank).
//
//   ranking.png   exibicao Ranking: o painel da direita NAO repete o cartaz
//                 (o cartaz ja e a celula) e a logo vale la.
//   grade-topo.png Grade com o foco na 1a linha: o cartaz crescido/anelado
//                 inteiro abaixo do cabecalho, sem corte no topo.
//
// Inclui src/vertudo.c (os statics so semeiam por dentro), no molde do
// tests/vertudo_shot.c. Nao entra na suite (tools/testa-tudo.sh pula
// *_shot.sh): precisa de janela GL e de olho humano.
static int shotGrade;
#define desc_vertudo_n shot_vertudo_n
#define desc_vertudo_item shot_vertudo_item
#include "../src/vertudo.c"
#include "nvtmp.h"
#undef desc_vertudo_n
#undef desc_vertudo_item
int desc_vertudo_n(void);
int desc_vertudo_item(int i, CatItem *dst);
int shot_vertudo_n(void) { return shotGrade ? shotGrade : desc_vertudo_n(); }
int shot_vertudo_item(int i, CatItem *dst) {
  if (!shotGrade) return desc_vertudo_item(i, dst);
  if (i < 0 || i >= shotGrade || !dst) return 0;
  memset(dst, 0, sizeof *dst);
  snprintf(dst->imdb, sizeof dst->imdb, "tt%07d", 100 + i);
  snprintf(dst->tipo, sizeof dst->tipo, "movie");
  snprintf(dst->titulo, sizeof dst->titulo, "Titulo %d", i + 1);
  snprintf(dst->genero, sizeof dst->genero, "Filme · Drama");
  snprintf(dst->meta, sizeof dst->meta, "%d · 120 min", 1992 + i * 3);
  snprintf(dst->sinopse, sizeof dst->sinopse, "Sinopse de exemplo para o painel da direita.");
  snprintf(dst->poster, sizeof dst->poster, "deploy/app/art/poster/%02d.jpg", i % 40);
  snprintf(dst->backdrop, sizeof dst->backdrop, "deploy/app/art/%02d.jpg", i % 30);
  snprintf(dst->logo, sizeof dst->logo, "deploy/app/art/logo/%02d.png", i % 40);
  return 1;
}
#include <SDL2/SDL_image.h>
#include <assert.h>

static ColFolder pasta;
static ColSource pastaFontes[COL_SOURCE_MAX];

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    vertudo_atualizar(1.0f / 60.0f, SDL_GetTicks());
    vertudo_desenhar(SDL_GetTicks());
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-colrank");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  pasta.sources = pastaFontes;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: ranking da colecao", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  snprintf(pasta.title, sizeof pasta.title, "A24");
  snprintf(pasta.group, sizeof pasta.group, "Collections");
  snprintf(pastaFontes[0].title, sizeof pastaFontes[0].title, "A24");
  snprintf(pastaFontes[0].type, sizeof pastaFontes[0].type, "movie");
  pasta.nSources = 1;
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0; shotGrade = 12;

  layEscolha = VT_LAY_RANKING; resolverLayout();
  snprintf(nome, sizeof nome, "%s-ranking.png", saida);
  captura(nome, w);

  layEscolha = VT_LAY_GRADE; resolverLayout();
  snprintf(nome, sizeof nome, "%s-grade-topo.png", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
