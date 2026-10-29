// Headless 1920x1080 PNG captures of rating and quick-tag states. Nao
// entra na suite: precisa de janela GL e de olho humano.
//
//   bash tests/reacao_shot.sh /pasta/de/saida
//
// 1. reacao-rec.bmp     veio de recomendacao, envio ligado, foco em Gostei
// 2. reacao-simples.bmp sem origem, foco em "Mais ou menos", contagem a meio
// 3. reacao-detalhe.bmp a linha discreta da pagina do titulo (pendente)
#include "reacao.h"
#include "nvtmp.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "shot_arte.h"
#include "corviva.h"

static GLuint shotFbo;
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void captura(const char *nome, SDL_Window *win) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(win);
  printf("captura: %s\n", nome);
}

// Match the app frame lifecycle and use real art behind floating panels.
static void cena(void) {
  SDL_Delay(8);
  gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER, shotFbo);
  glViewport(0, 0, 1920, 1080);
  gfx_sem_recorte();
  glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  assert(shot_arte_carregar());
  shot_arte_desenhar(getenv("NV_ILHA_SHOTS") ? 0 : .35f);
}

static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  return e;
}

int main(int argc, char **argv) {
  const char *pasta = argc > 1 ? argv[1] : nvt("/nuvio-reacao-shot");
  const char *dir = getenv("NUVIO_DADOS");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  Uint32 agora = 10000;
  int i;
  assert(dir && *dir);
  dados_iniciar(dir);
  dados_gravar("ajustes.txt", "idioma 0\nvidroLocal 1\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");
  ajustes_iniciar();
  ajustes_dir(dir);
  ajustes_definir_vidro(0);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: reacao", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  GLuint fbo, cor;
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  shotFbo = fbo;
  glGenTextures(1, &cor); glBindTexture(GL_TEXTURE_2D, cor);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cor, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  { char abs[1024]; assert(realpath("deploy/app/art", abs)); gfx_icones_dir(abs); }

  if (getenv("NV_ILHA_SHOTS")) {
    ajustes_definir_vidro(1);
    CatItem ci = {0};
    snprintf(ci.imdb, sizeof ci.imdb, "tt0970866");
    snprintf(ci.tipo, sizeof ci.tipo, "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "Entrando numa Fria Maior Ainda");
    snprintf(ci.poster, sizeof ci.poster, "%s", getenv("NV_ILHA_THUMB"));
    // Load the real thumb first, then use the approved artwork palette.
    CorvivaPaleta p = {0}; p.ok = 1; p.acento[0] = .728f; p.acento[1] = .657f; p.acento[2] = .610f;
    const char *states[] = {"a-repouso", "b-reacao", "c-estrelas", "d-salvo", "e-episodio", "f-varredura-01", "f-varredura-02", "f-varredura-03"};
    for (int state = 0; state < 8; state++) {
      reacao_fechar();
      if (state == 4) {
        snprintf(ci.imdb, sizeof ci.imdb, "tt0903747");
        snprintf(ci.tipo, sizeof ci.tipo, "series");
        reacao_nota_definir("tt0903747:1:1", 9);
        assert(reacao_episodio_abrir(&ci, 1, 1));
        SDL_Event ev = tecla(SDLK_RETURN); reacao_evento(&ev, 1);
        for (int j = 0; j < 15; j++) { ev = tecla(SDLK_LEFT); reacao_evento(&ev, 1); }
        ev = tecla(SDLK_RETURN); reacao_evento(&ev, 1);
        ev = tecla(SDLK_RIGHT); reacao_evento(&ev, 1);
      } else {
        snprintf(ci.imdb, sizeof ci.imdb, "tt097086%d", state);
        snprintf(ci.tipo, sizeof ci.tipo, "movie");
        assert(reacao_detalhe_abrir(&ci));
        if (state == 1) { SDL_Event ev = tecla(SDLK_LEFT); reacao_evento(&ev, 1); }
        if (state >= 2) {
          SDL_Event ev = tecla(SDLK_UP); reacao_evento(&ev, 1);
          ev = tecla(SDLK_RIGHT); reacao_evento(&ev, 1);
          if (state == 3) { ev = tecla(SDLK_RETURN); reacao_evento(&ev, 1); }
        }
      }
      for (i = 0; i < 80; i++) {
        tex_novo_quadro(); tex_bombear(8); txt_novo_quadro(); cena();
        corviva_anotar(ci.poster, &p);
        reacao_detalhe_dica(&ci, 1);
      }
      if (state >= 5) {
        // Restart an actual star-entry sweep and seek the production clock.
        SDL_Event ev = tecla(SDLK_ESCAPE); reacao_evento(&ev, 1);
        assert(reacao_detalhe_abrir(&ci));
        Uint32 clock = SDL_GetTicks();
        reacao_player_atualizar(1, clock, NULL, 0, 0, 0, 0, 0, 0);
        ev = tecla(SDLK_UP); reacao_evento(&ev, 1);
        ev = tecla(SDLK_RIGHT); reacao_evento(&ev, 1);
        const int elapsed[] = {100, 300, 450};
        reacao_player_atualizar(1, clock + elapsed[state - 5], NULL, 0, 0, 0, 0, 0, 0);
        tex_novo_quadro(); txt_novo_quadro(); cena();
        reacao_desenhar(clock + elapsed[state - 5], 1032);
      }
      snprintf(nome, sizeof nome, "%s/app-%s.png", pasta, states[state]); captura(nome, w);
    }
    tex_encerrar(); txt_encerrar(); gfx_encerrar();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
    return 0;
  }

  // 1. Veio de recomendacao.
  reacao_player_atualizar(0.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
  reacao_teste_abrir("tt0111161", "Um Sonho de Liberdade", "movie", 42, "Ana", 1);
  for (i = 0; i < 60; i++) {
    agora += 16;
    reacao_player_atualizar(1.0f / 60.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
    tex_novo_quadro(); tex_bombear(6); txt_novo_quadro();
    cena();
    reacao_desenhar(agora, 1080.0f - 48.0f);
    if (i == 59) { snprintf(nome, sizeof nome, "%s/reacao-rec.png", pasta); captura(nome, w); }
    else SDL_GL_SwapWindow(w);
  }
  // 2. Sem origem, foco no meio, metade da contagem.
  reacao_fechar();
  reacao_teste_abrir("tt0068646", "O Poderoso Chefão", "movie", 0, "", 0);
  reacao_player_atualizar(0.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
  { SDL_Event e = tecla(SDLK_RIGHT); reacao_evento(&e, 0); }
  for (i = 0; i < 240; i++) {
    agora += 16;
    reacao_player_atualizar(1.0f / 60.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
    tex_novo_quadro(); tex_bombear(6); txt_novo_quadro();
    cena();
    reacao_desenhar(agora, 1080.0f - 48.0f);
    if (i == 239) { snprintf(nome, sizeof nome, "%s/reacao-simples.png", pasta); captura(nome, w); }
    else SDL_GL_SwapWindow(w);
  }
  // Detail captures live in detail_eps_shot.c, inside the real page and scale scope.
  // 4. Fileira de estrelas em foco (UP), 2,5 estrelas.
  reacao_fechar();
  reacao_teste_abrir("tt0068646", "O Poderoso Chefão", "movie", 0, "", 0);
  reacao_player_atualizar(0.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
  { SDL_Event e = tecla(SDLK_UP); reacao_evento(&e, 0);
    for (i = 0; i < 3; i++) { e = tecla(SDLK_LEFT); reacao_evento(&e, 0); } }
  for (i = 0; i < 240; i++) {
    agora += 16;
    reacao_player_atualizar(1.0f / 60.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
    tex_novo_quadro(); tex_bombear(6); txt_novo_quadro();
    cena();
    reacao_desenhar(agora, 1080.0f - 48.0f);
    if (i == 239) { snprintf(nome, sizeof nome, "%s/reacao-estrelas.png", pasta); captura(nome, w); }
    else SDL_GL_SwapWindow(w);
  }
  { SDL_Event e = tecla(SDLK_RETURN); reacao_evento(&e, 0);
    for (int n = 0; n < 4; n++) { e = tecla(SDLK_UP); reacao_evento(&e, 0); }
    e = tecla(SDLK_RETURN); reacao_evento(&e, 0);
    e = tecla(SDLK_RIGHT); reacao_evento(&e, 0);
  }
  for (i = 0; i < 30; i++) {
    tex_novo_quadro(); tex_bombear(6); txt_novo_quadro(); cena();
    reacao_desenhar(agora, 1032);
    if (i == 29) { snprintf(nome, sizeof nome, "%s/reacao-tags.png", pasta); captura(nome, w); }
  }
  { SDL_Event e = tecla(SDLK_ESCAPE); reacao_evento(&e, 0); }
  // 5. The same editor with glass enabled.
  ajustes_definir_vidro(1);
  assert(ajustes_vidro());
  { CatItem ci = {0};
    snprintf(ci.imdb, sizeof ci.imdb, "tt0068646");
    snprintf(ci.tipo, sizeof ci.tipo, "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "O Poderoso Chefão");
    assert(reacao_detalhe_abrir(&ci));
    for (i = 0; i < 60; i++) {
      tex_novo_quadro(); tex_bombear(6); txt_novo_quadro(); cena();
      reacao_detalhe_dica(&ci, 1);
    }
    snprintf(nome, sizeof nome, "%s/reacao-glass.png", pasta); captura(nome, w);
    SDL_Event e = tecla(SDLK_RETURN); reacao_evento(&e, 1);
    e = tecla(SDLK_LEFT); reacao_evento(&e, 1);
    e = tecla(SDLK_RETURN); reacao_evento(&e, 1);
    e = tecla(SDLK_RIGHT); reacao_evento(&e, 1);
    for (i = 0; i < 30; i++) {
      tex_novo_quadro(); tex_bombear(6); txt_novo_quadro(); cena();
      reacao_detalhe_dica(&ci, 1);
    }
    snprintf(nome, sizeof nome, "%s/reacao-tags-glass.png", pasta); captura(nome, w);
  }
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas do cartao de reacao gravadas.");
  return 0;
}
