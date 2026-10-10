// Captura do cartao de novidades da versao atual (novidades_cartao.h) SEM
// janela visivel (janela GL escondida, FBO, glReadPixels), como
// tests/novidades202_shot.c: cada pagina, a previa em varios momentos das
// cenas, todas as plataformas, ingles e outros idiomas,
// animacoes reduzidas. No fim, a lista cabe acima do rodape em TODOS os
// idiomas e, com a fonte da TV (NUVIO_SHOT_FONTE=3), nenhuma frase precisa de
// reticencias.
#include "novidades_cartao.h"
#include "apoio.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "idiomacod.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static GLuint fbo, fboTex;

static void ajustesDeTeste(int idioma, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n", idioma, tema, reduzidas);
  // NUVIO_SHOT_FONTE=3 (TXT_FAMILIA_*): a fonte da interface da TV
  // (Montserrat na TCL do dono), mais larga que a Inter do Mac.
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novcartao_evento(&e);
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novcartao_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  novcartao_desenhar(SDL_GetTicks());
  glFinish();
}

static void grava(const char *nome) {
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
  printf("captura: %s\n", nome);
}

// Abre o cartao, espera as artes, anda ate a pagina `pagina` (OK em
// Continuar), poe o relogio da previa em `seg` e grava.
static void captura(const char *saida, const char *nome, float seg, int pagina) {
  char cam[700];
  int i, p;
  novcartao_abrir();
  for (i = 0; i < 900 && !novcartao_previa_pronta(); i++) { quadro(0.0f); SDL_Delay(4); }
  assert(novcartao_previa_pronta());
  for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);   // entrada do cartao e da lista
  for (p = 0; p < pagina; p++) { tecla(SDLK_RETURN); for (i = 0; i < 40; i++) quadro(1.0f / 60.0f); }
  assert(novcartao_pagina() == pagina);
  novcartao_teste_relogio(seg);
  // Quadros parados: icones, texto e a copia desfocada terminam de carregar.
  for (i = 0; i < 12; i++) { quadro(0.0f); SDL_Delay(2); }
  if (pagina == novcartao_paginas() - 1) {
    assert(novcartao_teste_discord());
    assert(!strcmp(apoio_url(APOIO_DISCORD), NV_URL_DISCORD));
    assert(apoio_n() == 2 && apoio_qual(2) == -1);
  }
  snprintf(cam, sizeof cam, "%s/%s.png", saida, nome);
  grava(cam);
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);
  for (i = 0; i < 20; i++) quadro(1.0f / 60.0f);
}

// Em TODOS os idiomas e plataformas a lista de cada pagina termina acima do
// rodape com folga e, com a fonte da TV, nenhuma frase precisa de reticencias.
static void cabeEmTodos(void) {
  static const unsigned PLAT[] = { NOV_LG, NOV_TPK, NOV_WGT, NOV_ANDROID, NOV_OUTRAS };
  int idi, i, p, k, comCorte = 0;
  novcartao_teste_medir(1);
  for (k = 0; k < 5; k++) {
    novcartao_teste_plataforma(PLAT[k]);
    for (idi = 0; idi < IDIOMA_N; idi++) {
      int c = 0, n, desenhados = 0;
      ajustesDeTeste(idi, 0, 2);
      novcartao_abrir();
      n = novcartao_paginas();
      for (p = 0; p < n - 1; p++) {
        for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);
        // Sem cenas, medir o texto da lista tambem.
        c += novcartao_teste_cortadas();
        desenhados += novcartao_teste_desenhados();
        if (idi == 0 && novcartao_teste_vao_max() > 0) {
          assert(fabsf(novcartao_teste_vao_min() - 12.0f) < 0.01f);
          assert(fabsf(novcartao_teste_vao_max() - 12.0f) < 0.01f);
        }
        assert(novcartao_paginas() == n);
        // Todas as cenas passam pela legenda quando o conteudo tem previa.
        for (int cena = 0; cena < novcartao_cenas(); cena++) {
          novcartao_teste_relogio(novcartao_teste_inicio_cena(cena) + 1.0f); quadro(0.0f);
          c += novcartao_teste_cortadas();
        }
        tecla(SDLK_RETURN);
      }
      assert(desenhados == novcartao_itens_visiveis());
      for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);
      assert(novcartao_teste_discord());
      if (novcartao_teste_folga() < 27.99f)
        printf("plataforma %u, idioma %d (%s): folga %.2f px\n", PLAT[k], idi, idioma_iso(idi), novcartao_teste_folga());
      assert(novcartao_teste_folga() >= 27.99f);   // lista exatamente cheia nao e erro de float
      if (c) { printf("plataforma %u, idioma %d (%s): %d frase(s) com reticencias\n", PLAT[k], idi, idioma_iso(idi), c); comCorte++; }
      tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);
      for (i = 0; i < 20; i++) quadro(1.0f / 60.0f);
    }
  }
  novcartao_teste_medir(0);
  novcartao_teste_plataforma(0);
  if (getenv("NUVIO_SHOT_FONTE")) assert(comCorte == 0);
  printf("PASS: a lista acaba acima do rodape nos %d idiomas e 5 plataformas (%d com reticencias)\n", IDIOMA_N, comCorte);
  puts("PASS: vaos de 12px em pt; Discord desenhado em todos os idiomas e plataformas");
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-novcartao";
  const char *dir = getenv("NUVIO_DADOS");
  const char *so = getenv("NUVIO_SHOT_SO");   // "medir": so a medida, sem capturas
  SDL_Window *w;
  SDL_GLContext gl;
  int cena;
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  novcartao_dir("deploy/app/art");

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novcartao-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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

  if (!so || strcmp(so, "medir")) {
    static const int idiomas[] = { 0, 1, IDIOMA_DE, IDIOMA_JA, IDIOMA_RU };
    static const unsigned plataformas[] = { NOV_LG, NOV_TPK, NOV_WGT, NOV_ANDROID, NOV_OUTRAS };
    static const char *nomes[] = { "lg", "tpk", "wgt", "android", "outras" };
    char nome[100];
    for (int plat = 0; plat < 5; plat++) {
      novcartao_teste_plataforma(plataformas[plat]);
      for (int idi = 0; idi < (plat == 0 ? 5 : 1); idi++) {
        ajustesDeTeste(idiomas[idi], 0, idi == 1 ? 5 : 2);
        for (int pg = 0; pg < novcartao_paginas() - 1; pg++) {
          snprintf(nome, sizeof nome, "%s-%s-pagina-%d", nomes[plat], idioma_iso(idiomas[idi]), pg + 1);
          captura(saida, nome, 1.2f, pg);
        }
        for (cena = 0; cena < novcartao_cenas(); cena++) {
          snprintf(nome, sizeof nome, "%s-%s-cena-%d", nomes[plat], idioma_iso(idiomas[idi]), cena + 1);
          captura(saida, nome, novcartao_teste_inicio_cena(cena) + 1.2f, 0);
        }
        snprintf(nome, sizeof nome, "%s-%s-apoio", nomes[plat], idioma_iso(idiomas[idi]));
        captura(saida, nome, 1.2f, novcartao_paginas() - 1);
      }
    }
    novcartao_teste_plataforma(NOV_LG);
    ajustesDeTeste(0, 1, 2);
    for (int pg = 0; pg < novcartao_paginas(); pg++) {
      snprintf(nome, sizeof nome, "lg-pt-reduzido-pagina-%d", pg + 1);
      captura(saida, nome, 0.7f, pg);
    }
    for (cena = 0; cena < novcartao_cenas(); cena++) {
      snprintf(nome, sizeof nome, "lg-pt-reduzido-cena-%d", cena + 1);
      captura(saida, nome, novcartao_teste_inicio_cena(cena) + 0.7f, 0);
    }
  }

  cabeEmTodos();

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas do cartao de novidades gravadas.");
  return 0;
}
