// Regras e capturas do cartao da 1.7.4 (ver tests/novidades174_shot.sh).
#include "novidades174.h"
#include "nvtmp.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "idiomacod.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void ajustesDeTeste(int idioma, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n", idioma, tema, reduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades174_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

#define MARCA "novidades-174-ui.txt"

// OK fecha (Entendi), Esquerda + OK fecha (Agora nao), Voltar fecha, cima poe
// o foco na previa e OK ali troca a cena sem fechar; todos gravam a marca, e
// com a marca gravada a primeira vez nao abre de novo.
static void regras(void) {
  dados_apagar(MARCA);
  novidades174_abrir();
  tecla(SDLK_RETURN);
  assert(!novidades174_aberto());
  assert(existe(MARCA));
  dados_apagar(MARCA);
  novidades174_abrir();
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!novidades174_aberto());
  assert(existe(MARCA));
  dados_apagar(MARCA);
  novidades174_abrir();
  tecla(SDLK_UP);
  tecla(SDLK_RETURN);
  assert(novidades174_aberto());
  tecla(SDLK_RIGHT);
  assert(novidades174_aberto());
  tecla(SDLK_DOWN);
  tecla(SDLK_ESCAPE);
  assert(!novidades174_aberto());
  assert(existe(MARCA));
  novidades174_primeira_vez();
  assert(!novidades174_aberto());
  puts("PASS: regras do cartao da 1.7.4");
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades174_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  novidades174_desenhar(SDL_GetTicks());
  glFinish();
}

static void gravar(const char *nome) {
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
  printf("captura: %s\n", nome);
}

static void captura(const char *saida, const char *sufixo, int c, float t, int previa) {
  char nome[800];
  int i;
  novidades174_abrir();
  if (previa) tecla(SDLK_UP);
  novidades174_ir(c, t);
  for (i = 0; i < 160; i++) { quadro(0.0f); SDL_Delay(4); }
  for (i = 0; i < 6; i++) { quadro(0.0f); SDL_Delay(2); }
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, sufixo);
  gravar(nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-novidades174");
  const char *so = getenv("N174_SO");
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  char suf[64];
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  novidades174_dir("deploy/app/art");

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades174-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  tex_iniciar(96);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");

  regras();

  // Cada descricao cabe numa linha, nos 30 idiomas? So avisa (a lista ainda
  // se arruma com duas); N174_ESTRITO=1 reprova.
  { int lg, i, largas = 0;
    for (lg = 0; lg < IDIOMA_N; lg++) {
      ajustesDeTeste(lg, 0, 2);
      for (i = 0; i < novidades174_itens(); i++) {
        int lim = 0;
        const char *nome = NULL;
        int wd = novidades174_item_largura(i, &lim, &nome);
        if (wd > lim) { printf("larga: idioma %d, %s: %d > %d\n", lg, nome, wd, lim); largas++; }
      }
    }
    printf("descricoes em duas linhas: %d\n", largas);
    if (getenv("N174_ESTRITO")) assert(largas == 0); }

  assert(novidades174_cenas() == 3);
  { static const struct { const char *suf; int c; float t; } MOM[] = {
      { "0-abertura", 0, 0.8f }, { "0-dissolvendo", 0, 2.3f }, { "0-login", 0, 5.0f },
      { "1-digitando", 1, 1.8f }, { "1-senha", 1, 4.0f }, { "1-entrar", 1, 5.6f },
      { "2-chegando", 2, 1.6f }, { "2-escolhida", 2, 3.6f }, { "2-todas", 2, 6.0f },
    };
    static const int LG[3] = { IDIOMA_PT, IDIOMA_EN, IDIOMA_JA };
    static const char *const NOME[3] = { "pt", "en", "ja" };
    int l, i;
    for (l = 0; l < 3; l++) {
      if (so && strcmp(so, NOME[l])) continue;
      ajustesDeTeste(LG[l], 0, 2);
      for (i = 0; i < (int)(sizeof MOM / sizeof *MOM); i++) {
        if (l == 2 && i % 3 != 2) continue;   // ja: um quadro por cena basta
        snprintf(suf, sizeof suf, "%s-%s", NOME[l], MOM[i].suf);
        captura(saida, suf, MOM[i].c, MOM[i].t, 0);
      }
    }
    if (!so || !strcmp(so, "pt")) captura(saida, "pt-previa", 1, 3.0f, 1); }

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 1.7.4 gravadas.");
  return 0;
}
