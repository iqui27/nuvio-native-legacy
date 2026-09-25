// CAPTURA DAS ABAS DA PAGINA DE COLECAO, nos dois estados, sem rede.
//
// Pedido do dono (19/09/2026): "vamos melhorar essas abas que nao estao
// legais, deixar mais parecido com o restante do app". As abas eram pilulas
// de 304x58 com anel, sublinhado e escala no foco — tres sinais que o resto do
// app ja abandonou (NV_COR_FOCO em layout.h; abas do painel de Salvos). Esta
// captura prova a gramatica nova:
//
//   com o D-pad nas abas:  a aba do cursor preenchida na cor de realce com
//                          texto escuro, a aberta em superficie clara;
//   com o D-pad na grade:  so a aberta clara, as outras quase transparentes.
//
// Inclui src/vertudo.c: `collection`, `tabFocus`, `tabCursor` e `source` sao
// estaticos, e semear por dentro e o unico jeito de fotografar sem addon.
#include "../src/vertudo.c"
#include "rail_shot.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static ColFolder pasta;

static void captura(const char *nome, SDL_Window *win) {
  int i;
  rail_shot_aplicar();
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
    rail_shot_desenhar(MENU_INICIO);
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
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

static void fonte(int i, const char *titulo, const char *tipo) {
  ColSource *s = &pasta.sources[i];
  snprintf(s->title, sizeof s->title, "%s", titulo);
  snprintf(s->type, sizeof s->type, "%s", tipo);
  snprintf(s->base, sizeof s->base, "https://exemplo.invalid/%d", i);
  snprintf(s->catId, sizeof s->catId, "cat%d", i);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-vertudo";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: abas da colecao", SDL_WINDOWPOS_CENTERED,
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

  snprintf(pasta.title, sizeof pasta.title, "Netflix");
  snprintf(pasta.group, sizeof pasta.group, "Streaming");
  fonte(0, "Netflix", "movie");
  fonte(1, "Netflix", "series");
  fonte(2, "Netflix Top 10", "movie");
  fonte(3, "Netflix Top 10", "series");
  fonte(4, "Netflix Kids Top 10", "movie");
  fonte(5, "Netflix Kids Top 10", "series");
  fonte(6, "Latest Netflix", "movie");
  pasta.nSources = 7;
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0;

  tabFocus = 1; tabCursor = 1;
  snprintf(nome, sizeof nome, "%s-abas-foco.bmp", saida);
  captura(nome, w);

  tabFocus = 0; tabCursor = 0;
  snprintf(nome, sizeof nome, "%s-abas-grade.bmp", saida);
  captura(nome, w);

  // Cursor no fim: a faixa rola para a ultima aba entrar inteira.
  tabFocus = 1; tabCursor = 6;
  snprintf(nome, sizeof nome, "%s-abas-fim.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
