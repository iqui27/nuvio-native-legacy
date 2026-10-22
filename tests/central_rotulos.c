// OS NOMES DOS BOTOES DA CENTRAL CABEM (centrallista.c): todos os 22 itens do
// catalogo, nao so os de fabrica, nos 30 idiomas, na fonte da TV (Montserrat) e na Inter de fabrica,
// no degrau menor que botao() usa antes de cortar (15 px). Precisa de GL
// (a medida passa pelo rasterizador de texto), por isso a janela escondida.
#include "central.h"
#include "centrallista.h"
#include "ajustes.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  float lim = central_rotulo_w();
  int lg, i, f, falhas = 0, menores = 0;
  static const TxtFamilia FAM[2] = { TXT_FAMILIA_MONTSERRAT, TXT_FAMILIA_INTER };
  assert(dir && *dir);
  ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("rotulos", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  assert(central_catalogo_n() == 27);
  // Montserrat (a da TV do dono) e a Inter de fabrica.
  for (f = 0; f < 2; f++) {
  txt_definir_fonte_interface(FAM[f]);
  for (lg = 0; lg < IDIOMA_N; lg++) {
    assert(ajustes_shot_valor("idioma", lg + 1));
    assert(ajustes_idioma() == lg);
    for (i = 0; i < central_catalogo_n(); i++) {
      const char *k = central_catalogo(i)->curto;
      int w15 = txt_largura(TXT_ILHA_HORA, k), w16 = txt_largura(TXT_G16B, k);
      if ((float)w15 > lim) {
        printf("FALHA %s/%s: \"%s\" -> \"%s\" %d px > %.0f\n", f ? "inter" : "montserrat",
               idioma_iso(lg), k, i18n(k), w15, lim);
        falhas++;
      } else if ((float)w16 > lim) menores++;
    }
  }
  }
  printf("limite %.0f px; %d nomes no degrau menor (15 px)\n", lim, menores);
  txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  if (falhas) { printf("%d nome(s) nao cabem\n", falhas); return 1; }
  puts("PASS: os 27 nomes cabem nos 30 idiomas");
  return 0;
}
