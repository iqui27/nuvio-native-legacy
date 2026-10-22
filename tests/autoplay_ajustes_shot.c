// CAPTURA de Ajustes > Fontes e addons > Escolha da fonte com as linhas do
// auto-play do oficial (#202): escopo, add-ons/plugins permitidos, "usar os
// outros", regex (modo, padrao, modelo), o padrao invalido e a folha de
// add-ons permitidos. Sem rede: os add-ons sao enderecos que nao respondem.
//
//   NUVIO_SHOT_FONTE=3 bash tests/autoplay_ajustes_shot.sh /tmp/nv-202/aj
#include "ajustes.h"
#include "addons.h"
#include "fonteregra.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int ajustes_teste_focar_opcao(int op);
extern int ajustes_teste_op_por_chave(const char *chave);
extern void ajustes_teste_fonte_interface(int familia);
extern void ajustes_teste_permitidos(int qual, int foco);

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nv-202/aj";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  AddonRemoto a[4];
  int opEscopo, opPadrao, opAddons;
  static const char *const NOMES[4] = { "Cuevana ES", "Torrentio Latino", "Torrentio", "AIOStreams" };

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: auto-play", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
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
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_FONTE")) ajustes_teste_fonte_interface(atoi(getenv("NUVIO_SHOT_FONTE")));

  memset(a, 0, sizeof a);
  for (int i = 0; i < 4; i++) {
    snprintf(a[i].nome, sizeof a[i].nome, "%s", NOMES[i]);
    snprintf(a[i].url, sizeof a[i].url, "http://127.0.0.1:1/a%d/manifest.json", i);
    a[i].ativo = 1;
  }
  addons_definir_lista(a, 4);
  fonteregra_limpar(0); fonteregra_limpar(1);
  fonteregra_alternar(0, "Cuevana ES");
  fonteregra_alternar(0, "Torrentio Latino");
  fonteregra_definir_regex(fonteregra_modelo(1));

  opEscopo = ajustes_teste_op_por_chave("streamAutoPlaySource");
  opPadrao = ajustes_teste_op_por_chave("-fonteRegexPadrao");
  opAddons = ajustes_teste_op_por_chave("-fonteAddonsPermitidos");
  assert(opEscopo >= 0 && opPadrao >= 0 && opAddons >= 0);

  // 1) As linhas novas, com o foco em "Fontes do automatico".
  assert(ajustes_teste_focar_opcao(opEscopo));
  snprintf(nome, sizeof nome, "%s-1-linhas.png", saida);
  captura(nome, w);

  // 2) Foco em "Add-ons permitidos" (o resumo com os dois espanhois).
  assert(ajustes_teste_focar_opcao(opAddons));
  snprintf(nome, sizeof nome, "%s-2-permitidos.png", saida);
  captura(nome, w);

  // 3) A folha de add-ons permitidos, foco no primeiro add-on.
  ajustes_teste_permitidos(1, 1);
  snprintf(nome, sizeof nome, "%s-3-folha.png", saida);
  captura(nome, w);
  ajustes_teste_permitidos(0, 0);

  // 3b) A folha da ordem dos add-ons (2.0.2): Torrentio primeiro, depois o
  // Cuevana; os outros dois estao fora da ordem. Foco no Cuevana.
  { const char *ordem[2] = { "Torrentio Latino", "Cuevana ES" };
    fonteregra_ordem_definir(ordem, 2); }
  ajustes_teste_permitidos(3, 2);
  snprintf(nome, sizeof nome, "%s-5-ordem.png", saida);
  captura(nome, w);
  ajustes_teste_permitidos(0, 0);
  assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("-fonteOrdem")));
  snprintf(nome, sizeof nome, "%s-6-ordem-linha.png", saida);
  captura(nome, w);
  fonteregra_ordem_definir(NULL, 0);

  // 4) Regex invalida: a linha avisa e o automatico a ignora.
  fonteregra_definir_regex("(ESP|Latino");
  assert(ajustes_teste_focar_opcao(opPadrao));
  snprintf(nome, sizeof nome, "%s-4-regex-invalida.png", saida);
  captura(nome, w);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
