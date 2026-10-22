// Captura do cartao de novidades da 2.0.2 SEM janela visivel (janela GL
// escondida, FBO, glReadPixels), como tests/novidades148_shot.c. Antes, as
// regras do cartao por tecla; depois a previa em varios momentos das tres
// cenas, a pagina "Apoie o projeto", ingles e animacoes reduzidas.
#include "novidades202.h"
#include "apoio.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "idiomacod.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void ajustesDeTeste(int ingles, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n", ingles, tema, reduzidas);
  // NUVIO_SHOT_FONTE=3 (TXT_FAMILIA_*): a fonte da interface da TV
  // (Montserrat na TCL do dono), mais larga que a Inter do Mac.
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades202_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

static void regras(void) {
  // Foco inicial = Continuar: OK vai para a pagina de apoio, sem fechar.
  dados_apagar(N202_ARQ);
  novidades202_abrir();
  assert(novidades202_pagina() == 0);
  tecla(SDLK_RETURN);
  assert(novidades202_aberto() && novidades202_pagina() == 1);
  // Voltar (tecla) volta para as novidades; OK em Concluir fecha.
  tecla(SDLK_ESCAPE);
  assert(novidades202_aberto() && novidades202_pagina() == 0);
  tecla(SDLK_RETURN);
  assert(novidades202_pagina() == 1);
  tecla(SDLK_RETURN);
  assert(!novidades202_aberto());
  assert(novidades202_pedido() == N202_PEDIU_NADA);
  assert(existe(N202_ARQ));
  // Agora nao fecha direto, sem passar pela pagina de apoio.
  dados_apagar(N202_ARQ);
  novidades202_abrir();
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!novidades202_aberto() && novidades202_pedido() == N202_PEDIU_NADA);
  // Voltar na primeira pagina = Agora nao.
  dados_apagar(N202_ARQ);
  novidades202_abrir();
  tecla(SDLK_ESCAPE);
  assert(!novidades202_aberto() && existe(N202_ARQ));
  // Os dois enderecos preenchidos.
  assert(apoio_n() == 2);
  assert(!strcmp(apoio_url(APOIO_PATREON), "https://www.patreon.com/cw/CraaazyDevs"));
  assert(!strcmp(apoio_url(APOIO_KOFI), "https://ko-fi.com/iqui27"));
  assert(!strcmp(apoio_url_curta(APOIO_PATREON), "patreon.com/cw/CraaazyDevs"));
  puts("PASS: regras do cartao da 2.0.2");
}

static void primeiraVez(void) {
  // Instalacao nova (ou vindo da 1.x): sem a marca do guia da 2.0 o guia abre
  // e este cartao NAO abre, mas fica visto.
  dados_apagar("novidades-20-guia.txt");
  dados_apagar(N202_ARQ);
  novidades202_teste_esquecer();
  novidades202_primeira_vez();
  assert(!novidades202_aberto());
  assert(existe(N202_ARQ));
  puts("PASS: instalacao nova: o cartao da 2.0.2 nao abre e fica visto");
  // Vindo da 2.0.0: o guia ja foi visto, a marca da 2.0.2 nao existe. Abre
  // uma vez; fechar grava a marca.
  dados_gravar("novidades-20-guia.txt", "1\n");
  dados_apagar(N202_ARQ);
  novidades202_teste_esquecer();
  novidades202_primeira_vez();
  assert(novidades202_aberto());
  tecla(SDLK_ESCAPE);
  assert(!novidades202_aberto() && existe(N202_ARQ));
  puts("PASS: vindo da 2.0.0: o cartao abre uma vez e grava a marca");
  // Ja visto: nao abre de novo (em outra sessao do app).
  novidades202_teste_esquecer();
  novidades202_primeira_vez();
  assert(!novidades202_aberto());
  puts("PASS: ja visto: nao abre de novo");
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades202_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  novidades202_desenhar(SDL_GetTicks());
  glFinish();
}

static void grava(const char *nome) {
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
  printf("captura: %s\n", nome);
}

// Abre o cartao, espera as artes, poe o relogio da previa em `seg` e grava.
// `pagina` 1 = OK em Continuar antes (a troca anda ate o fim).
static void captura(const char *saida, const char *nome, float seg, int pagina) {
  char cam[700];
  int i;
  novidades202_abrir();
  for (i = 0; i < 900 && !novidades202_previa_pronta(); i++) { quadro(0.0f); SDL_Delay(4); }
  assert(novidades202_previa_pronta());
  for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);   // entrada do cartao e da lista
  if (pagina) { tecla(SDLK_RETURN); for (i = 0; i < 40; i++) quadro(1.0f / 60.0f); }
  novidades202_teste_relogio(seg);
  // Quadros parados: icones e texto novos terminam de carregar.
  for (i = 0; i < 12; i++) { quadro(0.0f); SDL_Delay(2); }
  snprintf(cam, sizeof cam, "%s-%s.bmp", saida, nome);
  grava(cam);
}

// Em TODOS os idiomas a lista termina acima do rodape com folga, e com a
// fonte da TV (NUVIO_SHOT_FONTE=3) nenhuma frase precisa de reticencias.
static void cabeEmTodos(void) {
  int idi, i, comCorte = 0;
  for (idi = 0; idi < IDIOMA_N; idi++) {
    int c;
    float folga;
    ajustesDeTeste(idi, 0, 2);
    novidades202_abrir();
    for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);
    folga = novidades202_teste_folga();
    c = novidades202_teste_cortadas();
    if (c) { printf("idioma %d (%s): %d frase(s) com reticencias\n", idi, idioma_iso(idi), c); comCorte++; }
    assert(folga >= 28.0f);
    tecla(SDLK_ESCAPE);
    for (i = 0; i < 20; i++) quadro(1.0f / 60.0f);
  }
  if (getenv("NUVIO_SHOT_FONTE")) assert(comCorte == 0);
  printf("PASS: a lista acaba acima do rodape nos %d idiomas (%d com reticencias)\n", IDIOMA_N, comCorte);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-novidades202";
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  novidades202_dir("deploy/app/art");
  primeiraVez();

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades202-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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

  regras();

  ajustesDeTeste(0, 0, 2);
  captura(saida, "pt-menu-inicio", 0.40f, 0);
  captura(saida, "pt-menu-aberto", 1.30f, 0);
  captura(saida, "pt-menu-foco", 3.40f, 0);
  captura(saida, "pt-hero", 4.20f + 2.60f, 0);
  captura(saida, "pt-ilha-espera", 8.40f + 2.00f, 0);
  captura(saida, "pt-ilha-fonte", 8.40f + 3.60f, 0);
  captura(saida, "pt-apoio", 8.40f + 1.0f, 1);

  ajustesDeTeste(1, 0, 5);
  captura(saida, "en-menu", 2.50f, 0);
  captura(saida, "en-apoio", 1.0f, 1);

  ajustesDeTeste(IDIOMA_DE, 0, 2);
  captura(saida, "de-ilha", 8.40f + 3.60f, 0);
  ajustesDeTeste(IDIOMA_JA, 0, 2);
  captura(saida, "ja-menu", 2.50f, 0);
  ajustesDeTeste(IDIOMA_HU, 0, 2);
  captura(saida, "hu-menu", 2.50f, 0);
  ajustesDeTeste(IDIOMA_RU, 0, 2);
  captura(saida, "ru-ilha", 8.40f + 2.00f, 0);

  ajustesDeTeste(0, 1, 2);
  captura(saida, "reduzido", 0.5f, 0);

  cabeEmTodos();

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 2.0.2 gravadas.");
  return 0;
}
