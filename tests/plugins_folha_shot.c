// A FOLHA DE FONTES via repositorio local (ou PLUG_REPO externo) da conta.
// A busca de fontes dos addons (sem addon nenhum) roda os scrapers, e a lista e
// a que a folha desenha. Prova que o nome do scraper vira o provedor da linha e
// que qualidade/selos saem do parser de sempre.
//   bash tests/plugins_folha_shot.sh [/tmp/prefixo]
//   PLUG_REPO=https://example.org/manifest.json bash tests/plugins_folha_shot.sh
#include "plugins.h"
#include "addons.h"
#include "fontecache.h"
#include "streams.h"
#include "badges.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    stream_folha_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4); SDL_Surface *s; int y;
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0); SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}
static void tecla(SDL_Keycode k) { SDL_Event e = { 0 }; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; stream_folha_evento(&e); }

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-plugins-folha";
  char nome[600], c[700];
  SDL_Window *w; PlugRepo r; Stream *l = NULL; int n, t = 0, k; FILE *f;
  dados_iniciar(".");
  snprintf(c, sizeof c, "%s/ajustes.txt", dados_dir());
  f = fopen(c, "w"); if (f) { fprintf(f, "idioma 0\nselected_theme 2\n"); fclose(f); }
  ajustes_dir(dados_dir());
  plugins_iniciar();
  memset(&r, 0, sizeof r);
  assert(getenv("PLUG_REPO"));
  snprintf(r.url, sizeof r.url, "%s", getenv("PLUG_REPO"));
  snprintf(r.tipo, sizeof r.tipo, "NUVIO_JS"); r.ativo = 1;
  assert(plugins_definir_da_conta(&r, 1, plugins_geracao()));
  plugins_definir_ligado(1);
  while (plugins_atualizando() && t++ < 200) usleep(100000);
  plugins_definir_ligado(1);
  plugins_ligar_aos_addons();
  n = addons_consultar("tmdb:27205", "movie", NULL, 4, NULL, NULL, &l);
  printf("fontes de plugin: %d\n", n);
  if (n <= 0) return 1;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: fontes de plugin", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w); assert(SDL_GL_CreateContext(w));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  stream_definir_lista(l, n);
  stream_folha_contexto("Inception (2010)");
  stream_definir_atual(0);
  stream_folha_abrir();
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha.png", saida); captura(nome, w);
  for (k = 0; k < 14; k++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha-meio.png", saida); captura(nome, w);
  free(l);
  return 0;
}
