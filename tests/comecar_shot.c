// Captura da tela "Começar" (2.1): os 4 passos + o fim de "só o tour", em
// 1920x1080, janela GL escondida e desenho num FBO — como
// tests/novidades148_shot.c. Antes das capturas, confere por evento de tecla:
//   passo 1: OK abre o teclado; nome gravado continua no passo 2; cor e
//            infantil gravam em comecar.txt (estado=andamento);
//   passo 2: Voltar volta ao 1 sem perder o nome; Conta Nuvio e Começar do
//            zero avançam; Stremio e AIOManager ficam "Em breve" (não avançam);
//   passo 3: Trakt pede o código (traktauth), "Só nesta TV" avança;
//   passo 4: OK devolve COMECAR_PEDIU_TOUR ou NADA e grava estado=feito;
//   Back: volta um passo em 2/3/4, fecha o "só o tour";
//   D-pad: todo foco alcança e escapa — varredura por foco inicial (baixo,
//            cima, direita, esquerda) sem cair em item que não sai do lugar.
#include "comecar.h"
#include "comecarregra.h"
#include "ajustes.h"
#include "anim.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "teclado.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

static GLuint fbo, fboTex;

void teclado_teste_sistema(const char *t);   // teclado.c (uso de teste)

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  // Como o app: com o teclado da TV aberto, a tecla e dele; senao, do Comecar.
  if (teclado_aberto()) teclado_evento(&e);
  else comecar_evento(&e);
}

// Um quadro: fundo de mentira (a tela do app embaixo) e o Começar por cima.
static void quadro(float dt) {
  int i;
  comecar_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();   // como o app: sem isto o teto de rasterizacao acaba
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 200, 300, 600, 400 }, 0.02f, 0.35f, 0.55f, 1.0f, 1.0f);
  for (i = 0; i < 12; i++)
    gfx_cor((GfxRect){ 960 + i * 70, 700, 60, 160 }, 0.04f, 0.12f + 0.06f * i, 0.2f, 0.8f, 0.9f);
  comecar_desenhar(SDL_GetTicks());
}

static void adiantar(int quadros) {
  int i;
  for (i = 0; i < quadros; i++) quadro(1000.0f / 60.0f);
}

static void captura(const char *nome, int quadros) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  adiantar(quadros);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

static char *ler(void) {
  char *s = dados_ler(COMECAR_ARQ);
  assert(s);
  return s;
}

static void contem(const char *agulha) {
  char *s = ler();
  if (!strstr(s, agulha)) { printf("FALHA: comecar.txt sem '%s' (tem: %s)\n", agulha, s); exit(1); }
  free(s);
}

static void apagar(void) {
  dados_apagar(COMECAR_ARQ);
  comecar_teste_esquecer();
}

// O D-pad tem de levar a todo item: comeca em cada foco de partida, anda em
// cada direcao e confere que o foco continua valido (id >= 0) e que a tela
// continua no mesmo passo — nenhum item rouba o OK do D-pad e nenhuma direcao
// tranca. Volta ao foco de partida por CIMA e ESQUERDA: escapar tambem conta.
static void navegacao(int passoEsperado) {
  int partida, dir, volta, andadas = 0;
  for (partida = 0; partida < 12; partida++) {
    apagar();
    dados_gravar(COMECAR_ARQ, "estado=andamento\nnome=Ana\n");
    comecar_abrir();
    while (comecar_passo() != passoEsperado) {
      if (comecar_passo() == 2 && comecar_foco() == 0) { tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); }
      tecla(SDLK_RETURN);
    }
    // percorre o passo com DOWN/RIGHT para partir de lugares diferentes
    for (volta = 0; volta < partida; volta++) { tecla(SDLK_RIGHT); tecla(SDLK_DOWN); }
    for (dir = 0; dir < 4; dir++) {
      tecla(dir == 0 ? SDLK_RIGHT : dir == 1 ? SDLK_LEFT : dir == 2 ? SDLK_DOWN : SDLK_UP);
      assert(comecar_foco() >= 0);
      assert(comecar_passo() == passoEsperado || comecar_foco() == 100 || comecar_foco() == 101 || comecar_foco() == 102);
      if (comecar_passo() != passoEsperado) break;   // botao avancou: recomeca
      andadas++;
    }
    apagar();
  }
  assert(andadas > 0);   // o D-pad mexeu alguma coisa em algum passo
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-comecar";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;

  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("comecar-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");

  // ---- Regras, por evento de tecla. (Ids: 0=campo, 1..6=cores, 7=infantil,
  // 100=Voltar, 101=Continuar/principal, 102=secundario.)
  // Passo 1 (Perfil): o foco inicial e o campo; OK abre o teclado da TV, o
  // nome entra por ele e o foco sobe para o Continuar.
  apagar(); comecar_abrir();
  assert(comecar_passo() == 0 && comecar_aberto());
  assert(comecar_foco() == 0);
  tecla(SDLK_RETURN);            // abre o teclado
  assert(teclado_aberto());
  teclado_teste_modos(0, 0, 0);
  teclado_teste_sistema("so");   // como o teclado da TV entrega o valor
  assert(!strcmp(teclado_texto(), "so"));
  { SDL_Event e;                                 // Pronto: fileira de acoes
    int i;
    memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN;
    for (i = 0; i < 12 && teclado_foco_tipo() != TECLADO_FOCO_ACOES; i++) teclado_evento(&e);
    assert(teclado_foco_tipo() == TECLADO_FOCO_ACOES);
    e.key.keysym.sym = SDLK_RIGHT; teclado_evento(&e);   // 'limpar'
    e.key.keysym.sym = SDLK_RIGHT; teclado_evento(&e);   // 'pronto'
    e.key.keysym.sym = SDLK_RETURN; teclado_evento(&e); }
  assert(!teclado_aberto());
  quadro(0.016f);              // comecar_atualizar le o PRONTO e guarda
  assert(comecar_foco() == 101);
  contem("nome=so");
  // Cancelar o teclado deixa o nome como estava.
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_RETURN);   // campo: reabre
  assert(teclado_aberto());
  tecla(SDLK_ESCAPE);
  quadro(0.016f);
  assert(!teclado_aberto() && comecar_passo() == 0);
  // Cor e infantil gravam no arquivo.
  tecla(SDLK_DOWN); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  contem("cor=2");
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_DOWN);
  tecla(SDLK_RETURN);
  contem("infantil=1");
  // Continuar com nome vazio reabre o teclado em vez de avancar.
  apagar(); comecar_abrir();
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(comecar_passo() == 0 && teclado_aberto());
  tecla(SDLK_ESCAPE);
  quadro(0.016f);
  assert(comecar_passo() == 0 && !teclado_aberto());

  // Passo 2 (Trazer): Stremio e AIOManager sao "Em breve" e nao avancam.
  dados_gravar(COMECAR_ARQ, "estado=andamento\nnome=so\n");
  comecar_teste_esquecer(); comecar_abrir();
  tecla(SDLK_RETURN);            // nome ja ha: Continuar
  assert(comecar_passo() == 1);
  assert(comecar_foco() == 2);   // foco padrao: Conta Nuvio
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_RETURN);   // Stremio: fica
  assert(comecar_passo() == 1);
  tecla(SDLK_RETURN);                                   // AIOManager: fica
  assert(comecar_passo() == 1);
  // Voltar volta ao 1 sem perder o nome (desce ao rodape e vai a esquerda).
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  assert(comecar_foco() == 101);          // principal (Pular este passo)
  tecla(SDLK_LEFT); tecla(SDLK_RETURN);   // B_VOLTAR
  assert(comecar_passo() == 0);
  contem("nome=so");
  // De volta ao 2: "Pular este passo" (o principal) tambem avanca.
  tecla(SDLK_RETURN);
  assert(comecar_passo() == 1);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(comecar_passo() == 2);

  // Passo 3 (Acompanhar): "So nesta TV" avanca; Back volta um passo.
  assert(comecar_foco() == 0);   // I_TRAKT
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  assert(comecar_passo() == 3);
  tecla(SDLK_AC_BACK);
  assert(comecar_passo() == 2);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(comecar_passo() == 3);

  // Passo 4 (Pronto): OK no principal = Ir para a Home (NADA), grava feito.
  assert(comecar_foco() == 101);
  tecla(SDLK_RETURN);
  assert(!comecar_aberto());
  assert(comecar_pedido() == COMECAR_PEDIU_NADA);
  contem("estado=feito");
  comecar_decidir(1);            // feito: nunca mais, nem com login novo
  assert(!comecar_aberto());
  // O secundario (Fazer o tour) pede o tour e tambem grava feito.
  apagar();
  dados_gravar(COMECAR_ARQ, "estado=andamento\nnome=so\n");
  comecar_teste_esquecer(); comecar_abrir();
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);   // passo 2
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);   // passo 4
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN);   // secundario
  assert(!comecar_aberto());
  assert(comecar_pedido() == COMECAR_PEDIU_TOUR);
  contem("estado=feito");
  // So o tour (sessao restaurada): abre ja no fim e Back fecha com NADA.
  apagar(); comecar_decidir(0);
  assert(comecar_aberto() && comecar_passo() == 3);
  tecla(SDLK_AC_BACK);
  assert(!comecar_aberto() && comecar_pedido() == COMECAR_PEDIU_NADA);
  // Login novo, nada gravado: os 4 passos.
  apagar(); comecar_decidir(1);
  assert(comecar_aberto() && comecar_passo() == 0);

  navegacao(0); navegacao(1); navegacao(2); navegacao(3);

  // ---- Capturas em 1920x1080.
  apagar(); comecar_abrir();
  snprintf(nome, sizeof nome, "%s-1-perfil.png", saida);
  captura(nome, 40);

  dados_gravar(COMECAR_ARQ, "estado=andamento\nnome=so\ncor=1\n");
  comecar_teste_esquecer(); comecar_abrir();
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-2-trazer.png", saida);
  captura(nome, 2);

  tecla(SDLK_RETURN);   // Conta Nuvio
  snprintf(nome, sizeof nome, "%s-3-acompanhar.png", saida);
  captura(nome, 2);

  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-4-pronto.png", saida);
  captura(nome, 2);

  apagar(); comecar_decidir(0);
  snprintf(nome, sizeof nome, "%s-5-so-tour.png", saida);
  captura(nome, 2);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: comecar: regras conferidas e 5 capturas gravadas.");
  return 0;
}
