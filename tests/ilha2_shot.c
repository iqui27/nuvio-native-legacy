// CAPTURA DA ILHA COM CARTOES (ilha.h): atividade ao vivo na pilula, o modal
// nascendo dela (CH+/AZUL), o painel de Salvos nascendo do modal (seta para a
// direita) e recolhendo para a pilula, a estreia alternando com a atividade, o
// modal da estreia, vidro desligado, ancorada a direita e animacoes reduzidas.
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano.
#include "ilha.h"
#include "nvtmp.h"
#include "salvospainel.h"
#include "ajustes.h"
#include "anim.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;
static const char *base;
static int direita, homeLoading;
// RELOGIO FALSO (> 0): cada quadro vale 16 ms, por mais que a captura demore.
// Sem ele o glReadPixels de cada marco come quadros da mola do voo.
static Uint32 falso;

static void captura(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.bmp", base, nome);
  assert(SDL_SaveBMP(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

// O mesmo roteamento de app.c para o pedido de Salvos do modal.
static void pedidos(void) {
  IlhaCartao c;
  int qual, o = ilha_pediu(&c, &qual);
  if (o == ILHA_PEDIU_SALVOS) {
    float x, y, w, h;
    if (!spainel_aberto()) {
      if (ilha_rect(&x, &y, &w, &h)) spainel_abrir_de(x, y, w, h); else spainel_abrir();
    }
    ilha_modal_fechar(1);
  }
}

static void quadros(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = falso ? (falso += 16u) : SDL_GetTicks();
    GLuint fundo;
    tex_novo_quadro();
    tex_bombear(3);
    pedidos();
    spainel_atualizar(1.0f / 60.0f, agora);
    glClearColor(0.05f, 0.05f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fundo = tex_obter("deploy/app/art/00.jpg");
    if (fundo) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundo, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    if (spainel_visivel()) spainel_desenhar(agora);
    if (homeLoading) ilha_atividade("Carregando fileiras…", -1);
    ilha_relogio_visivel(ajustes_relogio_ligado());
    if (direita) ilha_ancorar(1920 - 64, 36, 1);
    { float x = 0, y = 0, w = 0, h = 0;
      int ok = ilha_rect(&x, &y, &w, &h);
      spainel_recolher_para(ok, x, y, w, h); }
    ilha_coberta(spainel_da_ilha());
    ilha_desenhar(agora);
    if (i == n - 1 && nome) captura(nome);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  if (!ilha_evento(&e) && !spainel_aberto()) {
    if (k == SDLK_s && ilha_cartao_na_tela()) ilha_modal_abrir();
  } else if (spainel_aberto() && k == SDLK_ESCAPE) spainel_fechar();
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700];
  SDL_GLContext gl;
  FILE *f;
  IlhaCartao vivo, est;
  base = argc > 1 ? argv[1] : nvt("/nuvio-ilha2");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n");
  fclose(f);
  ajustes_dir(dir);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: ilha2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);

  // Synthetic loading counters for visual review, never live TV evidence.
  homeLoading = 1;
  ilha_atividade_detalhes("Carregamento da Home",
    "Carregando fileiras…\nTempo: 8 s · Add-ons consultados: 4 de 6\nFileiras atualizadas: 3 · Falhas: 0\nVocê pode continuar navegando enquanto a Home atualiza.");
  quadros(45, "home-loading");
  assert(ilha_atividade_expansivel());
  assert(ilha_modal_abrir());
  quadros(70, "home-loading-details");
  tecla(SDLK_RIGHT);
  assert(ilha_modal_aberto());
  { IlhaCartao ignored; int qual; assert(!ilha_pediu(&ignored, &qual)); }
  tecla(SDLK_RETURN);
  assert(!ilha_modal_aberto());
  homeLoading = 0;
  ilha_atividade_detalhes(NULL, NULL);
  quadros(45, NULL);

  memset(&vivo, 0, sizeof vivo);
  snprintf(vivo.chave, sizeof vivo.chave, "vivo:tt12637874:1:3");
  snprintf(vivo.imdb, sizeof vivo.imdb, "tt12637874");
  vivo.serie = 1; vivo.t = 1; vivo.e = 3;
  snprintf(vivo.titulo, sizeof vivo.titulo, "Fallout");
  snprintf(vivo.epNome, sizeof vivo.epNome, "The Head");
  snprintf(vivo.sinopse, sizeof vivo.sinopse, "In a future, post-apocalyptic Los Angeles brought about by nuclear decimation, citizens must live in underground bunkers to protect themselves from radiation, mutants and bandits.");
  snprintf(vivo.poster, sizeof vivo.poster, "deploy/app/art/poster/00.jpg");
  snprintf(vivo.logo, sizeof vivo.logo, "deploy/app/art/logo/00.png");
  snprintf(vivo.arte, sizeof vivo.arte, "deploy/app/art/ep/00_1_03.jpg");
  vivo.progresso = 0.42f; vivo.restanteMin = 32;

  memset(&est, 0, sizeof est);
  snprintf(est.chave, sizeof est.chave, "estreia:agenda:tt1:2026-10-01");
  snprintf(est.avisoId, sizeof est.avisoId, "agenda:tt1:2026-10-01");
  snprintf(est.imdb, sizeof est.imdb, "tt1");
  est.serie = 1; est.t = 2; est.e = 5; est.progresso = -1.0f;
  snprintf(est.titulo, sizeof est.titulo, "Widow's Bay");
  snprintf(est.epNome, sizeof est.epNome, "The Lighthouse");
  snprintf(est.sinopse, sizeof est.sinopse, "A skeptical mayor leads the superstitious residents of a cursed New England island.");
  snprintf(est.poster, sizeof est.poster, "deploy/app/art/poster/01.jpg");
  snprintf(est.logo, sizeof est.logo, "deploy/app/art/logo/01.png");
  snprintf(est.arte, sizeof est.arte, "deploy/app/art/01.jpg");
  snprintf(est.quando, sizeof est.quando, "hoje");

  quadros(40, "0-relogio");
  ilha_cartao(ILHA_VIVO, &vivo);
  quadros(80, "1-vivo");
  tecla(SDLK_s);
  quadros(5, "2-modal-morfando");
  quadros(60, "3-modal");
  tecla(SDLK_RIGHT);
  quadros(20, "3b-modal-foco-detalhes");
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);   // a direita do ultimo: o painel
  quadros(4, "4-painel-nascendo");
  quadros(40, "5-painel");
  tecla(SDLK_ESCAPE);
  quadros(5, "6-painel-recolhendo");
  quadros(40, "7-de-volta");
  // Sem cartao: AZUL abre o painel da pilula do relogio.
  ilha_cartao(ILHA_VIVO, NULL);
  quadros(60, NULL);
  { float x, y, w, h; if (ilha_rect(&x, &y, &w, &h)) spainel_abrir_de(x, y, w, h); }
  quadros(4, "8-painel-do-relogio");
  spainel_fechar();
  quadros(40, NULL);
  ilha_cartao(ILHA_VIVO, &vivo);
  ilha_cartao(ILHA_ESTREIA, &est);
  // Espera a vez da estreia (alterna a cada ILHA_ALTERNA_MS).
  { int k = 0;
    while (((SDL_GetTicks() / ILHA_ALTERNA_MS) % 2) != 1 && k++ < 600) quadros(1, NULL); }
  quadros(70, "9-estreia");
  tecla(SDLK_s);
  quadros(60, "10-modal-estreia");
  tecla(SDLK_ESCAPE);
  quadros(40, NULL);
  ajustes_definir_vidro(0);
  ilha_cartao(ILHA_ESTREIA, NULL);
  quadros(60, "11-solido-vivo");
  tecla(SDLK_s);
  quadros(60, "12-solido-modal");
  tecla(SDLK_ESCAPE);
  quadros(40, NULL);
  ajustes_definir_vidro(1);
  direita = 1;
  quadros(60, "13-direita-vivo");
  tecla(SDLK_s);
  quadros(60, "14-direita-modal");
  tecla(SDLK_s);   // AZUL de novo no modal: o painel
  quadros(4, "15-direita-painel-nascendo");
  quadros(40, NULL);
  tecla(SDLK_ESCAPE);
  quadros(40, NULL);
  // Animacoes reduzidas: troca seca, um quadro.
  anim_politica_reduzida = 1;
  tecla(SDLK_s);
  quadros(1, "16-reduzida-modal-1quadro");
  tecla(SDLK_ESCAPE);
  quadros(1, "17-reduzida-fechado-1quadro");
  anim_politica_reduzida = 0;

  // MINIMIZAR NA ILHA (ilha_minimizar): o player saiu no meio; o quadro do
  // video (a arte do cartao) nasce em tela cheia e encolhe ate a mini capa.
  // Quadros da sequencia para OLHAR, e o final com o cartao na pilula.
  direita = 0;
  ilha_cartao(ILHA_VIVO, NULL);
  quadros(60, NULL);
  ilha_cartao(ILHA_VIVO, &vivo);
  falso = SDL_GetTicks();
  assert(ilha_minimizar("deploy/app/art/00.jpg"));
  assert(ilha_minimizando());
  { static const int marcos[] = { 1, 4, 8, 12, 16, 20, 26, 34, 44, 56 };
    int i, feito = 0;
    for (i = 0; i < (int)(sizeof marcos / sizeof *marcos); i++) {
      char nome[40];
      snprintf(nome, sizeof nome, "18-min-q%02d", marcos[i]);
      quadros(marcos[i] - feito, nome);
      feito = marcos[i];
    } }
  quadros(90, "19-min-final");
  assert(!ilha_minimizando());
  assert(ilha_cartao_na_tela());
  // Layout Dinamica / ancorada a direita: pousa na capa do outro canto.
  direita = 1;
  ilha_cartao(ILHA_VIVO, NULL);
  quadros(60, NULL);
  ilha_cartao(ILHA_VIVO, &vivo);
  assert(ilha_minimizar("deploy/app/art/00.jpg"));
  quadros(14, "20-min-direita-q14");
  quadros(90, "21-min-direita-final");
  assert(!ilha_minimizando());
  falso = 0;
  // Animacoes reduzidas: nada voa; o primeiro quadro ja e a home com a pilula.
  direita = 0;
  ilha_cartao(ILHA_VIVO, NULL);
  quadros(60, NULL);
  anim_politica_reduzida = 1;
  ilha_cartao(ILHA_VIVO, &vivo);
  assert(ilha_minimizar("deploy/app/art/00.jpg"));
  assert(!ilha_minimizando());
  quadros(1, "22-min-reduzida-1quadro");
  anim_politica_reduzida = 0;

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas da ilha com cartoes gravadas.");
  return 0;
}
