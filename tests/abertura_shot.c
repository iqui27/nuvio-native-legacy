// Abertura do app (2.0 N1): os tres estilos e os dois logos.
//  - Confere o tempo de cada estilo (parada minima e saida) sem janela de
//    espera: a funcao recebe o relogio como argumento.
//  - Grava, em NUVIO_ABERTURA_SAIDA (ou /tmp/nuvio-abertura), a abertura de
//    verdade por logo: parada e meio da saida, sobre uma "home" de mentira.
#include "abertura.h"
#include "nvtmp.h"
#include "ajustes.h"
#include "anim.h"
#include "gfx.h"
#include "logoapp.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

static void grava(const char *nome, SDL_Window *w) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(w);
  printf("captura: %s\n", nome);
}

static void config(const char *dir, int logo, int estilo, int reduzidas) {
  char cam[600];
  FILE *f;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  f = fopen(cam, "w");
  assert(f);
  fprintf(f, "logoAppLocal %d\naberturaAppLocal %d\nanimacoes %d\n", logo, estilo, reduzidas);
  fclose(f);
  ajustes_dir(dir);
}

// Um quadro: o fundo de mentira, depois a abertura.
static int quadro(Uint32 agora, int pendentes) {
  int r;
  glClearColor(0.10f, 0.16f, 0.26f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 300, 200, 700, 380 }, 0.04f, 0.9f, 0.55f, 0.2f, 1.0f);
  gfx_cor((GfxRect){ 1100, 300, 500, 500 }, 0.04f, 0.3f, 0.5f, 0.9f, 1.0f);
  r = abertura_desenhar(agora, 1.0f / 60.0f, pendentes);
  return r;
}

// Roda a abertura ate `ate` ms (quadros de 16 ms), pendentes = 1 ate `soltaEm`.
static Uint32 roda(Uint32 t0, Uint32 de, Uint32 ate, Uint32 soltaEm) {
  Uint32 t;
  for (t = de; t <= ate; t += 16) quadro(t0 + t, t < soltaEm);
  return t;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-abertura");
  char dir[NVT_MAX];
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  int logo, estilo;
  Uint32 t0 = 100000;

  nvt_set(dir, sizeof dir, "/nuvio-ab-XXXXXX");
  assert(mkdtemp(dir));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("abertura", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  logoapp_iniciar("deploy/app/art");

  // TEMPOS: Padrao e So esmaece seguram 350 ms e saem em 560; Direto nao segura
  // e sai em 220.
  for (estilo = 0; estilo < 3; estilo++) {
    int logo2 = estilo == 1;   // varia o logo: o tempo nao depende dele
    config(dir, logo2, estilo, 0);
    assert(ajustes_abertura() == estilo && ajustes_logo_app() == logo2);
    abertura_iniciar("deploy/app/art");
    assert(abertura_ativa());
    quadro(t0, 0);                           // quadro 0: nada em voo
    if (estilo == 2) {
      assert(abertura_ativa());              // saindo, ainda cobrindo
      quadro(t0 + 100, 0); assert(abertura_ativa());
      quadro(t0 + 221, 0); assert(!abertura_ativa());   // acabou em 220 ms
    } else {
      quadro(t0 + 200, 0); assert(abertura_ativa());
      quadro(t0 + 349, 0); assert(abertura_ativa());     // ainda na parada minima
      quadro(t0 + 350, 0); assert(abertura_ativa());     // comeca a sair agora
      quadro(t0 + 350 + 500, 0); assert(abertura_ativa());
      quadro(t0 + 350 + 561, 0); assert(!abertura_ativa());   // 560 ms depois
    }
    printf("estilo %d: tempos ok\n", estilo);
  }
  // O teto: com artes em voo para sempre, sai em 1500 ms em qualquer estilo.
  config(dir, 0, 0, 0);
  abertura_iniciar("deploy/app/art");
  quadro(t0, 1); quadro(t0 + 1499, 1); assert(abertura_ativa());
  quadro(t0 + 1500, 1); quadro(t0 + 1500 + 561, 1); assert(!abertura_ativa());

  // CAPTURAS: cada logo, parada e meio da saida (Padrao).
  for (logo = 0; logo < 2; logo++) {
    config(dir, logo, 0, 0);
    abertura_iniciar("deploy/app/art");
    quadro(t0, 1);
    roda(t0, 16, 300, 100000);
    snprintf(nome, sizeof nome, "%s-%s-parada.png", saida, logo ? "classico" : "novo");
    grava(nome, w);
    roda(t0, 316, 340, 100000);
    quadro(t0 + 400, 0);                      // solta: a saida comeca
    roda(t0, 416, 640, 100000);
    quadro(t0 + 640, 0);
    snprintf(nome, sizeof nome, "%s-%s-saida.png", saida, logo ? "classico" : "novo");
    grava(nome, w);
  }
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: abertura: tempos dos tres estilos e capturas dos dois logos");
  return 0;
}
