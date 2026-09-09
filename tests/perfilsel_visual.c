// CAPTURAS DA TELA DE ESCOLHA DE PERFIL, sem rede e sem conta.
//
// Existe pelo mesmo motivo de tests/player_regression.c --profile: interface de
// TV nao se revisa lendo codigo. A tela e olhada a 3 m, e coisas que so
// aparecem no pixel (nome que estoura a coluna, cinza sobre cinza, foco que nao
// se acha) sao invisiveis numa leitura.
//
// A LISTA VEM DO CACHE EM DISCO, pela API publica: o teste escreve perfis.txt
// numa pasta temporaria e chama perfis_carregar_ativo(). Ou seja, alem de
// desenhar a tela, ele exercita o mesmo caminho de arranque que a TV usa.
//
// As URLs de avatar e de fundo apontam para arte do PACOTE, nao para o Storage
// da conta: nenhum dado de pessoa entra aqui, e as capturas rodam sem rede.
//
//   bash tests/perfilsel.sh --capturas
#include "perfilsel.h"
#include "perfis.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;

static void escreverCache(const char *conteudo, int ativo) {
  char linha[16];
  assert(dados_gravar("perfis.txt", conteudo));
  snprintf(linha, sizeof linha, "%d\n", ativo);
  assert(dados_gravar("perfil.txt", linha));
}

// 90 quadros: tempo de sobra para a mola do foco assentar (120 ms medidos) e
// para o decode das texturas subir para a GPU.
static void captura(const char *nome) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    perfilsel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1); glClear(GL_COLOR_BUFFER_BIT);
    perfilsel_desenhar(SDL_GetTicks());
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
      SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(win); SDL_Delay(4);
  }
  printf("  %s  (preenchimento %.2f telas, %d desenhos)\n", nome, gfx_fill, gfx_n_rect);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  perfilsel_evento(&e);
}

int main(void) {
  const char *arte = "deploy/app/art";
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: revisao da escolha de perfil",
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir(arte);
  // O ARGUMENTO DE dados_iniciar E A PASTA DE ARTE, nao um desvio dos dados: o
  // desvio e a variavel NUVIO_DADOS. Passando a pasta temporaria aqui o teste
  // escrevia em ~/.nuvio — os perfis REAIS de quem roda — e o perfis_esquecer()
  // logo abaixo apagava os arquivos de la. Aconteceu uma vez; esta guarda
  // existe para que nao aconteca duas.
  dados_iniciar(NULL);
  { const char *d = dados_dir();
    const char *tmp = getenv("NUVIO_TESTE_DIR");
    if (!tmp || !*tmp || !d || strcmp(d, tmp)) {
      fprintf(stderr,
              "perfilsel_visual: recusando rodar fora de uma pasta descartavel.\n"
              "  dados_dir()=%s   NUVIO_TESTE_DIR=%s\n"
              "  Rode por tests/perfilsel.sh --capturas, que exporta NUVIO_DADOS.\n",
              d && *d ? d : "(nenhuma)", tmp ? tmp : "(vazia)");
      return 1;
    } }

  // indice \t temPin \t primario \t usaAddons \t cor \t nome \t avatar \t fundo
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n", 1);
  perfis_carregar_ativo();
  perfilsel_iniciar();
  captura("/tmp/nuvio-perfilsel-1.bmp");

  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro\t\t\n", 2);
  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro\t\t\n", 2);
  perfis_carregar_ativo();
  perfilsel_iniciar();
  captura("/tmp/nuvio-perfilsel-2.bmp");

  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t"
                "deploy/app/art/poster/00.jpg\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t1\t#E53935\tÁlvaro Nascimento da Silva\t\t\n"
                "3\t1\t0\t0\t#43A047\tInfantil\t\tdeploy/app/art/07.jpg\n"
                "4\t0\t0\t0\t#8E24AA\tVisitas\t\t\n", 1);
  perfis_carregar_ativo();
  perfilsel_iniciar();
  captura("/tmp/nuvio-perfilsel-4.bmp");

  // O terceiro perfil e o travado: tres DIREITA e a tela do selo de PIN em foco.
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  captura("/tmp/nuvio-perfilsel-4-travado.bmp");

  // OK sobre ele abre o teclado. Depois, quatro digitos e um erro de rede
  // fabricado nao — este e o estado normal de digitacao.
  tecla(SDLK_RETURN);
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP);   // sobe para a linha do "1"
  tecla(SDLK_RETURN);                               // 1
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN);            // 2
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);             // 5
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);             // 8
  captura("/tmp/nuvio-perfilsel-pin.bmp");

  // Oito perfis: o pior caso do layout (CONTA_PERFIL_MAX).
  perfis_esquecer(); dados_iniciar(NULL);
  escreverCache("1\t0\t1\t0\t#1E88E5\tHenrique\t\tdeploy/app/art/03.jpg\n"
                "2\t0\t0\t0\t#E53935\tÁlvaro\t\t\n"
                "3\t1\t0\t0\t#43A047\tInfantil\t\t\n"
                "4\t0\t0\t0\t#8E24AA\tVisitas\t\t\n"
                "5\t0\t0\t0\t#FB8C00\tMariana\t\t\n"
                "6\t0\t0\t0\t#00ACC1\tRoberto\t\t\n"
                "7\t1\t0\t0\t#C0CA33\tCarla\t\t\n"
                "8\t0\t0\t0\t#5E35B1\tPedro\t\t\n", 5);
  perfis_carregar_ativo();
  perfilsel_iniciar();
  captura("/tmp/nuvio-perfilsel-8.bmp");

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas em /tmp/nuvio-perfilsel-*.bmp (dados de teste, sem conta real).");
  return 0;
}
