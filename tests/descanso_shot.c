// CAPTURAS DA TELA DE DESCANSO (descanso.h): vitrine e relogio, sem rede.
// Os titulos sao os do catalogo embarcado (deploy/app/art/catalogo.txt), com
// fundo, logo e sinopse de verdade. Julgar o resultado na captura da TV antes
// de aprovar: aqui a fonte e o gama sao os do Mac.
//
//   bash tests/descanso_shot.sh
#include "descanso.h"
#include "nvtmp.h"
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

static SDL_Window *win;

static void salvarTela(const char *nome) {
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
  SDL_FreeSurface(s); free(pix);
}

// Seis titulos do catalogo embarcado: "fundo|poster|logo|titulo|genero|meta|cl|sinopse".
static void semearCatalogo(void) {
  static CatItem itens[6];
  char linha[2048];
  FILE *f = fopen("deploy/app/art/catalogo.txt", "r");
  int n = 0;
  assert(f);
  memset(itens, 0, sizeof itens);
  while (n < 6 && fgets(linha, sizeof linha, f)) {
    char *c[8], *p = linha;
    int k;
    for (k = 0; k < 8; k++) { c[k] = p; p = p ? strchr(p, '|') : NULL; if (p) *p++ = 0; }
    if (!c[7]) continue;
    c[7][strcspn(c[7], "\n")] = 0;
    snprintf(itens[n].imdb, sizeof itens[n].imdb, "tt-desc-%02d", n);
    snprintf(itens[n].tipo, sizeof itens[n].tipo, "movie");
    snprintf(itens[n].backdrop, sizeof itens[n].backdrop, "deploy/app/art/%s", c[0]);
    snprintf(itens[n].poster, sizeof itens[n].poster, "deploy/app/art/%s", c[1]);
    snprintf(itens[n].logo, sizeof itens[n].logo, "deploy/app/art/%s", c[2]);
    snprintf(itens[n].titulo, sizeof itens[n].titulo, "%s", c[3]);
    snprintf(itens[n].genero, sizeof itens[n].genero, "%s", c[4]);
    snprintf(itens[n].meta, sizeof itens[n].meta, "%s", c[5]);
    snprintf(itens[n].sinopse, sizeof itens[n].sinopse, "%s", c[7]);
    n++;
  }
  fclose(f);
  cat_definir_tudo(itens, n, NULL, 0);
}

static void rodar(int estilo, float segundos, const char *nome) {
  unsigned t0 = SDL_GetTicks();
  int i, n = (int)(segundos * 60.0f);
  esmaecer_estilo(estilo);
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    descanso_quadro(t0 + (unsigned)(i * 1000 / 60), 1.0f / 60.0f, 1, estilo, DESC_FONTE_CATALOGO);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    descanso_desenhar(t0 + (unsigned)(i * 1000 / 60));
    if (i == n - 1) salvarTela(nome);
    SDL_GL_SwapWindow(win);
  }
  printf("  %s  (preenchimento %.2f telas, %d desenhos)\n", nome, gfx_fill, gfx_n_rect);
  descanso_quadro(t0, 0.016f, 0, estilo, 0);   // sai: a proxima rodada entra de novo
}

int main(int argc, char **argv) {
  const char *pre = argc > 1 ? argv[1] : nvt("/nuvio-descanso");
  char nome[700];
  SDL_GLContext gl;
  // Funcoes puras.
  assert(descanso_item_serve(1, 1, 0, 0, DESC_FONTE_CATALOGO));
  assert(!descanso_item_serve(0, 1, 1, 50, DESC_FONTE_CATALOGO));   // sem fundo nao entra
  assert(!descanso_item_serve(1, 1, 0, 0, DESC_FONTE_LISTA));
  assert(descanso_item_serve(1, 1, 1, 0, DESC_FONTE_LISTA) && descanso_item_serve(1, 1, 0, 30, DESC_FONTE_LISTA));

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: tela de descanso", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  dados_iniciar(NULL);
  { const char *d = dados_dir(), *tmp = getenv("NUVIO_TESTE_DIR");
    if (!tmp || !d || strcmp(d, tmp)) { fprintf(stderr, "rode por tests/descanso_shot.sh\n"); return 1; } }
  { FILE *f; char c[700];
    snprintf(c, sizeof c, "%s/ajustes.txt", dados_dir());
    f = fopen(c, "w"); assert(f);
    if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
    fclose(f); ajustes_dir(dados_dir()); }
  semearCatalogo();

  snprintf(nome, sizeof nome, "%s-vitrine-entrando.bmp", pre);
  rodar(ESM_ESTILO_VITRINE, 3.0f, nome);
  snprintf(nome, sizeof nome, "%s-vitrine.bmp", pre);
  rodar(ESM_ESTILO_VITRINE, 7.0f, nome);
  assert(descanso_indice_em_tela() < 0 || descanso_indice_em_tela() < 6);
  snprintf(nome, sizeof nome, "%s-relogio.bmp", pre);
  rodar(ESM_ESTILO_RELOGIO, 3.0f, nome);
  printf("descanso: capturas ok\n");
  return 0;
}
