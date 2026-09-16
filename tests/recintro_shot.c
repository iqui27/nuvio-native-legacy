// CAPTURA DAS TRES PAGINAS do cartao de primeira vez do Social (recintro.c),
// sem interacao e sem rede.
//
// Existe pelo motivo que tests/ajustes_shot.c ja registra: interface de TV
// julgada so por codigo sai ilegivel a 3 m. Aqui cada pagina e desenhada e
// gravada em BMP para ser OLHADA — e duas pedras deste recurso so apareceram
// olhando: o raio do gfx_cor e FRACAO DA ALTURA (nao pixel) e um anel branco
// sobre pilula clara e invisivel.
//
// -DNV_REC_URL pelo include: o cartao SO EXISTE com o servico compilado
// (recomenda_ativo). Sem a bandeira ele nao abriria, que e o comportamento
// certo do pacote sem servico e nao o que esta foto quer provar. Mesma manobra
// de tests/social_shot.c — src/recomenda.c sai da lista do .sh e entra aqui.
#define NV_REC_URL "http://127.0.0.1:8799"
#include "../src/recomenda.c"
#include "recintro.h"
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

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  recintro_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    recintro_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    recintro_desenhar(SDL_GetTicks());
    if (i == 89) {
      unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32,
                                         SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
               1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-recintro";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;

  // A MESMA TRAVA de tests/social_shot.c, e pelo mesmo motivo: este teste
  // ESCREVE ajustes.txt e, ao fechar o cartao, escreveria a marca. Sem a
  // conferencia ele mexeria na pasta de verdade de quem o executa.
  if (!dir || !dir[0]) {
    printf("recintro_shot: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("recintro_shot: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando\n",
           dados_dir(), dir);
    return 2;
  }

  // IDIOMA DO DISCO, pelo caminho real. O padrao de fabrica e o INGLES
  // (ajustes.c) e quem revisa estas capturas le portugues. "selected_theme 2" e
  // o acento OCEANO: os pontos das paginas e os aneis das miniaturas tem de
  // sair AZUIS, provando que vieram de ajustes_acento e nao de branco cravado.
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
    f = fopen(caminho, "w");
    assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\n");
    fclose(f);
    ajustes_dir(dados_dir()); }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: recintro", SDL_WINDOWPOS_CENTERED,
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

  assert(recomenda_ativo());
  recintro_primeira_vez();
  assert(recintro_aberto());

  snprintf(nome, sizeof nome, "%s-1.bmp", saida);
  captura(nome, w);

  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-2.bmp", saida);
  captura(nome, w);

  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-3.bmp", saida);
  captura(nome, w);

  // A DIREITA NA ULTIMA NAO FAZ NADA e o OK fecha — as duas regras valem a
  // conferencia, porque um cartao de primeira vez que nao fecha e um app
  // travado na primeira abertura.
  tecla(SDLK_RIGHT);
  assert(recintro_aberto());
  tecla(SDLK_RETURN);
  assert(!recintro_aberto());
  { char *m = dados_ler("recintro-social.txt");
    assert(m);
    free(m); }

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: tres paginas gravadas, OK fecha e a marca foi escrita.");
  return 0;
}
