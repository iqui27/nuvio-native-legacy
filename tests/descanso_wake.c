/* Real GL/cache + production descanso/esmaecer: idle, rotate, evict artwork,
 * replace the catalog, then wake by remote/OK/mouse. No account or network.
 * Catches freed textures being retained on exit and wake input leaking through. */
#include "descanso.h"
#include "esmaecer.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned agora = 100;
static void quadro(int estilo, int tocando) {
  agora += 100;
  cat_quadro();
  tex_bombear(3);
  esmaecer_quadro(agora, 0.1f, tocando);
  descanso_quadro(agora, 0.1f, esmaecer_descanso(), estilo, DESC_FONTE_CATALOGO);
  gfx_novo_quadro(); tex_novo_quadro(); txt_novo_quadro();
  glClear(GL_COLOR_BUFFER_BIT);
  esmaecer_desenhar(agora);
  glFinish();
  assert(glGetError() == GL_NO_ERROR);
}

int main(void) {
  static CatItem itens[3];
  char arte[1024];
  const char *dir = getenv("NUVIO_DADOS");
  assert(dir && *dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("descanso wake", 0, 0, 64, 64,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 64, 64); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1) && tex_iniciar(8));
  dados_iniciar(NULL); ajustes_dir(dir);
  snprintf(arte, sizeof arte, "%s/arte.bmp", dir);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 32, 18, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s); SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 90, 120, 160, 255));
  assert(SDL_SaveBMP(s, arte) == 0); SDL_FreeSurface(s);
  for (int i = 0; i < 3; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt-wake-%d", i);
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Wake %d", i);
    snprintf(itens[i].backdrop, sizeof itens[i].backdrop, "%s", arte);
    snprintf(itens[i].logo, sizeof itens[i].logo, "%s", arte);
  }
  cat_definir_tudo(itens, 3, NULL, 0);
  for (int estilo = ESM_ESTILO_VITRINE; estilo <= ESM_ESTILO_RELOGIO; estilo++) {
    for (int input = 0; input < 3; input++) {
      esmaecer_reiniciar(); esmaecer_escolha(1); esmaecer_estilo(estilo);
      quadro(estilo, 0);
      agora += 60000;
      for (int i = 0; i < 350; i++) {
        if (i == 180) { cat_definir_tudo(itens, 3, NULL, 0); tex_esquecer(arte); }
        quadro(estilo, 0);
      }
      assert(esmaecer_descanso());
      if (estilo == ESM_ESTILO_VITRINE) assert(descanso_indice_em_tela() >= 0);
      int item = descanso_indice_em_tela();
      GLuint tex = tex_obter_larg(arte, 1920);
      // Clock mode never requests artwork. Wait for the real async decoder.
      for (int i = 0; !tex && i < 100; i++) {
        SDL_Delay(1); quadro(estilo, 0); tex = tex_obter_larg(arte, 1920);
      }
      assert(tex && glIsTexture(tex));
      if (input == 1) descanso_pedir_abrir(); // OK, before input resets the stage
      assert(esmaecer_entrada(agora + 1, input != 2) == (input != 2));
      assert(!esmaecer_descanso() && esmaecer_veu() == 0);
      assert(descanso_pedido_abrir() == (input == 1 ? item : -1));
      assert(descanso_pedido_abrir() == -1);
      quadro(estilo, 0);
      assert(descanso_indice_em_tela() == -1 && glIsTexture(tex));
      assert(esmaecer_entrada(agora + 1, 1) == 1); // held remote cannot act twice
      assert(esmaecer_entrada(agora + 401, 1) == 0);
      quadro(estilo, 1); assert(esmaecer_estagio() == ESM_ACESO);
    }
  }
  puts("descanso wake: remote/OK/mouse, rotation, eviction and catalog replacement ok");
  tex_encerrar(); txt_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); IMG_Quit(); SDL_Quit();
  return 0;
}
