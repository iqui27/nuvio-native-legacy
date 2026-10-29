// Capturas da trava de serie (travaui.c): o modal "Travar série com…", a folha
// de espera do episodio alem do limite (com o PIN) e o Perfil › "Travas de
// série". Sem rede: perfis e travas semeados direto no modelo (trava.h).
#include "travaui.h"
#include "trava.h"
#include "catalogo.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Uint32 relogio = 100000;
static void quadro(int desenhar, int perfil, SDL_Window *win, const char *path) {
  int q;
  for (q = 0; q < 70; q++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    travaui_atualizar(1.0f / 60.0f, relogio);
    travaui_perfil_atualizar(1.0f / 60.0f);
    glClearColor(.05f, .055f, .07f, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (perfil) travaui_perfil_desenhar(140.0f, 60.0f, 1640.0f, 900.0f, 1.0f);
    if (desenhar) travaui_desenhar(relogio);
    if (q == 69 && path) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      int y;
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, path) == 0); SDL_FreeSurface(s); free(pix);
      printf("captura: %s\n", path);
    }
    SDL_GL_SwapWindow(win);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; travaui_evento(&e);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS"), *out = argc > 1 ? argv[1] : "/tmp/nuvio-trava";
  char path[700]; FILE *f; SDL_Window *win; SDL_GLContext gl;
  static CatItem it; static CatEp eps[8];
  TravaPerfil p[3]; int i, outros[2] = { 2, 3 };
  assert(dir && *dir);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir); f = fopen(path, "w"); assert(f);
  fprintf(f, "selected_theme 2\n");
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(f);
  dados_iniciar(dir); ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0); IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("trava", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");

  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "tt11280740"); snprintf(it.tipo, sizeof it.tipo, "series");
  snprintf(it.titulo, sizeof it.titulo, "Severance");
  snprintf(it.poster, sizeof it.poster, "deploy/app/art/poster/00.jpg");
  snprintf(it.backdrop, sizeof it.backdrop, "deploy/app/art/00.jpg");
  cat_definir_tudo(&it, 1, NULL, 0);
  for (i = 0; i < 8; i++) { eps[i].temporada = 1; eps[i].episodio = i + 1;
    snprintf(eps[i].thumb, sizeof eps[i].thumb, "deploy/app/art/0%d.jpg", i % 4); }
  cat_definir_episodios(0, eps, 8);

  memset(p, 0, sizeof p);
  p[0].indice = 1; p[0].primario = 1; p[0].temPin = 1; snprintf(p[0].nome, sizeof p[0].nome, "Henrique");
  p[1].indice = 2; p[1].temPin = 1; snprintf(p[1].nome, sizeof p[1].nome, "Jerry");
  p[2].indice = 3; snprintf(p[2].nome, sizeof p[2].nome, "Sandra");
  trava_definir_conta("shot");
  trava_definir_perfis(p, 3, 1);

  // 1. O modal, com Jerry escolhido e o foco em Sandra.
  assert(travaui_abrir(cat_item(0), 0));
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN); tecla(SDLK_RIGHT);
  snprintf(path, sizeof path, "%s-modal.png", out); quadro(1, 0, win, path);
  // Cria (Separados, PIN) e anda as posicoes: eu no E4, Jerry no E2, Sandra no E3.
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  quadro(0, 0, win, NULL);
  assert(trava_n() == 1);
  trava_definir_membros(trava_i(0)->id, outros, 2);
  trava_passo("tt11280740", 1, 1, 4, 0);
  trava_posicao(trava_i(0)->id, 2, 1, 2, 1);
  trava_posicao(trava_i(0)->id, 3, 1, 3, 1);
  // 2. A folha de espera do E5, e o PIN.
  assert(travaui_abrir_espera(0, 1, 5));
  snprintf(path, sizeof path, "%s-espera.png", out); quadro(1, 0, win, path);
  tecla(SDLK_RETURN); tecla(SDLK_1); tecla(SDLK_2);
  snprintf(path, sizeof path, "%s-pin.png", out); quadro(1, 0, win, path);
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE); quadro(0, 0, win, NULL);
  // 3. Perfil, pagina 2.
  travaui_perfil_entrar();
  snprintf(path, sizeof path, "%s-perfil.png", out); quadro(0, 1, win, path);
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  return 0;
}
