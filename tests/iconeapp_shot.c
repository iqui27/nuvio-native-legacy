// CAPTURA DO ICONE DO APP (apoiadores) fora dos Ajustes: a marca no canto da
// tela de login e no alto da barra lateral aberta, com dois icones diferentes.
// Sem rede: a build de teste nao tem servidor, entao o login desenha a frase
// de "pacote sem servidor" — o que importa aqui e o canto.
//
//   bash tests/iconeapp_shot.sh /tmp/nuvio-icone
#include "ajustes.h"
#include "nvtmp.h"
#include "gfx.h"
#include "iconeapp.h"
#include "login.h"
#include "menu.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int ajustes_teste_op_icone(int escolha);

static void grava(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", nome);
}

static void quadros(SDL_Window *w, int tela, const char *nome) {
  for (int i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (tela == 0) login_desenhar(SDL_GetTicks());
    else menu_desenhar(SDL_GetTicks());
    if (i == 59) grava(nome);
    SDL_GL_SwapWindow(w);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-icone");
  static const int esc[] = { 0, 1, 9 };
  char nome[600];
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: icone do app", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, 1920, 1080,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w);
  assert(gl);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_iniciar();
  iconeapp_iniciar("deploy/app/art");

  // Sem o portao, o icone gravado nao vale: Original, e nada no canto.
  ajustes_teste_op_icone(1);
  assert(iconeapp_atual() == 0);
  setenv("NUVIO_APOIADOR", "1", 1);
  apoiador_reler();
  assert(apoiador_ativo() && iconeapp_atual() == 1);

  login_iniciar();
  for (int k = 0; k < 3; k++) {
    ajustes_teste_op_icone(esc[k]);
    snprintf(nome, sizeof nome, "%s-login-%s.png", saida, iconeapp_id(esc[k]));
    quadros(w, 0, nome);
    menu_abrir();
    snprintf(nome, sizeof nome, "%s-menu-%s.png", saida, iconeapp_id(esc[k]));
    quadros(w, 1, nome);
    menu_fechar();
  }
  puts("PASS: capturas do icone do app gravadas.");
  return 0;
}
