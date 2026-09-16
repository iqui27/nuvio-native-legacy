// CAPTURA DO CARTAO DE CANAL do Guia, nos DOIS estados, sem rede.
//
// Existe por um pedido do dono (16/09) que so se confere olhando: "tira a
// borda quando nao ta selecionado" e "muda o fundo da logo do canal, deixa so
// branco e preto, e o selecionado tb vai ser um ou outro".
//
// A "borda" era o azulejo de 92x92 pintado numa cor derivada do proprio logo.
// Ele saiu; o cartao passou a ter duas cores e o logo acompanha. Esta captura
// prova as duas metades:
//
//   sem foco:  cartao quase preto, logo BRANCO
//   com foco:  cartao branco,      logo QUASE PRETO
//
// DOIS LOGOS DE MENTIRA, e os dois casos importam:
//   recortado.png  fundo transparente, marca opaca — vai por GFX_MARCA e
//                  inverte junto com o cartao;
//   comfundo.png   quadrado preto opaco com a marca branca (o caso do HBO Max
//                  da foto) — vai por GFX_TEXTO, porque tinta-lo por alfa
//                  cheio desenharia um bloco chapado no lugar da marca.
//
// Inclui src/guia.c: desenharCard e a lista de canais sao estaticos, e semear
// por dentro e o unico jeito de fotografar o cartao sem addon no ar.
#include "../src/guia.c"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static void captura(const char *nome, SDL_Window *win) {
  int i;
  time_t agoraT = time(NULL);
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Os dois estados LADO A LADO, para a comparacao ser uma foto so.
    desenharCard(&canais[0], 120.0f, 120.0f, 0.0f, 1.0f, agoraT);
    desenharCard(&canais[1], 120.0f + G_CARD_W + 40.0f, 120.0f, 1.0f, 1.0f, agoraT);
    desenharCard(&canais[2], 120.0f, 120.0f + G_CARD_H + 40.0f, 0.0f, 1.0f, agoraT);
    desenharCard(&canais[3], 120.0f + G_CARD_W + 40.0f,
                 120.0f + G_CARD_H + 40.0f, 1.0f, 1.0f, agoraT);
    { TxtLinha l = txt_linha(TXT_CAPTION,
        "esquerda: sem foco   direita: com foco   "
        "linha 1: logo recortado   linha 2: logo com fundo proprio",
        170, 172, 180, 255);
      txt_desenhar(l, 120.0f, 120.0f + (G_CARD_H + 40.0f) * 2.0f + 10.0f); }
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

static void poeCanal(int i, const char *nome, const char *logo) {
  snprintf(canais[i].id, sizeof canais[i].id, "c%d", i);
  snprintf(canais[i].nome, sizeof canais[i].nome, "%s", nome);
  snprintf(canais[i].logo, sizeof canais[i].logo, "%s", logo);
  canais[i].cat = 0;
  canais[i].epg = -2;      // sem grade real: o cartao mostra "AO VIVO"
  canais[i].fav = i == 1;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-guia";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: cartao de canal", SDL_WINDOWPOS_CENTERED,
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

  poeCanal(0, "Canal Recortado HD", "tests/fixtures/logos/recortado.png");
  poeCanal(1, "Canal Recortado HD", "tests/fixtures/logos/recortado.png");
  poeCanal(2, "Canal Com Fundo HD", "tests/fixtures/logos/comfundo.png");
  poeCanal(3, "Canal Com Fundo HD", "tests/fixtures/logos/comfundo.png");
  nCanais = 4;
  snprintf(cats[0], sizeof cats[0], "%s", "Aberta");
  nCats = 1; catIni[0] = 0; catN[0] = 4;

  snprintf(nome, sizeof nome, "%s-cartoes.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas do cartao de canal gravadas.");
  return 0;
}
