// Capturas da BARRA DE ESTILO da legenda (faixas.c): com o foco no Estilo a
// folha vira uma faixa no topo e a previa da legenda aparece onde vai tocar.
// Fora da suite (precisa de janela GL e de olho humano):
//
//   bash tests/faixas_estilo_shot.sh /tmp/nv-faixas-estilo
//
// NUVIO_SHOT_VIDRO=1 liga a interface de vidro.
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "pausao.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;

static void salvar(const char *nome) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int y; unsigned char *p, *t;
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

// Luminancia media de uma faixa horizontal do quadro salvo (para provar que o
// veu cobre a tela inteira e nao so a faixa de baixo).
static Uint32 relogio = 100000;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(.62f, .66f, .74f, 1); glClear(GL_COLOR_BUFFER_BIT);   // "cena clara"
    player_desenhar(relogio);
    faixas_desenhar(relogio);
    SDL_Delay(1);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; faixas_evento(&ev);
  ev.type = SDL_KEYUP; faixas_evento(&ev);
  quadros(2);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-faixas-estilo");
  char nome[600];
  int i;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: estilo", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\n");
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  quadros(20);
  faixas_abrir_em(1);
  quadros(60);
  snprintf(nome, sizeof nome, "%s-folha.bmp", saida); salvar(nome);
  assert(!faixas_estilo_topo());
  faixas_aba_estilo(1);              // explicit Estilo tab; RIGHT now opens subtitle options
  quadros(60);
  assert(faixas_estilo_topo());
  snprintf(nome, sizeof nome, "%s-barra-pos4.bmp", saida); salvar(nome);
  tecla(SDLK_DOWN);                  // linha 2, coluna 1: "Posicao"
  for (i = 0; i < 4; i++) tecla(SDLK_RETURN);   // 4 de 8 -> 8 de 8
  quadros(30);
  snprintf(nome, sizeof nome, "%s-barra-pos8.bmp", saida); salvar(nome);
  tecla(SDLK_RETURN);                // 8 de 8 -> 1 de 8 (volta ao inicio)
  quadros(30);
  snprintf(nome, sizeof nome, "%s-barra-pos1.bmp", saida); salvar(nome);
  tecla(SDLK_LEFT);                  // coluna 1 da grade: volta a lista
  quadros(60);
  assert(!faixas_estilo_topo());
  snprintf(nome, sizeof nome, "%s-volta-lista.bmp", saida); salvar(nome);
  puts("faixas_estilo_shot: ok");
  return 0;
}
