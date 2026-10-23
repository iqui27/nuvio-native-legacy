// CAPTURAS DO DISPENSAR (06/10): o modal da estreia na ilha com o foco no
// ultimo botao, e a aba Avisos do painel de Salvos com a linha em foco. Com
// -DDEPOIS (o codigo novo) tambem o menu do OK longo, a lista depois de
// dispensar um e a linha "Dispensar todos". Sem -DDEPOIS compila contra o
// codigo de antes (tests/avisos_dispensar_shot.sh monta os dois), para o
// antes/depois com o mesmo roteiro. Itens da demonstracao (NUVIO_AVISOS_DEMO).
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano.
#include "ilha.h"
#include "nvtmp.h"
#include "salvospainel.h"
#include "avisos.h"
#include "ctxmenu.h"
#include "ajustes.h"
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
static const char *base;

static void captura(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.bmp", base, nome);
  assert(SDL_SaveBMP(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static void quadros(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    GLuint fundo;
    SDL_PumpEvents();
    tex_novo_quadro();
    tex_bombear(3);
    spainel_atualizar(1.0f / 60.0f, agora);
    ctx_atualizar(1.0f / 60.0f, agora);
    avisos_atualizar(1.0f / 60.0f, agora);
    glClearColor(0.05f, 0.05f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fundo = tex_obter("deploy/app/art/00.jpg");
    if (fundo) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundo, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    if (spainel_visivel()) spainel_desenhar(agora);
    if (ctx_aberto()) ctx_desenhar(agora);
    ilha_relogio_visivel(ajustes_relogio_ligado());
    ilha_posicionar(0);
    ilha_coberta(spainel_visivel());
    ilha_desenhar(agora);
    if (i == n - 1 && nome) captura(nome);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}

static void tecla(SDL_Keycode k, int tipo) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = tipo; e.key.keysym.sym = k;
  if (ctx_aberto()) ctx_evento(&e);
  else if (spainel_aberto()) spainel_evento(&e);
  else if (!ilha_evento(&e) && tipo == SDL_KEYDOWN && k == SDLK_s && ilha_cartao_na_tela()) ilha_modal_abrir();
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700];
  SDL_GLContext gl;
  FILE *f;
  IlhaCartao est;
  int k;
  base = argc > 1 ? argv[1] : nvt("/nuvio-dispensar");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n");
  fclose(f);
  ajustes_dir(dir);
  dados_iniciar(NULL);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: dispensar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);

  // --- 1. o modal da estreia, foco no ultimo botao ---
  memset(&est, 0, sizeof est);
  snprintf(est.chave, sizeof est.chave, "estreia:demo:agenda");
  snprintf(est.avisoId, sizeof est.avisoId, "demo:agenda");
  snprintf(est.imdb, sizeof est.imdb, "tt3006802");
  est.serie = 1; est.t = 7; est.e = 9; est.progresso = -1.0f;
  snprintf(est.titulo, sizeof est.titulo, "Outlander");
  snprintf(est.epNome, sizeof est.epNome, "Unfinished Business");
  snprintf(est.sinopse, sizeof est.sinopse, "Claire and Jamie face a reckoning as the war reaches Fraser's Ridge.");
  snprintf(est.poster, sizeof est.poster, "deploy/app/art/poster/01.jpg");
  snprintf(est.logo, sizeof est.logo, "deploy/app/art/logo/01.png");
  snprintf(est.arte, sizeof est.arte, "deploy/app/art/01.jpg");
  snprintf(est.quando, sizeof est.quando, "hoje");
  ilha_cartao(ILHA_ESTREIA, &est);
  quadros(60, "1-pilula-estreia");
  tecla(SDLK_s, SDL_KEYDOWN);
  quadros(50, NULL);
  for (k = 0; k < 3; k++) tecla(SDLK_RIGHT, SDL_KEYDOWN);
  quadros(30, "2-modal-estreia");
  tecla(SDLK_ESCAPE, SDL_KEYDOWN);   // Voltar: recolhe, nao dispensa
  quadros(40, "3-voltar-cartao-fica");
  assert(ilha_cartao_na_tela());
  ilha_cartao(ILHA_ESTREIA, NULL);
  quadros(30, NULL);

  // --- 2. a aba Avisos do painel de Salvos ---
  setenv("NUVIO_AVISOS_DEMO", "1", 1);
  avisos_iniciar();
  spainel_abrir();
  spainel_ir_aba(3);   // SP_ABA_AVISOS
  quadros(60, NULL);
  tecla(SDLK_DOWN, SDL_KEYDOWN);     // a estreia (segunda linha)
  quadros(40, "4-aba-avisos");
#ifdef DEPOIS
  tecla(SDLK_RETURN, SDL_KEYDOWN);   // segura OK
  SDL_Delay(760);
  quadros(40, "5-menu-segurar");
  tecla(SDLK_RETURN, SDL_KEYUP);
  tecla(SDLK_RETURN, SDL_KEYDOWN);   // "Dispensar" (o primeiro)
  quadros(60, "6-dispensado");
  for (k = 0; k < 8; k++) tecla(SDLK_DOWN, SDL_KEYDOWN);
  quadros(40, "7-dispensar-todos");
  tecla(SDLK_RETURN, SDL_KEYDOWN);
  quadros(40, "8-vazio");
  assert(avisos_lista_n() == 0);
#endif
  spainel_fechar();
  quadros(20, NULL);
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
