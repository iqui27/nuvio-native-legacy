// Capturas do PAINEL DE PAUSA (pauseOverlayEnabled) em tela cheia. Fora da
// suite (precisa de janela GL e de olho humano):
//
//   bash tests/player_pausa_shot.sh /tmp/nv-player-live-shots/pausa
//
// Variaveis: NUVIO_SHOT_VIDRO=1 liga a interface de vidro; NUVIO_SHOT_4K=1
// desenha num FBO de 3840x2160 (o drawable de uma TV que concede 4K) com o
// mesmo layout de 1920x1080 — o painel tem de cobrir a tela inteira nos dois.
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
    SDL_Delay(1);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-player-live-shots/pausa");
  char nome[600];
  const char *e4 = getenv("NUVIO_SHOT_4K");
  SDL_Event ev = {0};
  if (e4 && *e4 == '1') { LW = 3840; LH = 2160; }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: pausa", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  // Idioma e acento pelo caminho de verdade, como em social_shot: portugues e
  // OCEANO (2) por padrao; NUVIO_SHOT_EN=1 e NUVIO_SHOT_THEME=<n> trocam.
  { char caminho[700]; FILE *f;
    const char *en = getenv("NUVIO_SHOT_EN"), *tema = getenv("NUVIO_SHOT_THEME");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma %d\nselected_theme %d\n", en && *en == '1', tema && *tema ? atoi(tema) : 2);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);

  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    // Uma arte do pacote faz de "quadro parado": e nela que se ve se o veu cobre
    // a tela inteira ou so a faixa de baixo.
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    snprintf(c.meta, sizeof c.meta, "2024 · 2h 07min · Suspense · Drama");
    snprintf(c.sinopse, sizeof c.sinopse,
      "Uma restauradora de arte herda um casarao onde cada espelho guarda uma "
      "lembranca que ninguem deveria ter. Quando o ultimo morador desaparece, ela "
      "precisa decidir o que vale mais: a verdade ou a familia.");
    { static const char *nomes[] = { "Helena Duarte", "Rafael Monteiro", "Ines Carvalho",
                                     "Otavio Lins", "Bianca Serra", "Caio Amaral" };
      int i; for (i = 0; i < 6; i++) snprintf(c.elenco[i].nome, sizeof c.elenco[i].nome, "%s", nomes[i]);
      c.nElenco = 6; }
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();   // poe o player a TOCAR sem video
  quadros(20);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_PAUSE; player_evento(&ev);   // pausa
  quadros(60);
  snprintf(nome, sizeof nome, "%s-antes-5s.bmp", saida); salvar(nome);
  // 5 s parado + a mola de entrada.
  { int i; for (i = 0; i < 6; i++) { relogio += 1000; quadros(1); } }
  quadros(90);
  assert(pausao_visivel());
  snprintf(nome, sizeof nome, "%s-painel.bmp", saida); salvar(nome);
  puts("player_pausa_shot: ok");
  return 0;
}
