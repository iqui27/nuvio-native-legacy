// #210 (Samsung .tpk 1.6.5): barra lateral moderna ligada, RECOLHIDA, e o nome
// do perfil e "Trocar de usuário" ficavam visiveis, apagados, ao lado do
// avatar. Ao fechar o menu, `desliza` (150 ms) chega a 0 antes de `expande`
// (150 * 1.6 ms); o menu_atualizar entao parava de animar e so zerava
// `expande` se `desliza` ainda nao fosse 0 — e a rampa crava 0 exato. O resto
// de `expande` (~0,37) seguia valendo no rodape da rail fixa.
//
// Captura headless: abre e fecha o menu como o controle faz e compara o
// rodape da rail com o de um menu recem-iniciado. Grava os BMP para olhar.
#include "menu.h"
#include "nvtmp.h"
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

static float dtQuadro = 1.0f / 60.0f;
static unsigned char *quadro(SDL_Window *win, int frames, const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  int i;
  assert(pix);
  for (i = 0; i < frames; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    menu_atualizar(dtQuadro, SDL_GetTicks());
    glClearColor(0.30f, 0.30f, 0.34f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    menu_desenhar(SDL_GetTicks());
    if (i == frames - 1) glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    SDL_GL_SwapWindow(win);
  }
  { SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
    int y;
    assert(s);
    for (y = 0; y < 1080; y++)
      memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
    SDL_SaveBMP(s, nome);
    SDL_FreeSurface(s); }
  return pix;
}

// Maior diferenca de canal no retangulo do texto do rodape (a direita da rail
// de 144px, na altura do avatar). Coordenadas GL: y de baixo para cima.
static int difRodape(const unsigned char *a, const unsigned char *b) {
  int x, y, c, m = 0;
  for (y = 40; y < 260; y++)
    for (x = 150; x < 520; x++)
      for (c = 0; c < 3; c++) {
        int d = abs((int)a[(y * 1920 + x) * 4 + c] - (int)b[(y * 1920 + x) * 4 + c]);
        if (d > m) m = d;
      }
  return m;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-rail-rodape");
  char dir[NVT_MAX], caminho[700], nome[600]; nvt_set(dir, sizeof dir, "/nuvio-rail-rodape-XXXXXX");
  SDL_Window *w; SDL_GLContext gl; FILE *f;
  unsigned char *antes, *depois;
  int d, volta;

  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w"); assert(f);
  // Valor gravado e o indice da lista: 0 = Ligada.
  fputs("idioma 0\nmodernSidebar 0\n", f);
  fclose(f);
  ajustes_dir(dir);
  assert(ajustes_rail_moderna() && !ajustes_rail_recolhida());

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: rodape da rail", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  menu_iniciar();
  snprintf(nome, sizeof nome, "%s-inicio.bmp", saida);
  antes = quadro(w, 20, nome);
  // Varias idas e voltas, como trocar de item na barra. O passo de quadro
  // muda a cada volta: com 1/60 exato a rampa de `desliza` termina num resto
  // de float (!= 0) e o reset antigo rodava; com 1/50 ela passa do alvo e
  // crava 0 — o "as vezes" da issue e o FPS da TV variando.
  { static const float dts[] = { 1.0f / 60.0f, 1.0f / 50.0f, 1.0f / 45.0f, 1.0f / 30.0f };
  for (volta = 0; volta < 4; volta++) {
    dtQuadro = dts[volta];
    menu_abrir();
    snprintf(nome, sizeof nome, "%s-aberto%d.bmp", saida, volta);
    free(quadro(w, 40, nome));
    menu_fechar();
    snprintf(nome, sizeof nome, "%s-fechado%d.bmp", saida, volta);
    depois = quadro(w, 40, nome);
    d = difRodape(antes, depois);
    printf("volta %d (dt 1/%.0f): diferenca no rodape = %d\n", volta, 1.0f / dtQuadro, d);
    assert(d <= 2 && "texto do perfil visivel com a barra recolhida");
    free(depois);
  } }
  free(antes);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  unlink(caminho);
  { char t[700]; snprintf(t, sizeof t, "%s/ajustes.tmp", dir); unlink(t);
    snprintf(t, sizeof t, "%s/envio-151.txt", dir); unlink(t); }
  rmdir(dir);
  puts("rail_rodape_recolhida: ok");
  return 0;
}
