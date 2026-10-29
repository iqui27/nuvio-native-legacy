// Capturas da ABERTURA DO CINEMA: a barra na cor do logo (como a Imersiva), os
// primeiros quadros com o OSD calado e o OSD so depois de uma tecla. Fora da
// suite (precisa de janela GL e de olho humano):
//
//   bash tests/player_cineabre_shot.sh /tmp/nv-player-live-shots/cineabre
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
#include "corviva.h"
#include "artehero.h"
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

static Uint32 relogio = 100000;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio = SDL_GetTicks();   // o OSD mede o tempo parado pelo relogio real
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    corviva_quadro(1.f / 60, CORVIVA_IMERSIVA, 1, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(.62f, .66f, .74f, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    SDL_Delay(16);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-player-live-shots/cineabre");
  char nome[600];
  SDL_Event ev = {0};
  CatItem c;
  CorvivaPaleta pl;
  float r = 0, g = 0, b = 0;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: cineabre", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
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

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
  snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
  snprintf(c.logo, sizeof c.logo, "deploy/app/art/logo/04.png");
  snprintf(c.meta, sizeof c.meta, "2024 · 2h 07min · Suspense · Drama");
  cat_definir(&c, 1);

  // A paleta do logo, extraida do PNG como o fio de decode faz. Pega o primeiro
  // logo do pacote que tem cor (logo branco/preto nao tem paleta).
  { int n;
    memset(&pl, 0, sizeof pl);
    for (n = 0; n < 40 && !pl.ok; n++) {
      SDL_Surface *t, *s;
      snprintf(c.logo, sizeof c.logo, "deploy/app/art/logo/%02d.png", n);
      t = IMG_Load(c.logo); if (!t) continue;
      s = SDL_ConvertSurfaceFormat(t, SDL_PIXELFORMAT_ABGR8888, 0); SDL_FreeSurface(t); assert(s);
      memset(&pl, 0, sizeof pl);
      corviva_extrair(s->pixels, s->w, s->h, s->pitch, &pl);
      SDL_FreeSurface(s); }
    assert(pl.ok);
    printf("logo: %s\n", c.logo);
    cat_definir(&c, 1);
    { const char *chave = artehero_logo_sessao(&c); assert(chave); corviva_anotar(chave, &pl); } }

  player_abrir(0, NULL);
  player_shot_carregando(1);   // abertura: a barra do cinema aparece
  quadros(60);
  assert(corviva_logo_acento(&r, &g, &b));
  assert(r == pl.acento[0] && g == pl.acento[1] && b == pl.acento[2]);
  printf("cor do logo: %.3f %.3f %.3f\n", r, g, b);
  // A barra indeterminada corre em 1,6 s: quatro quadros pegam o brilho em pontos diferentes.
  { int k; for (k = 0; k < 4; k++) {
      quadros(25);
      snprintf(nome, sizeof nome, "%s-1-abertura%d.bmp", saida, k); salvar(nome); } }

  // O filme comeca: nenhum OSD, nem depois de um tempo.
  player_shot_carregando(0);
  player_erro_fonte(); player_limpar_erro_fonte();
  quadros(30);
  assert(!player_controles_visiveis());
  snprintf(nome, sizeof nome, "%s-2-comeco.bmp", saida); salvar(nome);
  quadros(120);
  assert(!player_controles_visiveis());
  snprintf(nome, sizeof nome, "%s-3-depois.bmp", saida); salvar(nome);

  // Tecla: o OSD volta.
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RIGHT; player_evento(&ev);
  quadros(40);
  assert(player_controles_visiveis());
  snprintf(nome, sizeof nome, "%s-4-tecla.bmp", saida); salvar(nome);
  puts("player_cineabre_shot: ok");
  return 0;
}
