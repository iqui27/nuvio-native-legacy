// CAPTURA DO MENU LATERAL (rail + aberto) E DA FOLHA DE FAIXAS, sem interacao e sem rede.
// Serve ao "Interface de vidro" (compile com -DNV_VIDRO_TESTE para forcar ligado).
//
// Mesmo motivo de tests/ajustes_shot.c: interface de TV julgada so por codigo
// sai ilegivel a 3 m. E esta tela em especial nao da para alcancar na TV por
// tecla injetada — ela abre pelo botao de episodios DO PLAYER, e chegar la
// exige reproduzir algo. Uma captura headless e repetivel e nao gasta fonte.
#include "menu.h"
#include "nvtmp.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "ilha.h"
#include "shot_arte.h"
#include "dados.h"
#include "perfis.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int qual = 0;   // 0 = menu lateral, 1 = folha de faixas
static int nq = 50;   // quadros por captura (poucos = a barra no meio da abertura)
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  if (qual) faixas_evento(&e); else menu_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  // A ilha mede o passo pelo relogio de parede (SDL_GetTicks): sem a espera
  // os 50 quadros passam em poucos ms e o texto dela ainda nao entrou.
  for (i = 0; i < nq; i++) {
    SDL_PumpEvents();
    if (!qual) SDL_Delay(16);
    gfx_novo_quadro();   // per-frame glass bookkeeping, as in app.c
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    if (qual) faixas_atualizar(1.0f / 60.0f, SDL_GetTicks()); else menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Glass UI: a arte atras (shot_arte.h) e a ilha do relogio por cima, nas
    // mesmas guardas de app.c (ilha_posicionar decide o canto com o menu).
    if (!qual) shot_arte_desenhar(.30f);
    // NUVIO_SHOT_POSTERS=1: uma fileira de cartazes por baixo da barra do topo,
    // o caso em que o vidro precisa mostrar o que passa atras.
    if (!qual && getenv("NUVIO_SHOT_POSTERS")) {
      int p;
      for (p = 0; p < 9; p++) {
        float h = (float)p / 9.0f;
        gfx_cor((GfxRect){ 60.0f + p * 200.0f, 0, 180.0f, 330.0f }, 0.04f,
                .25f + .6f * h, .75f - .5f * h, .35f + .4f * (p & 1), 1);
        gfx_cor((GfxRect){ 60.0f + p * 200.0f, 140.0f, 180.0f, 60.0f }, 0, 1, 1, 1, .85f);
      }
    }
    { int n0 = gfx_n_rect; double f0 = gfx_fill_gpu, f1 = gfx_fill_gpu_mist;
      Uint64 t0;
      glFinish(); t0 = SDL_GetPerformanceCounter();
      if (qual) faixas_desenhar(SDL_GetTicks()); else menu_desenhar(SDL_GetTicks());
      glFinish();
      // Custo do quadro so do menu: quads, preenchimento da GPU (telas) e ms.
      if (i == nq - 1)
        printf("custo: quads=%d fill=%.3f fill_mist=%.3f ms=%.3f\n", gfx_n_rect - n0,
               gfx_fill_gpu - f0, gfx_fill_gpu_mist - f1,
               (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency()); }
    if (!qual) {
      ilha_relogio_visivel(1);
      ilha_posicionar(0);
      ilha_desenhar(SDL_GetTicks());
    }
    if (i == nq - 1) {
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
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-vidro-menu");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;

  // Mesmo accent Ocean da captura social; o tema e ajustavel pelo ambiente.
  { const char *dir = getenv("NUVIO_DADOS");
    if (dir && *dir) {
      const char *temaEnv = getenv("NUVIO_SHOT_THEME");
      char caminho[700];
      FILE *f;
      int tema = temaEnv && *temaEnv ? atoi(temaEnv) : 2;
      if (tema < 0 || tema >= 16) tema = 2;
      snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
      f = fopen(caminho, "w"); assert(f);
      // NUVIO_SHOT_RAIL=recolhida: sem a rail de icones (collapseSidebar 0 =
      // "Recolhida" na ordem de V_RAIL); o padrao da captura e a rail fixa.
      { const char *rail = getenv("NUVIO_SHOT_RAIL");
        { const char *idi = getenv("NUVIO_SHOT_IDIOMA");   // 0 pt, 1 en, 4 ru, 5 fr, 6 de
          fprintf(f, "idioma %d\nselected_theme %d\ncollapseSidebar %d\n", idi && *idi ? atoi(idi) : 0, tema,
                rail && !strcmp(rail, "recolhida") ? 0 : 1); } }
      // NUVIO_SHOT_LAYOUT=0|1|2: Moderna, Padrao ou Dinamica (homeLayoutLocal);
      // o menu muda de forma com o layout da home.
      { const char *lay = getenv("NUVIO_SHOT_LAYOUT");
        if (lay && *lay) fprintf(f, "homeLayoutLocal %d\n", atoi(lay)); }
      // NUVIO_SHOT_TOPO=1: a barra no topo (menuTopoLocal, #461).
      if (getenv("NUVIO_SHOT_TOPO")) fputs("menuTopoLocal 1\n", f);
      shot_arte_material(f);
      fclose(f);
      // O perfil do mockup (avatar roxo com a inicial e o nome), pelo mesmo
      // cache que o app le no arranque: o rodape do menu deixa de ser "Sua conta".
      snprintf(caminho, sizeof caminho, "%s/perfis.txt", dir);
      f = fopen(caminho, "w"); assert(f);
      fputs("1\t0\t1\t0\t#7c5cff\tHenrique\t\t\t\n", f);
      fclose(f);
      snprintf(caminho, sizeof caminho, "%s/perfil.txt", dir);
      f = fopen(caminho, "w"); assert(f); fputs("1\n", f); fclose(f);
      dados_iniciar("deploy/app/art");
      perfis_carregar_ativo();
      ajustes_dir(dir);
      ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_OPAC / _FOSCO
    } }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao dos episodios", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
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

  menu_iniciar();
  // NUVIO_SHOT_PILULA=1: the top bar at rest, as on the home (A27 top chrome).
  if (getenv("NUVIO_SHOT_PILULA")) menu_pilula_mostrar(1.0f);
  snprintf(nome, sizeof nome, "%s-rail.bmp", saida);
  captura(nome, w);
  menu_abrir();
  if (getenv("NUVIO_SHOT_TOPO")) {   // NUVIO_SHOT_FOCO=n: quantos passos para a direita (padrao 2)
    const char *fo = getenv("NUVIO_SHOT_FOCO");
    int p, np = fo && *fo ? atoi(fo) : 2;
    for (p = 0; p < np; p++) tecla(SDLK_RIGHT);
  }
  else { tecla(SDLK_DOWN); tecla(SDLK_DOWN); }
  // A ilha do menu no meio da abertura: a largura cresce, o rotulo entra.
  nq = 8;
  snprintf(nome, sizeof nome, "%s-abrindo.bmp", saida);
  captura(nome, w);
  nq = 50;
  snprintf(nome, sizeof nome, "%s-menu.bmp", saida);
  captura(nome, w);
  menu_fechar();
  // NUVIO_SHOT_SO_MENU=1: so o menu (sem a folha de faixas), para comparar com
  // os mockups do Glass UI sem gravar o que nao vai ser olhado.
  if (getenv("NUVIO_SHOT_SO_MENU")) {
    tex_encerrar(); txt_encerrar(); gfx_encerrar();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
    puts("PASS: capturas do menu gravadas.");
    return 0;
  }
  qual = 1;
  faixas_reiniciar();
  faixas_abrir_em(1);
  snprintf(nome, sizeof nome, "%s-faixas.bmp", saida);
  captura(nome, w);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas do menu e das faixas gravadas.");
  return 0;
}
