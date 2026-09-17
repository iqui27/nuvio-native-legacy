// CAPTURA DO CARTAO DE ATUALIZACAO, nos quatro estados que importam.
//
// Existe porque este cartao e o unico da interface que NAO DA PARA FOTOGRAFAR
// usando o app: ele so abre quando ha no GitHub uma versao mais nova que a
// instalada, e o ramo com botoes e barra so existe onde ha o Homebrew Channel.
// Fotografar de verdade exigiria segurar uma release, uma TV rooteada e o
// momento certo ao mesmo tempo — foi por isso que a imagem 20-update.png ficou
// faltando no album do segundo post por dois dias.
//
// Inclui src/atualizacao.c: tagNova, notas, estado e a barra sao estaticos, e
// semear por dentro e o unico jeito de encenar os quatro casos sem rede.
#include "../src/atualizacao.c"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

// O CHAO IMPORTA, e isto ja custou duas rodadas de polimento nesta base: o
// cartao vive SOBRE A HOME, e um harness que limpa a tela para preto valida um
// desenho que na TV aparece sobre arte. Aqui a home nao esta montada, entao o
// mais honesto que da para fazer e a arte de fundo com o mesmo veu que a home
// usa atras de um cartao modal.
static void chaoDeHome(GLuint arte) {
  if (arte) {
    gfx_tex_aspect_atual = 0.0f;
    gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, arte, GFX_FUNDO,
             0.0f, 0, 0, 0.0f, 1, 1, 1, 1);
  }
}

static void captura(const char *nome, SDL_Window *win, GLuint arte) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    chaoDeHome(arte);
    atualizacao_atualizar(1.0f / 60.0f, SDL_GetTicks());
    atualizacao_desenhar(SDL_GetTicks());
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32,
                                         SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch,
               pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

// As notas vem da release do GitHub em Markdown e passam por limparNotas antes
// de chegar na tela. Semear o MARKDOWN CRU, e nao o texto ja limpo, e o que faz
// a captura provar tambem o cortador de secao e o de marcacao.
static const char MD[] =
  "## Added\n"
  "- Schedule screen: every show you follow on a time axis, with reminders\n"
  "- Lists in the Library: Trakt, Simkl and your Nuvio collections\n"
  "- Audience charts and famous quotes on a title's page\n"
  "\n"
  "## Fixed\n"
  "- Focus is filled, never outlined, across the whole interface\n"
  "- Channel logos no longer draw a black box around the mark\n"
  "\n"
  "## Notes\n"
  "Install with the Homebrew Channel or Developer Mode.\n";

static void semear(int comoEstado, float pct, const char *passo) {
  snprintf(tagNova, sizeof tagNova, "%s", "1.2.0");
  limparNotas(MD, notas, sizeof notas);
  snprintf(ipkUrl, sizeof ipkUrl, "%s",
           "https://github.com/iqui27/nuvio-native-legacy/releases/download/"
           "v1.2.0/space.nuvio.native.legacy_1.2.0_arm.ipk");
  estado = comoEstado;
  instPct = pct;
  snprintf(instPasso, sizeof instPasso, "%s", passo ? passo : "");
  aberto = 1; mostrado = 1; entrada = 1.0f; foco = 0;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-update";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  GLuint arte;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: cartao de atualizacao", SDL_WINDOWPOS_CENTERED,
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
  mtx = SDL_CreateMutex();

  arte = tex_obter("deploy/app/art/00.jpg");
  { int i; for (i = 0; i < 40 && !arte; i++) { tex_bombear(6);
      arte = tex_obter("deploy/app/art/00.jpg"); SDL_Delay(16); } }

  semear(AT_PARADO, 0.0f, "");
  snprintf(nome, sizeof nome, "%s-aviso.bmp", saida);
  captura(nome, w, arte);

  foco = 1;   // "Depois"
  snprintf(nome, sizeof nome, "%s-depois.bmp", saida);
  captura(nome, w, arte);

  semear(AT_INSTALANDO, 62.0f, "Downloading");
  snprintf(nome, sizeof nome, "%s-baixando.bmp", saida);
  captura(nome, w, arte);

  semear(AT_PRONTO, 100.0f, "");
  snprintf(nome, sizeof nome, "%s-pronto.bmp", saida);
  captura(nome, w, arte);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas do cartao de atualizacao gravadas.");
  return 0;
}
