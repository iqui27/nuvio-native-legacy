// CAPTURA DA FILMOGRAFIA DO DIRETOR EM TODAS AS EXIBICOES (job 205-diretorpol).
//
//   timeline.png        Linha do tempo, foco no 1o titulo (sem backdrop,
//                       titulo longo), 0 vistos.
//   timeline-cab.png    Linha do tempo com o foco no seletor de exibicao.
//   coverflow.png       Cover Flow, foco no 3o titulo.
//   ranking.png / decadas.png / vitrine.png  as outras exibicoes.
//
// Inclui src/vertudo.c (os statics so semeiam por dentro), no molde do
// tests/vertudo_colrank_shot.c. Nao entra na suite: precisa de janela GL.
static int shotN;
#define desc_vertudo_n shot_vertudo_n
#define desc_vertudo_item shot_vertudo_item
#include "../src/vertudo.c"
#include "nvtmp.h"
#undef desc_vertudo_n
#undef desc_vertudo_item
int desc_vertudo_n(void);
int desc_vertudo_item(int i, CatItem *dst);
int shot_vertudo_n(void) { return shotN ? shotN : desc_vertudo_n(); }
int shot_vertudo_item(int i, CatItem *dst) {
  static const char *nomes[] = { "Rick Springfield: The Beat of the Live Drum", "Madonna: The Immaculate Collection",
                                 "Alien 3", "Se7en", "The Game", "Fight Club", "Panic Room", "Zodiac",
                                 "The Social Network", "Gone Girl", "Mank", "The Killer" };
  static const int anos[] = { 1985, 1990, 1992, 1995, 1997, 1999, 2002, 2007, 2010, 2014, 2020, 2023 };
  if (!shotN) return desc_vertudo_item(i, dst);
  if (i < 0 || i >= shotN || !dst) return 0;
  memset(dst, 0, sizeof *dst);
  snprintf(dst->imdb, sizeof dst->imdb, "tt%07d", 100 + i);
  snprintf(dst->tipo, sizeof dst->tipo, "movie");
  snprintf(dst->titulo, sizeof dst->titulo, "%s", nomes[i % 12]);
  snprintf(dst->genero, sizeof dst->genero, "Movie · Music · TV Movie");
  snprintf(dst->meta, sizeof dst->meta, "%d · 120 min", anos[i % 12]);
  snprintf(dst->sinopse, sizeof dst->sinopse, "He soared to the heights of stardom in the '80s as a teen idol, "
           "a soap star on General Hospital, and a platinum-selling, Grammy-Award winning music artist.");
  snprintf(dst->poster, sizeof dst->poster, "deploy/app/art/poster/%02d.jpg", i % 40);
  // O 1o titulo nao tem backdrop: o cartao da linha cai no cartaz.
  if (i) snprintf(dst->backdrop, sizeof dst->backdrop, "deploy/app/art/%02d.jpg", i % 30);
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

static void exibicao(int l, int f, int cab, const char *saida, const char *sufixo, SDL_Window *w) {
  char nome[600];
  layEscolha = l; resolverLayout();
  foco = f; tabFocus = cab; tabCursor = cab ? cabN() : 0;
  fluxoPos = tlProg = -1.0f;
  snprintf(nome, sizeof nome, "%s-%s.png", saida, sufixo);
  captura(nome, w);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-diretorpol");
  SDL_Window *w;
  SDL_GLContext gl;
  pasta.sources = pastaFontes;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: filmografia", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | (argc > 2 ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
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

  snprintf(pasta.title, sizeof pasta.title, "David Fincher");
  snprintf(pasta.group, sizeof pasta.group, "Directors");
  snprintf(pastaFontes[0].title, sizeof pastaFontes[0].title, "David Fincher");
  snprintf(pastaFontes[0].type, sizeof pastaFontes[0].type, "movie");
  pasta.nSources = 1;
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0; shotN = 12;

  if (argc > 2) gfx_escala_txt_definir(strtof(argv[2], NULL));
  exibicao(VT_LAY_LINHA, 0, 0, saida, "timeline", w);
  if (argc > 2) goto fim;
  exibicao(VT_LAY_LINHA, 0, 1, saida, "timeline-cab", w);
  exibicao(VT_LAY_FLUXO, 2, 0, saida, "coverflow", w);
  exibicao(VT_LAY_FLUXO, 0, 1, saida, "coverflow-cab", w);
  exibicao(VT_LAY_RANKING, 0, 0, saida, "ranking", w);
  exibicao(VT_LAY_DECADAS, 0, 0, saida, "decadas", w);
  exibicao(VT_LAY_VITRINE, 0, 1, saida, "vitrine", w);

fim:
  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
