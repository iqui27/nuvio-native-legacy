// Capturas do cartao "O que achou?" (reacao.c), em BMP, sem interacao. Nao
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
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(win);
  printf("captura: %s\n", nome);
}

// Um "quadro de filme" de mentira: degrade escuro, para o cartao ter o que cobrir.
static void cena(void) {
  int i;
  glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  for (i = 0; i < 6; i++)
    gfx_cor((GfxRect){ 0, (float)i * 180.0f, 1920, 180 }, 0.0f,
            0.16f + 0.04f * (float)i, 0.12f + 0.02f * (float)i, 0.10f, 1.0f);
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
  ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: reacao", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  // 1. Veio de recomendacao.
  reacao_player_atualizar(0.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
  reacao_teste_abrir("tt0111161", "Um Sonho de Liberdade", "movie", 42, "Ana", 1);
  for (i = 0; i < 60; i++) {
    agora += 16;
    reacao_player_atualizar(1.0f / 60.0f, agora, NULL, 0, 0, 0, 0, 0, 0);
    tex_novo_quadro(); txt_novo_quadro();
    cena();
    reacao_desenhar(agora, 1080.0f - 48.0f);
    if (i == 59) { snprintf(nome, sizeof nome, "%s/reacao-rec.bmp", pasta); captura(nome, w); }
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
    tex_novo_quadro(); txt_novo_quadro();
    cena();
    reacao_desenhar(agora, 1080.0f - 48.0f);
    if (i == 239) { snprintf(nome, sizeof nome, "%s/reacao-simples.bmp", pasta); captura(nome, w); }
    else SDL_GL_SwapWindow(w);
  }
  // 3. Pendente na pagina do titulo.
  { CatItem ci;
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "tt0903747");
    snprintf(ci.tipo, sizeof ci.tipo, "series");
    snprintf(ci.titulo, sizeof ci.titulo, "Breaking Bad");
    reacao_fechar();
    // Fim de temporada com a duracao estavel: abre, e o VOLTAR deixa pendente.
    for (i = 0; i < 100; i++) {
      agora += 100;
      reacao_player_atualizar(0.1f, agora, &ci, 1, 2300, 2400, 0, 0, 0);
    }
    { SDL_Event e = tecla(SDLK_ESCAPE); reacao_evento(&e, 0); }
    reacao_fechar();
    for (i = 0; i < 30; i++) {
      tex_novo_quadro(); txt_novo_quadro();
      cena();
      reacao_detalhe_dica(&ci, 1.0f);
      if (i == 29) { snprintf(nome, sizeof nome, "%s/reacao-detalhe.bmp", pasta); captura(nome, w); }
      else SDL_GL_SwapWindow(w);
    } }
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas do cartao de reacao gravadas.");
  return 0;
}
