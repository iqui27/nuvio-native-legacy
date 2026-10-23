// CAPTURA DA BARRA ESTILO APPLE TV (layout Dinamica da home), sem rede.
// Fechada (so a pilula), aberta com o foco em varios itens, meio da abertura,
// e com um tema colorido. A arte de fundo e deploy/app/art/00.jpg, para julgar
// o vidro e a sombra da esquerda sobre uma imagem de verdade.
//
//   bash tests/sidebaratv_shot.sh /tmp/nv-sidebaratv
#include "menu.h"
#include "nvtmp.h"
#include "home.h"
#include "colecoes.h"
#include "catalogo.h"
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

static SDL_Window *win;
static const char *dirDados;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  menu_evento(&e);
}

static void ajusta(int tema, const char *extra) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dirDados);
  f = fopen(caminho, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme %d\nhomeLayoutLocal 2\n%s", tema, extra ? extra : "");
  fclose(f);
  ajustes_dir(dirDados);
}

// `n` quadros a 60 fps; grava o ultimo se `nome`.
static void quadros(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    GLuint fundo;
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    tex_bombear(6);
    menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fundo = tex_obter("deploy/app/art/03.jpg");
    if (fundo) {
      gfx_tex_aspect_atual = 16.0f / 9.0f;
      gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundo, GFX_CARD, 0, 0, 0, 0, 1, 1, 1, 1);
    }
    menu_desenhar(SDL_GetTicks());
    if (nome && i == n - 1) {
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
      printf("captura: %s rects=%d fill=%.2f\n", nome, gfx_n_rect, gfx_fill);
    }
    SDL_GL_SwapWindow(win);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-sidebaratv");
  char nome[600];
  SDL_GLContext gl;
  int i;

  dirDados = getenv("NUVIO_DADOS");
  assert(dirDados && *dirDados);
  ajusta(0, NULL);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: barra Apple TV", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1920, 1080,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  assert(home_iniciar("deploy/app/art"));
  assert(col_definir_json("{\"collections\":[{\"id\":\"streaming-fixture\",\"title\":\"Streaming\",\"folders\":[{\"id\":\"netflix-fixture\",\"title\":\"Netflix\",\"sources\":[{\"addonBaseUrl\":\"https://fixture.invalid\",\"type\":\"movie\",\"catalogId\":\"fixture\"}]}]}]}") == 1);
  { CatItem item = *cat_item(0); CatFileira row = {0};
    snprintf(row.chave, sizeof row.chave, "streaming-fixture-row");
    snprintf(row.base, sizeof row.base, "https://fixture.invalid");
    snprintf(row.catId, sizeof row.catId, "fixture");
    snprintf(row.tipo, sizeof row.tipo, "movie"); row.n = 1;
    cat_definir_tudo(&item, 1, &row, 1);
  }
  home_atualizar(0.016f, SDL_GetTicks());
  menu_iniciar();

  for (i = 0; i < 40; i++) { quadros(1, NULL); SDL_Delay(5); }   // arte de fundo
  snprintf(nome, sizeof nome, "%s-1-fechada.bmp", saida); quadros(30, nome);
  menu_abrir();
  snprintf(nome, sizeof nome, "%s-2-abrindo.bmp", saida); quadros(5, nome);
  snprintf(nome, sizeof nome, "%s-3-aberta-inicio.bmp", saida); quadros(60, nome);
  tecla(SDLK_DOWN);   // ordem do original: Inicio, Busca, Explorar...
  snprintf(nome, sizeof nome, "%s-4-foco-busca.bmp", saida); quadros(30, nome);
  tecla(SDLK_UP); tecla(SDLK_UP);
  snprintf(nome, sizeof nome, "%s-5-foco-perfil.bmp", saida); quadros(30, nome);
  for (i = 0; i < 12; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-6-foco-ajustes.bmp", saida); quadros(30, nome);
  tecla(SDLK_RETURN);   // escolhe Ajustes: a pilula passa a dizer "Ajustes"
  snprintf(nome, sizeof nome, "%s-7-fechando.bmp", saida); quadros(6, nome);
  snprintf(nome, sizeof nome, "%s-8-fechada-ajustes.bmp", saida); quadros(60, nome);

  // Tema colorido (Ocean = 2) e itens escondidos (Guia e Agenda).
  ajusta(2, "menuGuiaLocal 1\nmenuAgendaLocal 1\n");
  menu_definir_destino(MENU_INICIO);
  menu_abrir();
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-9-tema-ocultos.bmp", saida); quadros(60, nome);
  menu_fechar();
  quadros(40, NULL);

  // Pilula escondida (pagina rolada).
  menu_pilula_mostrar(0.0f);
  snprintf(nome, sizeof nome, "%s-10-rolada.bmp", saida); quadros(60, nome);
  { float x, y, w, h; int ok = menu_pilula_rect(&x, &y, &w, &h);
    printf("pilula: ok=%d x=%.0f y=%.0f w=%.0f h=%.0f alfa=%.2f\n", ok, x, y, w, h, menu_pilula_alfa()); }

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas da barra Apple TV gravadas.");
  return 0;
}
