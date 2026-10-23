// Guia da 2.0 sem janela visivel (FBO, como tests/novidades180_shot.c).
// Primeiro as regras por tecla (lista, modo essencial, estados da cena, AZUL,
// Voltar e o dialogo, a marca de "visto", o aparelho); depois as capturas:
//   NUVIO_SHOT_VIDRO=1 (padrao) vidro, =0 solido.
// Saida: <saida>-<id>.png.
#include "novidades20.h"
#include "nvtmp.h"
#include "novidadesfila.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "idiomacod.h"
#include "layout.h"
#include "logoapp.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static int vidro = 1;

static void ajustesDeTeste(int idioma) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme 2\nanimacoes 0\nvidroLocal %d\n", idioma, vidro ? 0 : 1);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void teclaSc(SDL_Keycode k, int sc) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.keysym.scancode = (SDL_Scancode)sc;
  novidades20_evento(&e);
}
static void tecla(SDL_Keycode k) { teclaSc(k, 0); }

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

static void regras(void) {
  int i, n;
  // Primeira vez: abre, grava a marca do guia e a da 1.8.0; uma segunda
  // chamada (outro arranque) nao reabre.
  dados_apagar(N20_ARQ);
  dados_apagar(NF_ARQ_180);
  novidades20_primeira_vez();
  assert(novidades20_aberto());
  assert(existe(N20_ARQ) && existe(NF_ARQ_180));
  // Guia completo: hero + 11 capitulos + resumo + fim.
  assert(novidades20_telas() == 14 && novidades20_tela_tipo(0) == N20_HERO);
  assert(novidades20_tela_tipo(12) == N20_RESUMO && novidades20_tela_tipo(13) == N20_FIM);
  // Hero: cima/baixo trocam o botao (com volta).
  assert(novidades20_foco() == 0);
  tecla(SDLK_DOWN); assert(novidades20_foco() == 1);
  tecla(SDLK_UP); tecla(SDLK_UP); assert(novidades20_foco() == 2);
  tecla(SDLK_DOWN); assert(novidades20_foco() == 0);
  // "Só o que muda pra mim": esconde Servidores e Plugins/P2P.
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(novidades20_essencial() && novidades20_telas() == 12 && novidades20_tela() == 1);
  for (i = 0; i < novidades20_telas(); i++) {
    int t = novidades20_tela_tipo(i);
    assert(t != 8 && t != 9);
  }
  // OK troca o estado da cena, em volta.
  n = novidades20_estados(0);
  assert(n == 3);
  tecla(SDLK_RETURN); assert(novidades20_estado(0) == 1);
  tecla(SDLK_RETURN); tecla(SDLK_RETURN); assert(novidades20_estado(0) == 0);
  // Direita/esquerda: capitulos.
  tecla(SDLK_RIGHT); assert(novidades20_tela_tipo(novidades20_tela()) == 1);
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); assert(novidades20_tela() == 0);
  // AZUL pula para o resumo; no resumo, volta ao inicio.
  tecla(SDLK_RIGHT);
  teclaSc(SDLK_UNKNOWN, NV_SCANCODE_BLUE);
  assert(novidades20_tela_tipo(novidades20_tela()) == N20_RESUMO);
  tecla(SDLK_s);
  assert(novidades20_tela() == 0);
  // Voltar abre "Sair do guia?"; Voltar de novo = Continuar.
  tecla(SDLK_RIGHT);
  tecla(SDLK_ESCAPE); assert(novidades20_saindo() && novidades20_foco() == 0);
  tecla(SDLK_ESCAPE); assert(!novidades20_saindo() && novidades20_aberto());
  // Ver depois fecha (a marca continua) e nao pede nada.
  tecla(SDLK_ESCAPE); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  assert(!novidades20_aberto() && novidades20_pedido() == N20_PEDIU_NADA);
  novidades20_primeira_vez();
  assert(!novidades20_aberto());
  // Instalacao nova: a tela final oferece o Guia de uso primeiro.
  novidades20_abrir(1);
  teclaSc(SDLK_UNKNOWN, NV_SCANCODE_BLUE);
  tecla(SDLK_RIGHT);
  assert(novidades20_tela_tipo(novidades20_tela()) == N20_FIM && novidades20_foco() == 0);
  tecla(SDLK_RETURN);
  assert(!novidades20_aberto() && novidades20_pedido() == N20_PEDIU_GUIA);
  // Atualizacao: Concluir e o primeiro.
  novidades20_abrir(0);
  teclaSc(SDLK_UNKNOWN, NV_SCANCODE_BLUE); tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  assert(!novidades20_aberto() && novidades20_pedido() == N20_PEDIU_NADA);
  // Aparelho: AutoSync pelo audio (cap 4, "Também" 0) so no Android; zoom do
  // trailer (cap 3, "Também" 4) so no .tpk; Plugins/P2P fora do .wgt.
  novidades20_aparelho(N20_DEV_ANDROID);
  assert(novidades20_disponivel(4, 3) && !novidades20_disponivel(3, 7) && novidades20_disponivel(9, 0));
  novidades20_aparelho(N20_DEV_TPK);
  assert(!novidades20_disponivel(4, 3) && novidades20_disponivel(3, 7) && novidades20_disponivel(9, 1));
  novidades20_aparelho(N20_DEV_WGT);
  assert(!novidades20_disponivel(9, 0) && !novidades20_disponivel(9, 1) && novidades20_disponivel(9, 2));
  novidades20_aparelho(N20_DEV_LG);
  assert(!novidades20_disponivel(5, 7) && novidades20_disponivel(9, 0));
  novidades20_aparelho(-1);
  puts("PASS: regras do guia da 2.0");
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades20_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.04f, 0.043f, 0.055f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  novidades20_desenhar(SDL_GetTicks());
  glFinish();
}

static void gravar(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) {
    int x;
    unsigned char *d = (unsigned char *)s->pixels + y * s->pitch;
    memcpy(d, pix + (1079 - y) * 1920 * 4, 1920 * 4);
    for (x = 0; x < 1920; x++) d[x * 4 + 3] = 255;
  }
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

// Abre, aplica as teclas (R/L/U/D, O = OK, B = Azul, V = Voltar), espera e grava.
static void captura(const char *saida, const char *id, int dev, int novo, const char *teclas) {
  char nome[800];
  int i;
  novidades20_aparelho(dev);
  novidades20_abrir(novo);
  for (; teclas && *teclas; teclas++) {
    char c = *teclas;
    if (c == 'B') teclaSc(SDLK_UNKNOWN, NV_SCANCODE_BLUE);
    else tecla(c == 'L' ? SDLK_LEFT : c == 'R' ? SDLK_RIGHT : c == 'U' ? SDLK_UP : c == 'D' ? SDLK_DOWN :
               c == 'V' ? SDLK_ESCAPE : SDLK_RETURN);
  }
  for (i = 0; i < 200; i++) { quadro(1.0f / 30.0f); SDL_Delay(3); }
  snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
  gravar(nome);
  tecla(SDLK_ESCAPE); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  for (i = 0; i < 20; i++) quadro(1.0f / 30.0f);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-novidades20");
  const char *dir = getenv("NUVIO_DADOS");
  const char *so = getenv("N20_SO");
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  if (getenv("NUVIO_SHOT_VIDRO")) vidro = atoi(getenv("NUVIO_SHOT_VIDRO"));
  dados_iniciar(dir);
  novidades20_dir("deploy/app/art");
  logoapp_iniciar("deploy/app/art");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades20-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  tex_iniciar(160);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");
  ajustesDeTeste(IDIOMA_PT);
  regras();

  { static const struct { const char *id; int dev, novo; const char *teclas; } Q[] = {
      { "hero", N20_DEV_LG, 0, "" },
      { "hero-novo", N20_DEV_ANDROID, 1, "D" },
      { "cap01-visual", N20_DEV_LG, 0, "R" },
      { "cap01-visual-solido", N20_DEV_LG, 0, "RO" },
      { "cap02-descanso", N20_DEV_LG, 0, "RR" },
      { "cap02-descanso-relogio", N20_DEV_LG, 0, "RRO" },
      { "cap02-descanso-escurecer", N20_DEV_LG, 0, "RROO" },
      { "cap03-home", N20_DEV_LG, 0, "RRR" },
      { "cap03-home-padrao", N20_DEV_LG, 0, "RRRO" },
      { "cap04-titulo-serie", N20_DEV_LG, 0, "RRRRO" },
      { "cap05-player-lg", N20_DEV_LG, 0, "RRRRR" },
      { "cap05-player-autosync", N20_DEV_ANDROID, 0, "RRRRRO" },
      { "cap06-fontes", N20_DEV_LG, 0, "RRRRRR" },
      { "cap06-memoria", N20_DEV_LG, 0, "RRRRRRO" },
      { "cap07-social", N20_DEV_LG, 0, "RRRRRRR" },
      { "cap07-social-enquete", N20_DEV_LG, 0, "RRRRRRRO" },
      { "cap08-perfis", N20_DEV_LG, 0, "RRRRRRRR" },
      { "cap08-perfis-pin", N20_DEV_LG, 0, "RRRRRRRROO" },
      { "cap09-servidores", N20_DEV_LG, 0, "RRRRRRRRR" },
      { "cap10-plugins-wgt", N20_DEV_WGT, 0, "RRRRRRRRRR" },
      { "cap10-p2p-tpk", N20_DEV_TPK, 0, "RRRRRRRRRRO" },
      { "cap11-ajustes", N20_DEV_LG, 0, "RRRRRRRRRRR" },
      { "cap11-ajustes-avancado", N20_DEV_LG, 0, "RRRRRRRRRRRO" },
      { "resumo-lg", N20_DEV_LG, 0, "RB" },
      { "resumo-essencial-android", N20_DEV_ANDROID, 0, "DOB" },
      { "fim-novo", N20_DEV_LG, 1, "RBR" },
      { "fim", N20_DEV_LG, 0, "RBR" },
      { "sair", N20_DEV_LG, 0, "RRRV" },
    };
    int i;
    for (i = 0; i < (int)(sizeof Q / sizeof *Q); i++) {
      if (so && !strstr(Q[i].id, so)) continue;
      captura(saida, Q[i].id, Q[i].dev, Q[i].novo, Q[i].teclas);
    } }
  if (getenv("N20_EN")) {
    ajustesDeTeste(IDIOMA_EN);
    captura(saida, "en-cap", N20_DEV_LG, 0, "RRRRR");
    captura(saida, "en-resumo", N20_DEV_LG, 0, "RB");
  }
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas do guia da 2.0 gravadas.");
  return 0;
}
