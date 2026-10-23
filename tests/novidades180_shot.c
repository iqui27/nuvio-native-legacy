// Captura do cartao de NOVIDADES DA 1.8.0 sem janela visivel (FBO, como
// tests/novidades170_shot.c). Antes, as regras do cartao por tecla; depois os
// quadros do mockup aprovado (novidades-mockup.html) num material:
//   NUVIO_SHOT_VIDRO=1 (padrao) vidro, =0 solido.
// Saida: <saida>-<id>.png, com o id do quadro do mockup. Atras do cartao, a
// home do mockup (arte 13, veu, logo e a fileira "Em alta").
#include "novidades180.h"
#include "nvtmp.h"
#include "novidadesfila.h"
#include "ajustes.h"
#include "badges.h"
#include "dados.h"
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
static int vidro = 1;

static void ajustesDeTeste(int idioma, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\nvidroLocal %d\n", idioma, tema, reduzidas, vidro ? 0 : 1);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades180_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

static void regras(void) {
  // De fabrica: foco na principal; OK = Vidro ou solido, grava a marca.
  dados_apagar(NF_ARQ_180);
  novidades180_abrir(0);
  assert(novidades180_foco() == 2 && !novidades180_foco_na_previa());
  tecla(SDLK_RETURN);
  assert(!novidades180_aberto());
  assert(novidades180_pedido() == N180_PEDIU_VIDRO);
  assert(novidades180_pedido() == N180_PEDIU_NADA);
  assert(existe(NF_ARQ_180));
  // Esquerda: Abrir o guia.
  novidades180_abrir(0);
  tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  assert(novidades180_pedido() == N180_PEDIU_GUIA);
  // Duas esquerdas (e uma a mais na borda): Agora nao.
  novidades180_abrir(0);
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  assert(novidades180_pedido() == N180_PEDIU_NADA && !novidades180_aberto());
  // Instalacao nova: foco no guia.
  novidades180_abrir(1);
  assert(novidades180_foco() == 1 && novidades180_boas_vindas());
  // Cima: a previa; direita troca a cena sem fechar; baixo volta.
  tecla(SDLK_UP);
  assert(novidades180_foco_na_previa());
  { int c = novidades180_cena();
    tecla(SDLK_RIGHT);
    assert(novidades180_cena() == (c + 1) % novidades180_cenas());
    tecla(SDLK_RETURN);
    assert(novidades180_aberto());
    tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
    assert(novidades180_cena() == (c + novidades180_cenas() - 1) % novidades180_cenas()); }
  tecla(SDLK_DOWN);
  assert(!novidades180_foco_na_previa());
  // Voltar = Agora nao.
  tecla(SDLK_ESCAPE);
  assert(!novidades180_aberto() && novidades180_pedido() == N180_PEDIU_NADA);
  // De volta do guia: reabre com o foco no guia.
  novidades180_reabrir();
  assert(novidades180_aberto() && novidades180_foco() == 1);
  tecla(SDLK_ESCAPE);
  puts("PASS: regras do cartao da 1.8.0");
}

// A home do mockup atras do cartao.
static void home(void) {
  static const char *const PO[8] = { "04", "09", "12", "15", "16", "19", "21", "02" };
  char c[300];
  GLuint t;
  int i;
  t = tex_obter_larg("deploy/app/art/13.jpg", 1920);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto("deploy/app/art/13.jpg");
    gfx_card_forcar_cover_atual = 1;
    gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, t, GFX_CARD, 0, 0, 0, 0, 0, 0, 0, 1);
    gfx_card_forcar_cover_atual = 0;
    gfx_tex_aspect_atual = 0;
  }
  gfx_veu_css((GfxRect){ 0, 0, 1344, 1080 }, 2, 1.0f, 1.0f, 0.72f);
  gfx_veu_css((GfxRect){ 0, 594, 1920, 486 }, 0, 1.0f, 1.0f, 0.70f);
  t = tex_obter_larg("deploy/app/art/logo/13.png", 560);
  if (t) {
    float asp = tex_aspecto("deploy/app/art/logo/13.png"), w = 560, h;
    if (asp <= 0) asp = 3;
    h = w / asp; if (h > 200) { h = 200; w = h * asp; }
    gfx_rect((GfxRect){ 120, 250, w, h }, t, GFX_TEXTO, 0, 0, 0, 0, 1, 1, 1, 1);
  }
  for (i = 0; i < 8; i++) {
    snprintf(c, sizeof c, "deploy/app/art/poster/%s.jpg", PO[i]);
    t = tex_obter_larg(c, 200);
    if (!t) continue;
    gfx_tex_aspect_atual = tex_aspecto(c);
    gfx_card_forcar_cover_atual = 1;
    gfx_rect((GfxRect){ 120 + i * 222.0f, 738, 200, 300 }, t, GFX_CARD, 0, 0, 0, 14.0f / 300, 0, 0, 0, 1);
    gfx_card_forcar_cover_atual = 0;
  }
  gfx_tex_aspect_atual = 0;
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades180_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  home();
  novidades180_desenhar(SDL_GetTicks());
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
  for (y = 0; y < 1080; y++) {
    int x;
    unsigned char *d = (unsigned char *)s->pixels + y * s->pitch;
    memcpy(d, pix + (1079 - y) * 1920 * 4, 1920 * 4);
    for (x = 0; x < 1920; x++) d[x * 4 + 3] = 255;
  }
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

// Abre na variante, aplica as teclas, vai para a cena/instante e grava.
static void captura(const char *saida, const char *id, int bv, const char *teclas, int c, float t) {
  char nome[800];
  int i;
  novidades180_abrir(bv);
  for (; teclas && *teclas; teclas++)
    tecla(*teclas == 'L' ? SDLK_LEFT : *teclas == 'R' ? SDLK_RIGHT : *teclas == 'U' ? SDLK_UP : SDLK_DOWN);
  novidades180_ir(c, t);
  for (i = 0; i < 160; i++) { quadro(0.0f); SDL_Delay(3); }
  snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
  gravar(nome);
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 30; i++) quadro(1.0f / 60.0f);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-novidades180");
  const char *dir = getenv("NUVIO_DADOS");
  const char *so = getenv("N180_SO");
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  if (getenv("NUVIO_SHOT_VIDRO")) vidro = atoi(getenv("NUVIO_SHOT_VIDRO"));
  dados_iniciar(dir);
  novidades180_dir("deploy/app/art");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades180-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  badges_carregar("deploy/app/art");
  ajustesDeTeste(IDIOMA_PT, 0, 2);
  regras();

  { int lim = 0, i, largas = 0;
    const char *nome;
    for (i = 0; i < novidades180_itens(); i++)
      if (novidades180_item_largura(i, &lim, &nome) > lim) { printf("larga: %s\n", nome); largas++; }
    printf("frases que cortam em pt: %d\n", largas); }

  { static const struct { const char *id; int bv; const char *teclas; int c; float t; } Q[] = {
      { "novidades", 0, "", 0, 2.85f },
      { "novidades-peek-2", 0, "", 1, 1.84f },
      { "novidades-peek-3", 0, "", 2, 3.59f },
      { "novidades-agora-foco", 0, "LL", 0, 2.85f },
      { "novidades-guia-foco", 0, "L", 0, 2.85f },
      { "novidades-previa-foco", 0, "U", 1, 1.0f },
      { "novidades-instalacao-nova", 1, "", 0, 2.85f },
    };
    int i;
    for (i = 0; i < (int)(sizeof Q / sizeof *Q); i++) {
      if (so && !strstr(Q[i].id, so)) continue;
      captura(saida, Q[i].id, Q[i].bv, Q[i].teclas, Q[i].c, Q[i].t);
    } }
  if (getenv("N180_EN")) {
    ajustesDeTeste(IDIOMA_EN, 0, 2);
    captura(saida, "en", 0, "", 0, 2.0f);
    ajustesDeTeste(IDIOMA_RU, 0, 2);
    captura(saida, "ru", 1, "", 2, 2.0f);
  }
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 1.8.0 gravadas.");
  return 0;
}
