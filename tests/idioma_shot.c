// CAPTURA DO SELETOR DE IDIOMA E DE TELAS TRADUZIDAS, para OLHAR: os 22 idiomas de
// 2026-09 trouxeram escritas (grego, cirilico, vietnamita, japones, chines) que
// nenhum teste de tabela mede — a linha existe, o glifo pode ser um quadrado.
//
//   bash tests/idioma_shot.sh /tmp/nuvio-idioma-shots 27,28,29,24,26
//
// Cada numero e um IDIOMA_* (idiomacod.h). Para cada um grava, em <saida>-<N>-*.bmp:
//   -seletor   Ajustes com o foco na linha "Idioma" (e o rotulo nativo dele)
//   -metadados Ajustes com o foco em "Idioma dos metadados"
//   -tmdb-basico  a linha "Titulo e sinopse" (texto de ajuda mais longo)
// NUVIO_SEM_RESERVA_DE_SISTEMA=1 mostra o que o WASM da Samsung desenha (so a
// CJK embarcada). Fora da suite: precisa de janela GL e de olho humano.
#include "ajustes.h"
#include "nvtmp.h"
#include "rail_shot.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern int ajustes_teste_focar_opcao(int op);
extern int ajustes_teste_op_por_chave(const char *chave);

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ajustes_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  for (int i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-idioma");
  const char *lista = argc > 2 ? argv[2] : "27";
  char dir[256], arq[300], nome[600], *copia, *tok, *sp = NULL;
  SDL_Window *w;
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: idiomas", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

  snprintf(dir, sizeof dir, "%s/nuvio-idioma-shot-%d", nvt_dir(), (int)getpid());
  mkdir(dir, 0755);
  copia = strdup(lista);
  for (tok = strtok_r(copia, ",", &sp); tok; tok = strtok_r(NULL, ",", &sp)) {
    int lg = atoi(tok);
    FILE *f;
    // Um ajustes.txt por idioma, lido pelo caminho real. Sem "idiomaAutoLocal"
    // o idioma e escolha manual.
    snprintf(arq, sizeof arq, "%s/ajustes.txt", dir);
    f = fopen(arq, "w");
    assert(f);
    fprintf(f, "idioma %d\n", lg);
    fclose(f);
    ajustes_dir(dir);
    ajustes_iniciar();
    assert(ajustes_idioma() == lg);
    assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("idioma")));
    snprintf(nome, sizeof nome, "%s-%d-seletor.bmp", saida, lg);
    captura(nome, w);
    assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("tmdb_language")));
    snprintf(nome, sizeof nome, "%s-%d-metadados.bmp", saida, lg);
    captura(nome, w);
    assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("tmdb_use_basic_info")));
    snprintf(nome, sizeof nome, "%s-%d-tmdb-basico.bmp", saida, lg);
    captura(nome, w);
  }
  free(copia);
  (void)tecla;
  snprintf(arq, sizeof arq, "%s/ajustes.txt", dir);
  unlink(arq);
  rmdir(dir);
  return 0;
}
