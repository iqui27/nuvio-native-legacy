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

// Tamanho do quadro capturado. 1920x1080 e o da LG; 1280x720 e o das TVs
// que desenham em 720p (Tizen antigo, LG 2015), onde o layout e o mesmo mas o
// recorte (glScissor) e convertido para pixel de buffer — e onde um erro de
// escala cortaria as notas no lugar errado.
static int capW = 1920, capH = 1080;

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
      unsigned char *pix = malloc((size_t)capW * capH * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, capW, capH, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, capW, capH, 32,
                                         SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < capH; y++)
        memcpy((char *)s->pixels + y * s->pitch,
               pix + (size_t)(capH - 1 - y) * capW * 4, (size_t)capW * 4);
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

// AS NOTAS DE VERDADE da 1.4.5, que sao as que empurraram o botao para fora
// da tela no Tizen. Lidas do repositorio para a captura acompanhar a proxima
// release longa sem ninguem copiar texto para ca.
static char mdLongo[16384];
static void lerNotasLongas(void) {
  FILE *f = fopen("docs/releases/1.4.5/NOTAS.md", "rb");
  size_t n;
  assert(f);
  n = fread(mdLongo, 1, sizeof mdLongo - 1, f);
  mdLongo[n] = 0;
  fclose(f);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  atualizacao_evento(&e);
}

static void semearMd(const char *md, int comoEstado, float pct, const char *passo) {
  snprintf(tagNova, sizeof tagNova, "%s", "1.2.0");
  limparNotas(md, notas, sizeof notas);
  snprintf(ipkUrl, sizeof ipkUrl, "%s",
           "https://github.com/iqui27/nuvio-native-legacy/releases/download/"
           "v1.2.0/space.nuvio.native.legacy_1.2.0_arm.ipk");
  estado = comoEstado;
  instPct = pct;
  snprintf(instPasso, sizeof instPasso, "%s", passo ? passo : "");
  aberto = 1; mostrado = 1; entrada = 1.0f;
  reiniciarVista();
}
static void semear(int comoEstado, float pct, const char *passo) {
  semearMd(MD, comoEstado, pct, passo);
}

// NOTAS LONGAS: o rodape nao pode sair do lugar e a rolagem tem de chegar ao
// fim. `sufixo` distingue 1080p de 720p.
static void longas(SDL_Window *w, GLuint arte, const char *saida, const char *sufixo) {
  char nome[600];
  int i;
  float max;
  semearMd(mdLongo, AT_PARADO, 0.0f, "");
  snprintf(nome, sizeof nome, "%s-longo-topo%s.bmp", saida, sufixo);
  captura(nome, w, arte);
  max = rolarMax();
  // Foco comeca no primario, a janela das notas acaba antes do rodape e o
  // texto da 1.4.5 nao cabe nela (senao este caso nao provaria nada).
  assert(foco == 0);
  assert(max > 0.0f);
  assert(AT_Y + AT_H - AT_RODAPE_H <= AT_Y + AT_H - 96.0f);
  for (i = 0; i < 40; i++) tecla(SDLK_DOWN);
  assert(rolarAlvo == max);
  assert(foco == 0);            // rolar nao mexe no foco dos botoes
  snprintf(nome, sizeof nome, "%s-longo-fim%s.bmp", saida, sufixo);
  captura(nome, w, arte);
  // Chegou ao fim (o desenho reclampa com o vistaH do quadro, dai a folga).
  assert(rolar > max - 1.0f && rolar <= max + 1.0f);
  // SEM INSTALADOR (o caso do Tizen): mesmo rodape fixo, com o endereco.
  semearMd(mdLongo, AT_PARADO, 0.0f, "");
  ipkUrl[0] = 0;
  snprintf(nome, sizeof nome, "%s-longo-tizen%s.bmp", saida, sufixo);
  captura(nome, w, arte);
  for (i = 0; i < 3; i++) tecla(SDLK_UP);
  assert(rolarAlvo == 0.0f);
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

  lerNotasLongas();
  longas(w, arte, saida, "");
  capW = 1280; capH = 720;
  SDL_SetWindowSize(w, capW, capH);
  glViewport(0, 0, capW, capH);
  gfx_tamanho_alvo(capW, capH);
  longas(w, arte, saida, "-720p");

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas do cartao de atualizacao gravadas.");
  return 0;
}
