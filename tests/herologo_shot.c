// CAPTURA DA HOME POR COMBINACAO DE AJUSTE DE ARTE (22/09).
//
// Pedido do dono: "a settings de selecionar o source das artes nao ta
// funcionando, e tem que ter um toggle de ter uma versao diferente do que
// mostra no card do que ta na hero". Esta captura e a prova visual das duas
// coisas: para cada fonte (Automatico, TMDB, Trakt) com "Destaque com outra
// arte" desligado e ligado, a home com o destaque e a fileira de cards
// deitados. Desligado, card focado e destaque sao a MESMA foto; ligado, o
// card fica com a do catalogo e o destaque muda.
//
// Os itens sao do Cinemeta como chegam de verdade (background = metahub pelo
// id, e so) — exatamente o caso em que o ajuste antigo dava cinco vezes a
// mesma url. Os ajustes entram pelo ajustes.txt da pasta temporaria, o mesmo
// caminho que a TV le.
#include "app.h"
#include "addons.h"
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "descoberta.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "nuvem.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NT 3
static int shotW = 1920, shotH = 1080;
static void quadros(SDL_Window *w, int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    home_atualizar(1.0f / 60.0f, SDL_GetTicks());
    home_desenhar(SDL_GetTicks());
    if (bmp && i == n - 1) {
      unsigned char *pix = malloc(shotW * shotH * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, shotW, shotH, 32, SDL_PIXELFORMAT_RGBA32);
      int y;
      assert(pix && s);
      glReadPixels(0, 0, shotW, shotH, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (y = 0; y < shotH; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (shotH - 1 - y) * shotW * 4, shotW * 4);
      size_t len = strlen(bmp);
      assert((len >= 4 && !strcmp(bmp + len - 4, ".png") ? IMG_SavePNG(s, bmp) : SDL_SaveBMP(s, bmp)) == 0);
      SDL_FreeSurface(s);
      free(pix);
      printf("captura: %s\n", bmp);
    }
    SDL_GL_SwapWindow(w);
    SDL_Delay(16);
  }
}

// Moderna hero logo position: local fixture images, no network.
// usage: herologo_shot <tmpdir> <fixtures dir> <out prefix>
int main(int argc, char **argv) {
  assert(argc >= 4);
  const char *dir = argv[1], *fx = argv[2], *saida = argv[3];
  if (argc > 7) {
    shotW = atoi(argv[6]); shotH = atoi(argv[7]);
  }
  layout_tela_definir(shotW, shotH);
  layout_modo_definir(shotW != 1920 || shotH != 1080);
  static CatItem itens[NT];
  CatFileira fil;
  SDL_Window *w;
  SDL_GLContext gl;
  char cache[600], bmp[700], cam[700];
  int i;
  FILE *a;
  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("hero logo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       shotW, shotH, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, shotW, shotH);
  gfx_tamanho_alvo(shotW, shotH);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  artehero_definir_falhou(tex_falhou);
  nuvem_configurar("deploy/app/art");
  desc_tmdb("deploy/app/art");
  snprintf(cache, sizeof cache, "%s/cache", dir);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  a = fopen(cam, "w"); assert(a);
  fprintf(a, "trailerHero 1\n");
  if (argc > 5)
    fprintf(a, "heroSizeLocal %d\nhomeLayoutLocal %d\nvidroLocal 1\nvidroFoscoLocal 1\n", atoi(argv[4]), atoi(argv[5]));
  fclose(a);
  ajustes_dir(dir);
  if (argc > 4) {
    AddonRemoto addon = {0};
    snprintf(addon.nome, sizeof addon.nome, "Fixture");
    snprintf(addon.url, sizeof addon.url, "https://addon.invalid/x");
    addon.ativo = 1;
    assert(addons_definir_lista(&addon, 1));
#ifdef NV_SHOT_HOOKS
    assert(ajustes_shot_valor("vidroLocal", 1));
    assert(ajustes_shot_valor("heroSizeLocal", atoi(argv[4])));
    assert(ajustes_shot_valor("homeLayoutLocal", atoi(argv[5])));
#endif
    assert(!ajustes_vidro());
    assert(ajustes_hero_tamanho() == atoi(argv[4]));
    // Settings initialization selects TV mode on this host; apply the shot viewport afterward.
    layout_tela_definir(shotW, shotH);
    layout_modo_definir(shotW != 1920 || shotH != 1080);
  }
  assert(home_iniciar("deploy/app/art"));
  memset(itens, 0, sizeof itens);
  for (i = 0; i < NT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt000000%d", i + 1);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Fixture %d", i + 1);
    snprintf(c->genero, sizeof c->genero, "Movie");
    snprintf(c->meta, sizeof c->meta, "2024");
    if (argc > 4) {
      snprintf(c->genero, sizeof c->genero, "Movie · Drama · Mystery");
      snprintf(c->meta, sizeof c->meta, "2024 · 2 h 10 min");
      snprintf(c->classificacao, sizeof c->classificacao, "14");
      c->nota = 84;
    }
    snprintf(c->backdrop, sizeof c->backdrop, "%s/bg.png", fx);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
    snprintf(c->poster, sizeof c->poster, "%s/bg.png", fx);
    snprintf(c->logo, sizeof c->logo, "%s/%s.png", fx, i == 1 ? "tall" : "wide");
    if (i != 2)
      snprintf(c->sinopse, sizeof c->sinopse, "A long synopsis for the fixture title that wraps over several lines so the text block is as tall as a real one. It keeps going to fill three whole lines of the hero copy area, and a bit more beyond.");
  }
  memset(&fil, 0, sizeof fil);
  snprintf(fil.chave, sizeof fil.chave, "cinemeta_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Populares - Filme");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = NT;
  cat_definir_tudo(itens, NT, &fil, 1);
  quadros(w, 60, NULL);
  if (argc > 4) quadros(w, 60, saida);
  else { SDL_Event e;
    const SDL_Keycode TECLAS[] = { SDLK_DOWN, SDLK_RIGHT, SDLK_RIGHT, SDLK_UP };
    const char *nome[] = { "wide-sinopse", "tall-sinopse", "wide-semsinopse", "hero-foco" };
    for (i = 0; i < 4; i++) {
      memset(&e, 0, sizeof e);
      e.type = SDL_KEYDOWN; e.key.keysym.sym = TECLAS[i];
      home_evento(&e);
      e.type = SDL_KEYUP;
      home_evento(&e);
      quadros(w, 90, NULL);
      snprintf(bmp, sizeof bmp, "%s-%s.bmp", saida, nome[i]);
      quadros(w, 5, bmp);
    } }
  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
