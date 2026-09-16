// CAPTURAS DO CARTAO DE NOVIDADES DA 1.1, sem interacao e sem rede.
//
// Existe pelo motivo que tests/agenda_shot.c e tests/social_shot.c ja
// registram: interface de TV julgada so por codigo sai ilegivel a 3 m. E aqui
// ha um agravante — sao SETE figuras desenhadas a mao com gfx_*, e cada uma
// pode ter o defeito classico deste repositorio sem que o compilador diga nada:
// o raio do gfx_cor e FRACAO DO MENOR LADO (num retangulo alto, a LARGURA), e
// texto claro sobre superficie clara some.
//
// O que cada foto prova:
//   -pt-0 .. -pt-6   as sete paginas em portugues, com a figura inteira dentro
//                    do cartao e as quatro linhas de recurso sem estourar;
//   -en-0 .. -en-6   as mesmas em ingles. A tabela de traducao pode nao ter as
//                    chaves novas ainda (elas sao entregues a parte, em
//                    i18n-novidades11.txt); para ver o ingles de verdade antes
//                    da fusao, compile com um idioma_tab.h de rascunho a
//                    frente do -Isrc, que e o que o .sh faz quando o arquivo
//                    NUVIO_TAB existe;
//   -reduzido        a pagina 1 com "reduzir animacoes": o despertador nao
//                    treme e a pagina troca SECA — sem deslize e sem meia
//                    transparencia. O estado do lembrete na miniatura NAO pode
//                    depender de animar.
//
// NUVIO_DADOS aponta para uma pasta temporaria e o .c ABORTA se dados_dir() nao
// for exatamente ela: fechar o cartao GRAVA novidades-11.txt, e um teste deste
// repositorio ja escreveu por cima dos dados reais do dono.
#include "novidades11.h"
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

// Ajustes gravados em disco e lidos por ajustes_dir: e o caminho publico para
// escolher idioma e "reduzir animacoes" sem setter de teste. As chaves sao as
// mesmas de ajustes.txt no aparelho.
static void ajustesDeTeste(int idiomaIngles, int animReduzidas) {
  char caminho[600];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  if (!f) return;
  fprintf(f, "idioma %d\nanimacoes %d\n", idiomaIngles, animReduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(32);
    gfx_novo_quadro();
    novidades11_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    novidades11_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
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
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-novidades11";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int p;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao do cartao da 1.1", SDL_WINDOWPOS_CENTERED,
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
  tex_iniciar(160);
  gfx_icones_dir("deploy/app/art");

  dados_iniciar(".");
  { const char *esperado = getenv("NUVIO_DADOS");
    if (!esperado || !esperado[0] || strcmp(dados_dir(), esperado)) {
      printf("FALHA: dados_dir() e [%s], esperado [%s]. Rode por "
             "tests/novidades11_shot.sh.\n",
             dados_dir(), esperado ? esperado : "(vazia)");
      return 1;
    } }

  // --- PORTUGUES: as sete paginas ------------------------------------------
  ajustesDeTeste(0, 0);
  for (p = 0; p < 7; p++) {
    novidades11_abrir(p);
    snprintf(nome, sizeof nome, "%s-pt-%d.bmp", saida, p);
    captura(nome, w);
  }

  // --- INGLES: as mesmas sete ----------------------------------------------
  ajustesDeTeste(1, 0);
  for (p = 0; p < 7; p++) {
    novidades11_abrir(p);
    snprintf(nome, sizeof nome, "%s-en-%d.bmp", saida, p);
    captura(nome, w);
  }

  // --- REDUZIR ANIMACOES ----------------------------------------------------
  // A pagina 1 depois de UMA troca de pagina: com o ajuste ligado o conteudo
  // tem de estar no lugar e OPACO ja no primeiro quadro. Sem ele a captura
  // sairia deslocada 44 px e a 30% de alfa — que e exatamente o que este
  // ajuste existe para evitar.
  ajustesDeTeste(0, 1);
  novidades11_abrir(0);
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_RIGHT;
    novidades11_evento(&e);
    novidades11_evento(&e);
    e.key.keysym.sym = SDLK_LEFT;
    novidades11_evento(&e);
    novidades11_evento(&e); }
  snprintf(nome, sizeof nome, "%s-reduzido.bmp", saida);
  captura(nome, w);

  // O CARTAO FECHA E GRAVA, e a marca tem de ser a NOVA e nao a do Guia: o
  // cartao do Guia de TV continua sendo decidido pelo arquivo dele, e
  // sobrescreve-lo faria esta versao apagar aquele anuncio para quem nunca o
  // viu. Isto e verificacao, nao foto — mas mora aqui porque e a unica vez em
  // que este modulo grava alguma coisa.
  { SDL_Event e;
    char *marca;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_AC_BACK;
    novidades11_evento(&e);
    assert(!novidades11_aberto());
    marca = dados_ler("novidades-11.txt");
    assert(marca);
    free(marca);
    marca = dados_ler("novidades-guia.txt");
    if (marca) { printf("FALHA: o cartao da 1.1 escreveu na marca do Guia.\n");
                 return 1; }
    printf("marca gravada: novidades-11.txt (e so ela)\n"); }

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas do cartao da 1.1 gravadas.");
  return 0;
}
