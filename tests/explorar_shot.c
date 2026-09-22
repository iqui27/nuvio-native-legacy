// Captura a tela Explorar com dados locais: posters, meta, sinopse, nota,
// progresso e fundo. O objetivo e olhar legibilidade, foco e corte; nao e
// teste de rede nem de ordenacao do catalogo real.
#include "explorar.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void capturar(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 120; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(10);
    gfx_novo_quadro();
    explorar_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    explorar_desenhar(SDL_GetTicks());
    if (i == 119) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch,
               pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
    SDL_Delay(4);
  }
}

static void semear(void) {
  static CatItem itens[9];
  static const char *poster[] = {
    "deploy/app/art/poster/19.jpg", "deploy/app/art/poster/25.jpg",
    "deploy/app/art/poster/31.jpg", "deploy/app/art/poster/30.jpg",
    "deploy/app/art/poster/24.jpg", "deploy/app/art/poster/18.jpg",
    "deploy/app/art/poster/32.jpg", "deploy/app/art/poster/26.jpg",
    "deploy/app/art/poster/27.jpg"
  };
  int i;
  memset(itens, 0, sizeof itens);
  for (i = 0; i < 9; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt-explorar-%02d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "%s", i & 1 ? "series" : "movie");
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "%s",
             i == 0 ? "A última fronteira" : i == 1 ? "Noite de verão" :
             i == 2 ? "O mapa das estrelas" : i == 3 ? "Depois da chuva" :
             i == 4 ? "As cidades invisíveis" : "Horizonte %d");
    if (i >= 5) snprintf(itens[i].titulo, sizeof itens[i].titulo, "Horizonte %d", i - 4);
    snprintf(itens[i].poster, sizeof itens[i].poster, "%s", poster[i]);
    snprintf(itens[i].backdrop, sizeof itens[i].backdrop, "%s", poster[(i + 2) % 9]);
    snprintf(itens[i].meta, sizeof itens[i].meta, "%s", i & 1 ? "2024 · 3 temporadas" : "2025 · 1 h 41 min");
    snprintf(itens[i].genero, sizeof itens[i].genero, "%s", i & 1 ? "Drama · Mistério" : "Ficção científica · Aventura");
    snprintf(itens[i].direcao, sizeof itens[i].direcao, "%s", i & 1 ? "Lana Wachowski" : "Denis Villeneuve");
    snprintf(itens[i].pais, sizeof itens[i].pais, "%s", i & 1 ? "Estados Unidos" : "Canadá");
    snprintf(itens[i].provNome, sizeof itens[i].provNome, "%s", i & 1 ? "Nuvio Play" : "Cinemeta");
    snprintf(itens[i].classificacao, sizeof itens[i].classificacao, "%s", i & 1 ? "16" : "14");
    snprintf(itens[i].elenco[0].nome, sizeof itens[i].elenco[0].nome, "%s", "Rebecca Ferguson");
    snprintf(itens[i].elenco[1].nome, sizeof itens[i].elenco[1].nome, "%s", "Oscar Isaac");
    itens[i].nElenco = 2;
    snprintf(itens[i].sinopse, sizeof itens[i].sinopse,
             "Uma descoberta muda o caminho de quem decide olhar mais longe e atravessar o desconhecido.");
    itens[i].nota = 74 + i * 2;
    itens[i].progresso = i == 0 ? 38 : 0;
    itens[i].naLista = i == 0;
    itens[i].nTemporadas = i & 1 ? 3 : 0;
  }
  cat_definir_tudo(itens, 9, NULL, 0);
}

int main(int argc, char **argv) {
  SDL_Window *win;
  SDL_GLContext gl;
  char nome[640];
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: revisao da tela Explorar",
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  semear();
  explorar_iniciar();
  snprintf(nome, sizeof nome, "%s-base.bmp", argc > 1 ? argv[1] : "/tmp/nuvio-explorar");
  capturar(nome, win);
  { SDL_Event e = { 0 }; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RIGHT; explorar_evento(&e); }
  snprintf(nome, sizeof nome, "%s-foco2.bmp", argc > 1 ? argv[1] : "/tmp/nuvio-explorar");
  capturar(nome, win);
  puts("explorar_shot: capturas gravadas");
  explorar_encerrar();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
  return 0;
}
