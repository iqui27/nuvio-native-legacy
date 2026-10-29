// CAPTURAS DA TELA CACA E DO AVISO DE CONQUISTA NA ILHA, sem rede.
//
// As cacadas e as listas de filmes sao gravadas na pasta descartavel no
// formato que caca.c guarda em disco (cacas-p1.txt, caca-l-*.txt) com os
// pôsteres do PACOTE, e o "visto" entra pelo historico do catalogo — o mesmo
// caminho do Trakt e da conta. Nada de URL de rede, nada de dado de pessoa.
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano.
//   NUVIO_SHOT_FONTE=3 bash tests/caca_shot.sh [prefixo]
#include "caca.h"
#include "cacaui.h"
#include "catalogo.h"
#include "conquista.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "ilha.h"
#include "ilhasinais.h"
#include "rail_shot.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SDL_Window *win;
static const char *base;
static int comTela = 1, comIlha;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  if (comTela) cacaui_evento(&e); else ilha_evento(&e);
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
    caca_passo(agora, 1);
    cacaui_atualizar(1.0f / 60.0f, agora);
    glClearColor(0.05f, 0.05f, 0.05f, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (comTela) { cacaui_desenhar(agora); rail_shot_desenhar(MENU_PERFIL); }
    else gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, tex_obter_larg("deploy/app/art/03.jpg", 1920), GFX_HERO,
                  0, 0, 0, 0, 0, 0, 0, 1.0f);
    if (comIlha) {
      ilha_relogio_visivel(1);
      ilha_ancorar(96, 36, 0);
      ilha_desenhar(agora);
      ilhasinais_passo(agora);
    }
    SDL_GL_SwapWindow(win);
    SDL_Delay(4);
  }
}

static void captura(const char *nome, int h) {
  unsigned char *pix = malloc(1920 * (size_t)h * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < h; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", cam);
}

// Uma lista de filmes no formato de caca.c, com pôsteres do pacote.
static void lista(const char *arq, const char *nome, const char *foto, const char *const *tit,
                  const long *data, int n, int base0) {
  char *b = malloc(64 * 1024), *p;
  int i;
  assert(b);
  p = b + sprintf(b, "v1 %lld\n#nome\t%s\n#foto\t%s\n", (long long)time(NULL), nome, foto);
  for (i = 0; i < n; i++)
    p += sprintf(p, "%d\ttt%07d\t%ld\t0\tdeploy/app/art/poster/%02d.jpg\t%s\n",
                 base0 + i, base0 + i, data[i], (base0 + i) % 10, tit[i]);
  assert(dados_gravar(arq, b));
  free(b);
}

static void visto(int id) {
  char im[16];
  snprintf(im, sizeof im, "tt%07d", id);
  cat_historico_definir_id(im, "movie", 1);
}

int main(int argc, char **argv) {
  static const char *VERB[11] = { "The Mexican", "The Ring", "Pirates of the Caribbean", "Dead Man's Chest",
    "At World's End", "Rango", "The Lone Ranger", "A Cure for Wellness", "Mouse Hunt", "The Weather Man", "Spinning Gold" };
  static const long VD[11] = { 20010302, 20021018, 20030709, 20060707, 20070525, 20110304,
                               20130703, 20170224, 19971219, 20051028, 20230331 };
  static const char *VIL[10] = { "Un 32 août sur terre", "Maelström", "Polytechnique", "Incendies",
    "Prisoners", "Enemy", "Sicario", "Arrival", "Blade Runner 2049", "Dune" };
  static const long VID[10] = { 19980828, 20000922, 20090206, 20100917, 20130920, 20131206,
                                20150919, 20161111, 20171006, 20210922 };
  static const char *LOTR[3] = { "The Fellowship of the Ring", "The Two Towers", "The Return of the King" };
  static const long LD[3] = { 20011219, 20021218, 20031217 };
  const char *dir = getenv("NUVIO_DADOS"), *fonte = getenv("NUVIO_SHOT_FONTE");
  char caminho[700], cacas[2048];
  SDL_GLContext gl;
  FILE *f;
  int i;
  base = argc > 1 ? argv[1] : "/tmp/nuvio-caca";
  assert(dir && *dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w"); assert(f);
  fprintf(f, "idioma %d\nselected_theme 2\n", getenv("NUVIO_SHOT_EN") ? 1 : 0);
  if (fonte && *fonte) fprintf(f, "fonteInterface %d\n", atoi(fonte));
  fclose(f);
  ajustes_dir(dir);
  dados_iniciar("deploy/app");
  assert(!strcmp(dados_dir(), dir));

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: Caca", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(96);
  gfx_icones_dir("deploy/app/art");
  rail_shot_aplicar();

  // 0. Sem cacada nenhuma: o estado vazio, com a legenda dos degraus.
  caca_iniciar();
  cacaui_abrir(-1);
  quadros(80);
  captura("0-vazio", 1080);

  // As tres cacadas: Verbinski 2/11 (a captura do dono), Villeneuve 9/10 (vai
  // fechar no item 5) e a saga do Senhor dos Aneis completa.
  lista("caca-l-dir_1704.txt", "Gore Verbinski", "deploy/app/art/poster/04.jpg", VERB, VD, 11, 100);
  lista("caca-l-dir_137427.txt", "Denis Villeneuve", "deploy/app/art/poster/07.jpg", VIL, VID, 10, 200);
  lista("caca-l-saga_119.txt", "O Senhor dos Anéis", "deploy/app/art/poster/02.jpg", LOTR, LD, 3, 300);
  snprintf(cacas, sizeof cacas,
           "v1\t1\t1704\t-\t0\t%d\t1\t%d\t0\tGore Verbinski\tdeploy/app/art/poster/04.jpg\n"
           "v1\t1\t137427\t-\t0\t%d\t1\t%d\t0\tDenis Villeneuve\tdeploy/app/art/poster/07.jpg\n"
           "v1\t3\t119\t-\t0\t%d\t1\t%d\t1\tO Senhor dos Anéis\tdeploy/app/art/poster/02.jpg\n",
           CACA_F_SEMEADA, CONQ_PRIMEIRO,
           CACA_F_SEMEADA | CACA_F_FILEIRA, CONQ_PRIMEIRO | CONQ_METADE | CONQ_QUASE,
           CACA_F_SEMEADA | CACA_F_COLECAO, CONQ_PRIMEIRO | CONQ_METADE | CONQ_QUASE | CONQ_COMPLETA);
  assert(dados_gravar("cacas-p1.txt", cacas));
  visto(101); visto(102);
  for (i = 0; i < 9; i++) visto(200 + i);
  for (i = 0; i < 3; i++) visto(300 + i);
  caca_perfil_trocado();
  for (i = 0; i < 200 && caca_n_filmes(0) == 0; i++) quadros(1);
  assert(caca_n() == 3 && caca_n_filmes(0) == 11);

  // 1. A lista e o detalhe de Verbinski: "Voce viu 2 de 11 · faltam 9".
  cacaui_abrir(0);
  quadros(120);
  { CacaInfo c; caca_info(0, &c); assert(c.vistos == 2 && c.total == 11); }
  captura("1-verbinski", 1080);
  // 2. Foco num pôster que falta.
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  quadros(40);
  captura("2-poster", 1080);
  // 3. Os botoes.
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  quadros(40);
  captura("3-botoes", 1080);
  // 4. Villeneuve, com a sugestao "faltam 1" la em cima.
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  tecla(SDLK_DOWN);
  quadros(60);
  captura("4-villeneuve", 1080);
  // 5. "+ Nova trilha".
  tecla(SDLK_UP); tecla(SDLK_UP);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  quadros(60);
  captura("5-trilhas", 1080);
  tecla(SDLK_ESCAPE);
  quadros(10);

  // 6. O AVISO: Dune visto, Villeneuve fecha. A ilha diz uma vez, sem player.
  comTela = 0; comIlha = 1;
  visto(209);
  { Uint32 t0 = SDL_GetTicks(); CacaInfo c;   // debounce de 2 s, depois o voo
    do { quadros(10); caca_info(1, &c); } while (!(c.ganhos & CONQ_COMPLETA) && SDL_GetTicks() - t0 < 8000);
    quadros(90); }
  { CacaInfo c; caca_info(1, &c); assert(c.ganhos & CONQ_COMPLETA); }
  captura("6-aviso", 250);
  tecla(SDLK_s);
  quadros(80);
  captura("7-aviso-modal", 760);
  printf("caca_shot: ok\n");
  return 0;
}
