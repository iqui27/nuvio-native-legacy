// CAPTURA DA FOLHA DE EPISODIOS E DO MENU DE VISTO, sem interacao e sem rede.
//
// Mesmo motivo de tests/ajustes_shot.c: interface de TV julgada so por codigo
// sai ilegivel a 3 m. E esta tela em especial nao da para alcancar na TV por
// tecla injetada — ela abre pelo botao de episodios DO PLAYER, e chegar la
// exige reproduzir algo. Uma captura headless e repetivel e nao gasta fonte.
#include "episodios.h"
#include "catalogo.h"
#include "vistoep.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void tecla(SDL_Keycode k, int tipo, int repeticao) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = tipo;
  e.key.keysym.sym = k;
  e.key.repeat = repeticao;
  episodios_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 50; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    episodios_atualizar(1.0f / 60.0f);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    episodios_desenhar();
    if (i == 49) {
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
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-episodios";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  CatItem c;
  CatEp eps[10];
  int i;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao dos episodios", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
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

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Silo");
  snprintf(c.imdb, sizeof c.imdb, "tt14688458");
  c.nTemporadas = 1; c.temporadas[0] = 1;
  cat_definir(&c, 1);
  memset(eps, 0, sizeof eps);
  for (i = 0; i < 8; i++) {
    eps[i].temporada = 1; eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Episódio %d", i + 1);
    snprintf(eps[i].data, sizeof eps[i].data, "5 de dezembro de 2024");
    snprintf(eps[i].duracao, sizeof eps[i].duracao, "53 min");
    snprintf(eps[i].sinopse, sizeof eps[i].sinopse,
             "Juliette parte numa busca perigosa para recuperar um traje.");
  }
  cat_definir_episodios(0, eps, 8);

  // Quatro vistos, o quinto explicitamente NAO visto, o resto desconhecido —
  // os tres estados que a linha sabe desenhar.
  for (i = 1; i <= 4; i++) vistoep_definir("tt14688458", 1, i, 1);
  vistoep_definir("tt14688458", 1, 5, 0);

  episodios_abrir(0, 1, 3);
  snprintf(nome, sizeof nome, "%s-lista.bmp", saida);
  captura(nome, w);

  // Desce para um episodio VISTO e segura OK: o menu tem de abrir oferecendo
  // DESMARCAR, porque e o gesto que faz sentido sobre o que ja esta visto.
  tecla(SDLK_DOWN, SDL_KEYDOWN, 0);
  tecla(SDLK_RETURN, SDL_KEYDOWN, 0);
  SDL_Delay(800);                      // maior que NV_HOLD_MS
  tecla(SDLK_RETURN, SDL_KEYUP, 0);
  snprintf(nome, sizeof nome, "%s-menu.bmp", saida);
  captura(nome, w);

  tecla(SDLK_DOWN, SDL_KEYDOWN, 0);
  snprintf(nome, sizeof nome, "%s-menu-ate.bmp", saida);
  captura(nome, w);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas da folha de episodios gravadas.");
  return 0;
}
