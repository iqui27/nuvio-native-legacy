// "Foco na cor de destaque": the focus plate takes the accent colour (tinted glass)
// instead of the gray bar. Asserts: OFF draws nothing (callers keep today's gray,
// pixel-identical), ON draws in the accent with readable light text, light accents
// included. With FOCO_STAGE=<dir> also saves PNGs of the Saved panel, a settings
// row and the context menu.
#include <SDL2/SDL_image.h>
#include "../src/ajustes.c"
#include "nvtmp.h"
#include "salvos.h"
#include "salvospainel.h"
#include "ctxmenu.h"
#include <assert.h>
#include <math.h>
#include <unistd.h>

#define W 1920
#define H 1080
static const int ACENTOS[3] = { 0, 7, 1 };   // default (white), gold (yellow), crimson (red)
static const char *NOMES[3] = { "default", "yellow", "red" };

static void px(int x, int y, unsigned char *rgb) {
  glReadPixels(x, H - 1 - y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
}
static float lin(int c) { float v = c / 255.0f; return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f); }
static float lum(const unsigned char *c) { return 0.2126f * lin(c[0]) + 0.7152f * lin(c[1]) + 0.0722f * lin(c[2]); }
static void fundo(void) { glClearColor(0.08f, 0.085f, 0.10f, 1); glClear(GL_COLOR_BUFFER_BIT); }
static void liga(int on) { valor[AJ_FOCO_ACENTO] = on ? 0 : 1; }

static void salvar(const char *dir, const char *nome) {
  static unsigned char p[W * H * 4], f[W * H * 4];
  SDL_Surface *s;
  char path[1024];
  glFinish();
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, p);
  for (int y = 0; y < H; y++) memcpy(f + y * W * 4, p + (H - 1 - y) * W * 4, W * 4);
  s = SDL_CreateRGBSurfaceWithFormatFrom(f, W, H, 32, W * 4, SDL_PIXELFORMAT_RGBA32);
  snprintf(path, sizeof path, "%s/%s.png", dir, nome);
  assert(s && IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
}
static void quadros(int n, void (*desenha)(void)) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents(); ponteiro_quadro(1000); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    fundo();
    desenha();
  }
}
static void dSalvos(void) { spainel_atualizar(1.0f / 60.0f, 1000); spainel_desenhar(1000); }
static void dAjustes(void) { ajustes_atualizar(1.0f / 60.0f, 1000); ajustes_desenhar(1000); }
static void dCtx(void) { ctx_atualizar(1.0f / 60.0f, 1000); ctx_desenhar(1000); }

int main(void) {
  char dir[1024];
  const char *stage = getenv("FOCO_STAGE");
  snprintf(dir, sizeof dir, "%s/foco-acento-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  { char c[1100]; FILE *f; long long agora = (long long)time(NULL);
    snprintf(c, sizeof c, "%s/salvos.txt", dir); f = fopen(c, "wb"); assert(f);
    fprintf(f, "# nuvio salvos v1\n");
    for (int i = 0; i < 6; i++)
      fprintf(f, "tt00000%02d\tmovie\t%lld\t80\t2000\tdeploy/app/art/0%d.jpg\tMatchbox the Movie %d\n", 10 + i, agora - 3600 * (i + 1), i, i + 1);
    fclose(f); }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("foco acento", 0, 0, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w && SDL_GL_CreateContext(w)); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, W, H); gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); ajustes_recursos("deploy/app/art");
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); ajustes_iniciar();
  salvos_iniciar();
  valor[AJ_VIDRO] = 0;

  GfxRect r = { 400, 300, 600, 90 };
  for (int a = 0; a < 3; a++) {
    unsigned char bg[3], off[3], on[3], rim[3], gray[3];
    valor[AJ_TEMA] = ACENTOS[a];
    // reference: today's gray plate drawn directly
    fundo(); gfx_cor(r, 0.5f, 1, 1, 1, .12f); px(700, 345, gray);
    // OFF: the helper draws nothing and returns 0 -> the caller's gray stays identical
    liga(0); fundo(); px(700, 345, bg);
    assert(gfx_foco_placa(r, 0.5f, 1.0f, 1.0f) == 0); px(700, 345, off);
    assert(memcmp(bg, off, 3) == 0);
    fundo(); if (!gfx_foco_placa(r, 0.5f, 1.0f, 1.0f)) gfx_cor(r, 0.5f, 1, 1, 1, .12f);
    px(700, 345, off); assert(memcmp(off, gray, 3) == 0);
    // ON: accent plate
    valor[AJ_VIDRO_CONTORNO] = 0;
    liga(1); fundo(); assert(gfx_foco_placa(r, 0.5f, 1.0f, 1.0f) == 1); px(700, 345, on); px(700, 300 + 1, rim);
    printf("%s: bg %d,%d,%d gray %d,%d,%d on %d,%d,%d rim %d,%d,%d\n", NOMES[a], bg[0], bg[1], bg[2], gray[0], gray[1], gray[2], on[0], on[1], on[2], rim[0], rim[1], rim[2]);
    assert(memcmp(on, gray, 3) != 0);
    // light text (0.96) on the plate keeps WCAG AA 4.5:1
    { float cr = (0.96f + 0.05f) / (lum(on) + 0.05f); printf("  text contrast %.2f\n", cr); assert(cr >= 4.5f); }
    // the plate carries the accent hue
    if (a == 2) assert(on[0] > on[1] + 20 && on[0] > on[2] + 15);
    if (a == 1) assert(on[0] > on[2] + 20 && on[1] > on[2] + 15);
    // the rim is stronger than the fill
    assert(lum(rim) > lum(on) || a == 2);
    // outline OFF: fill and tint stay, the accent ring is gone (rim == fill)
    { unsigned char in[3], rim2[3];
      valor[AJ_VIDRO_CONTORNO] = 1; fundo(); assert(gfx_foco_placa(r, 0.5f, 1.0f, 1.0f) == 1);
      px(400 + 1, 345, rim2); px(400 + 30, 345, in);
      assert(memcmp(rim2, in, 3) == 0);
      valor[AJ_VIDRO_CONTORNO] = 0; }
    // Accent focus remains available without glass.
    valor[AJ_VIDRO] = 1; assert(ajustes_foco_acento()); valor[AJ_VIDRO] = 0;
  }
  assert(!ajustes_foco_acento() || valor[AJ_FOCO_ACENTO] == 0);
  liga(0); assert(!ajustes_foco_acento());   // default stays OFF in the shipped table

  if (stage) {
    CatItem it; char nome[80];
    memset(&it, 0, sizeof it);
    snprintf(it.imdb, sizeof it.imdb, "tt0000010"); snprintf(it.titulo, sizeof it.titulo, "Matchbox the Movie");
    snprintf(it.tipo, sizeof it.tipo, "movie");
    for (int a = -1; a < 3; a++) {
      const char *tag = a < 0 ? "off" : NOMES[a];
      valor[AJ_TEMA] = a < 0 ? 0 : ACENTOS[a]; liga(a >= 0);
      spainel_fechar(); spainel_abrir(); quadros(120, dSalvos);
      snprintf(nome, sizeof nome, "salvos-%s", tag); salvar(stage, nome);
      spainel_fechar();
      focarOpcao(AJ_FOCO_ACENTO); quadros(90, dAjustes);
      snprintf(nome, sizeof nome, "ajustes-%s", tag); salvar(stage, nome);
      ctx_abrir_salvo(&it); quadros(90, dCtx);
      snprintf(nome, sizeof nome, "ctx-%s", tag); salvar(stage, nome);
    }
  }
  if (getenv("REMOVER_STAGE")) {
    CatItem it; char nome[80];
    memset(&it, 0, sizeof it);
    snprintf(it.imdb, sizeof it.imdb, "tt0000010"); snprintf(it.titulo, sizeof it.titulo, "Matchbox the Movie");
    snprintf(it.tipo, sizeof it.tipo, "movie");
    valor[AJ_TEMA] = 1; liga(1);
    for (int c = 0; c < 2; c++) {
      valor[AJ_VIDRO_CONTORNO] = c ? 0 : 1;
      spainel_fechar(); spainel_abrir(); quadros(60, dSalvos);
      ctx_inline_pedir(1); ctx_abrir_salvo(&it); quadros(120, dSalvos);
      snprintf(nome, sizeof nome, "salvosremover-%s-contorno-%s", getenv("REMOVER_TAG"), c ? "on" : "off");
      salvar(getenv("REMOVER_STAGE"), nome);
    }
  }
  printf("foco_acento: ok\n");
  return 0;
}
