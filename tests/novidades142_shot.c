// Captura do cartao de novidades da 1.4.2 (PT, EN, foco em "Depois" e
// animacoes reduzidas) SEM janela visivel — janela GL escondida, desenho num
// FBO, quadro por glReadPixels, como tests/explorar_shot.c. Antes das
// capturas, confere as regras do cartao por evento de tecla.
#include "novidades142.h"
#include "diagnostico.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void ajustesDeTeste(int ingles, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n", ingles, tema, reduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades142_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

static void apagarMarcas(void) {
  dados_apagar("novidades-142.txt");
  dados_apagar("novidades-14-celebracao.txt");
  dados_apagar("diagnostico-otimizacao-intro.cfg");
}

static void regras(void) {
  // OK com o foco inicial = rodar o diagnostico.
  apagarMarcas();
  novidades142_abrir();
  tecla(SDLK_RETURN);
  assert(!novidades142_aberto());
  assert(novidades142_pediu_diagnostico() == 1);
  assert(novidades142_pediu_diagnostico() == 0);   // consumido
  assert(existe("novidades-142.txt") && existe("novidades-14-celebracao.txt"));
  assert(existe("diagnostico-otimizacao-intro.cfg"));
  assert(!diagnostico_intro_aberto());
  // Voltar = Depois: fecha, marca as duas versoes, nao pede diagnostico e nao
  // grava a apresentacao do diagnostico como vista.
  apagarMarcas();
  novidades142_abrir();
  tecla(SDLK_ESCAPE);
  assert(!novidades142_aberto());
  assert(novidades142_pediu_diagnostico() == 0);
  assert(existe("novidades-142.txt") && existe("novidades-14-celebracao.txt"));
  assert(!existe("diagnostico-otimizacao-intro.cfg"));
  // Esquerda + OK = Depois.
  apagarMarcas();
  novidades142_abrir();
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!novidades142_aberto());
  assert(novidades142_pediu_diagnostico() == 0);
  assert(!novidades142_pendente());
  // Com a apresentacao dispensada nesta sessao, primeira_vez nao a reabre.
  diagnostico_intro_primeira_vez();
  assert(!diagnostico_intro_aberto());
  puts("PASS: regras do cartao da 1.4.2");
}

static void captura(const char *nome) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    novidades142_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.025f, 0.025f, 0.035f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    novidades142_desenhar(SDL_GetTicks());
    glFinish();
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
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_Delay(2);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-novidades142";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;

  regras();

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades142-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");

  ajustesDeTeste(0, 0, 2);
  novidades142_abrir();
  snprintf(nome, sizeof nome, "%s-pt.bmp", saida);
  captura(nome);

  ajustesDeTeste(1, 0, 2);
  novidades142_abrir();
  snprintf(nome, sizeof nome, "%s-en.bmp", saida);
  captura(nome);

  ajustesDeTeste(1, 0, 6);
  novidades142_abrir();
  tecla(SDLK_LEFT);
  snprintf(nome, sizeof nome, "%s-en-depois.bmp", saida);
  captura(nome);

  ajustesDeTeste(0, 1, 5);
  novidades142_abrir();
  snprintf(nome, sizeof nome, "%s-reduzido.bmp", saida);
  captura(nome);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 1.4.2 gravadas.");
  return 0;
}
