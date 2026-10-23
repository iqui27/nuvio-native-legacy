// CAPTURA DA ENQUETE NA ILHA (enquete.h): convite, opcoes, resultado, bolinha de
// "Agora nao" e opt-out com Desfazer, contra o servidor de mentira
// (enquete_servidor.inc), pelo caminho de verdade (enquete_passo + ilha).
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano. Cada
// captura e um PNG so da faixa de cima (a altura do quadro do mockup), para
// comparar lado a lado com o mockup renderizado na mesma escala.
//
// FUNDO: NUVIO_SHOT_FUNDOS=<pasta> com bgNN.png (o fundo do quadro NN do
// mockup, 1920x1080, sem a ilha) — assim o vidro fica sobre a MESMA arte. Sem
// a pasta, a arte embarcada 00.jpg. NUVIO_SHOT_VIDRO=0 = solido.
#include <assert.h>
#include "nvtmp.h"
#include <time.h>
#include <unistd.h>
#define NV_REC_URL "http://fake.test"
#define ESPERA_MS 0u
#define ENQUETE_ARTE "tests/fixtures/arte/enquete-logo-480x270.png"
#define rede_baixar_st         teste_get
#define rede_postar_st         teste_post
#define recomenda_cabecalhos   teste_cabecalhos
#include "../src/enquete.c"
#include "enquete_servidor.inc"
#include "ilha.h"
#include "ilhaacao.h"
#include "ilhasinais.h"
#include "dados.h"
#include "anim.h"
#include "ajustes.h"
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
static GLuint fundoTex;
static int fundoN = -1, direita;

static void fundo(int n) {
  const char *dir = getenv("NUVIO_SHOT_FUNDOS");
  char cam[600];
  SDL_Surface *s, *t;
  if (n == fundoN) return;
  fundoN = n;
  if (fundoTex) { glDeleteTextures(1, &fundoTex); fundoTex = 0; }
  if (dir && *dir) snprintf(cam, sizeof cam, "%s/bg%02d.png", dir, n);
  else snprintf(cam, sizeof cam, "deploy/app/art/00.jpg");
  s = IMG_Load(cam);
  if (!s) return;
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  if (!t) return;
  glGenTextures(1, &fundoTex);
  glBindTexture(GL_TEXTURE_2D, fundoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
}

// Grava a faixa [0, h) da tela em PNG (sem BMP intermediario: disco cheio).
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
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static int relogio = 1;
static const char *atividade;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    // O orcamento de rasterizacao de texto e POR QUADRO: sem isto, as linhas
    // novas depois das primeiras centenas voltam vazias (o rotulo do botao).
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(3);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    if (atividade) ilha_atividade(atividade, -1.0f);
    ilha_relogio_visivel(relogio);
    // A margem do mockup: pilula em x 96 (esquerda) ou a 64 da borda direita.
    if (direita) ilha_ancorar(1920 - 64, 36, 1); else ilha_ancorar(96, 36, 0);
    ilha_desenhar(agora);
    ilhasinais_passo(agora);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ilha_evento(&e);
}

static void limpar(void) {
  int q;
  while (ilha_aviso_vez()[0]) ilha_retirar(ilha_aviso_vez());
  for (q = 0; q < ILHA_N_CARTOES; q++) ilha_cartao(q, NULL);
  ilha_modal_fechar(1);
  atividade = NULL;
  direita = 0;
  quadros(45);
}


static void novaSessao(const char *id, long long dur) {
  gen++;
  memset(&cur, 0, sizeof cur);
  buscando = 0; proxima = 0; pediuVoto = 0; abrindo = 0; nVistos = -1;
  ilha_retirar(CH_CONVITE); ilha_retirar(CH_OPCOES); ilha_retirar(CH_RESULTADO);
  ilha_ponto_enquete(0); ponto = 0;
  memset(&S, 0, sizeof S);
  snprintf(S.id, sizeof S.id, "%s", id);
  S.ativa = 1; S.fim = (long long)time(NULL) + dur;
  S.base[0] = 5; S.base[1] = 14; S.base[2] = 3;
  iniciouEm = 1;
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700];
  SDL_GLContext gl;
  FILE *f;
  base = argc > 1 ? argv[1] : nvt("/nuvio-enquete");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n");
  fclose(f);
  ajustes_dir(dir);
  dados_iniciar("deploy/app");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: enquete", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
  txt_definir_fonte_interface(TXT_FAMILIA_MONTSERRAT);   // a da TV (o Mac usaria Inter)
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);
  fundo(0);
  quadros(60);

  // 1. CONVITE: a pilula avisa e cresce sozinha ate o modal.
  captura("00-relogio-repouso", 140);
  novaSessao("logo-1", 3 * 86400);
  { int i; for (i = 0; i < 400 && !abrindo; i++) quadros(1); }
  assert(abrindo);
  quadros(8);
  captura("01a-convite-aviso", 140);
  quadros(90);
  assert(ilha_modal_aberto());
  captura("01b-convite-modal", 420);
  // 2. OPCOES: Responder.
  tecla(SDLK_RETURN);
  quadros(110);
  assert(ilha_modal_aberto());
  captura("02-opcoes", 640);
  tecla(SDLK_RIGHT); quadros(25);
  captura("02b-opcoes-foco-2", 640);
  // 3. AGORA NAO (Voltar): bolinha no relogio.
  tecla(SDLK_ESCAPE);
  quadros(120);
  assert(ponto == 1 && !ilha_modal_aberto());
  captura("03-declinado-bolinha", 140);
  // 4. A AZUL no relogio reabre as opcoes; vota na 2.
  tecla(SDLK_s);
  quadros(110);
  assert(ilha_modal_aberto());
  captura("04a-reaberta-pela-azul", 640);
  tecla(SDLK_RIGHT); quadros(10);
  tecla(SDLK_RETURN);
  quadros(120);
  assert(S.voto == 2 && cur.voto == 2 && ponto == 0);
  captura("04b-resultado", 640);
  tecla(SDLK_RETURN);
  quadros(90);
  captura("04c-depois-do-ok-sem-bolinha", 140);
  limpar();
  // 5. NAO RECEBER MAIS: aviso de acao com Desfazer.
  novaSessao("logo-2", 86400);
  { int i; for (i = 0; i < 400 && !abrindo; i++) quadros(1); }
  quadros(100);
  assert(ilha_modal_aberto());
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); quadros(20);
  captura("05a-convite-foco-optout", 420);
  tecla(SDLK_RETURN);
  quadros(110);
  assert(S.optout == 1 && !ajustes_enquetes() && ilhaacao_tem_desfazer());
  captura("05b-optout-aviso", 140);
  tecla(SDLK_s);
  quadros(90);
  captura("05c-optout-modal-desfazer", 560);
  tecla(SDLK_RETURN);   // Desfazer
  quadros(120);
  assert(S.optout == 0 && ajustes_enquetes());
  captura("05d-desfeito", 140);
  limpar();
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas da enquete gravadas.");
  return 0;
}
