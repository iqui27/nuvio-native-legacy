// A LINHA EXPANDIDA DO PAINEL DE SALVOS (acordeao, dono 06/10): recolhida,
// meio da animacao, aberta e com o foco na 2a acao. PNG via BMP.
//   bash tests/spainel_acordeao_shot.sh /tmp/nv-acordeao
#define NV_REC_URL "http://127.0.0.1:8799"
#include "../src/recomenda.c"
#include "salvospainel.h"
#include "ctxmenu.h"
#include "salvosorg.h"
#include "ctxinfo.h"
#include "extras.h"
#include "salvos.h"
#include "avisos.h"
#include "catalogo.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "shot_arte.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint fbo, fboTex;
static unsigned char *ult;   // ultimo quadro lido (RGBA, de cima para baixo)

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  spainel_evento(&e);
}

static void captura(const char *nome, SDL_Window *win, int n) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    spainel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    ctx_atualizar(1.0f / 60.0f, SDL_GetTicks());
    recomenda_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.20f, 0.24f, 0.32f, 1.0f);   // fundo claro: o vidro deixa ver o corte
    glClear(GL_COLOR_BUFFER_BIT);
    // Com NUVIO_SHOT_ARTE=<jpg> a arte vai atras, para julgar o vidro; sem ela
    // o fundo liso, que e o que a MEDIDA do corte (primeiroDesenhado) precisa.
    if (getenv("NUVIO_SHOT_ARTE")) shot_arte_desenhar(.30f);
    spainel_desenhar(SDL_GetTicks());
    ctx_desenhar(SDL_GetTicks());
    if (i == n - 1) {
      unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glFinish();
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      if (!ult) ult = (unsigned char *)malloc(1920 * 1080 * 4);
      memcpy(ult, s->pixels, 1920 * 1080 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

static void semear(const char *dir) {
  char caminho[700];
  long long agora = (long long)time(NULL);
  int i;
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/recomendacoes.txt", dir);
  f = fopen(caminho, "wb");
  assert(f);
  fprintf(f, "# nuvio recomendacoes v2\n");
  fprintf(f, "7\t%lld\t0\t2\tseries\t2023\tnuvio:pedro\tPedro\ttt14688458\t"
             "deploy/app/art/00.jpg\tTerminei, sua vez\t81\t\tSilo\n", agora - 864000);
  fprintf(f, "6\t%lld\t1\t-1\tmovie\t2014\tnuvio:pedro\tPedro\ttt2582802\t"
             "deploy/app/art/01.jpg\t\t85\t\tWhiplash\n", agora - 900000);
  fclose(f);
  snprintf(caminho, sizeof caminho, "%s/salvos.txt", dir);
  f = fopen(caminho, "wb");
  assert(f);
  fprintf(f, "# nuvio salvos v1\n");
  for (i = 0; i < 9; i++)
    fprintf(f, "tt00000%02d\tmovie\t%lld\t80\t2000\tdeploy/app/art/0%d.jpg\tTitulo salvo %d\n",
            10 + i, agora - 3600 * (i + 1), i % 8, i + 1);
  fclose(f);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma 0\nselected_theme 1\n");
  fclose(f);
}

// A partir da linha `y0` (logo abaixo das abas) desce a coluna `x` e devolve o
// primeiro pixel que difere do fundo do painel por mais que `lim` — o topo do
// que foi desenhado ali. Medido longe de texto e de cartaz.
static int primeiroDesenhado(int x, int y0, int y1) {
  int y, r0 = ult[(y0 * 1920 + x) * 4], g0 = ult[(y0 * 1920 + x) * 4 + 1], b0 = ult[(y0 * 1920 + x) * 4 + 2];
  for (y = y0 + 1; y < y1; y++) {
    const unsigned char *p = ult + (y * 1920 + x) * 4;
    if (abs(p[0] - r0) + abs(p[1] - g0) + abs(p[2] - b0) > 18) return y;
  }
  return -1;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nv-foco";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  int vidro;
  if (!dir || !dir[0]) { printf("NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  semear(dir);
  ajustes_dir(dados_dir());

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("foco", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  SDL_GL_SetSwapInterval(0);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  salvos_iniciar();
  recomenda_iniciar();
  avisos_modo_seguro("t1", "Canal ao vivo travou", "Voltamos para o canal anterior.");
  avisos_modo_seguro("t2", "Fonte trocada", "A fonte caiu e a proxima entrou.");
  aparecer = REC_APARECER_SIM;
  nContatos = 1;
  memset(contatos, 0, sizeof contatos);
  snprintf(contatos[0].id, sizeof contatos[0].id, "%s", "nuvio:pedro");
  snprintf(contatos[0].nome, sizeof contatos[0].nome, "%s", "Pedro");

  for (vidro = 1; vidro >= 0; vidro--) {
    int est;
    const char *v = vidro ? "vidro" : "solido";
    SDL_Event e;
    ajustes_definir_vidro(vidro);
    spainel_fechar(); spainel_abrir();
    tecla(SDLK_DOWN);   // foco na 1a linha (a 0 e a barra)
    snprintf(nome, sizeof nome, "%s-%s-1-recolhida.bmp", saida, v);
    captura(nome, w, 60);
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; spainel_evento(&e);
    SDL_Delay(NV_HOLD_MS + 150);
    spainel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    printf("menu aberto: %d inline_t=%.2f\n", ctx_aberto(), ctx_inline_t());
    e.type = SDL_KEYUP; ctx_evento(&e);
    snprintf(nome, sizeof nome, "%s-%s-2-meio.bmp", saida, v);
    captura(nome, w, 6);
    snprintf(nome, sizeof nome, "%s-%s-3-aberta.bmp", saida, v);
    captura(nome, w, 90);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RIGHT; ctx_evento(&e);
    snprintf(nome, sizeof nome, "%s-%s-4-segunda-acao.bmp", saida, v);
    captura(nome, w, 40);
    e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e);
    snprintf(nome, sizeof nome, "%s-%s-5-fechando.bmp", saida, v);
    captura(nome, w, 6);
    printf("depois do Voltar: aberto=%d\n", ctx_aberto());
    if (vidro) for (est = 1; est <= 2; est++) {
      SDL_Event k;
      sorg_definir_estilo(est);
      spainel_fechar(); spainel_abrir();
      tecla(SDLK_DOWN);
      memset(&k, 0, sizeof k);
      k.type = SDL_KEYDOWN; k.key.keysym.sym = SDLK_RETURN; spainel_evento(&k);
      SDL_Delay(NV_HOLD_MS + 150);
      spainel_atualizar(1.0f / 60.0f, SDL_GetTicks());
      k.type = SDL_KEYUP; ctx_evento(&k);
      snprintf(nome, sizeof nome, "%s-estilo%d-aberta.bmp", saida, est);
      captura(nome, w, 90);
      k.type = SDL_KEYDOWN; k.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&k);
    }
  }
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
