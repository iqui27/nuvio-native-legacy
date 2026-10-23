// The Sources sheet on the title DETAIL page (no player) is born from the
// clock island: the island stretches from the pill into the sheet (ilha_corpo),
// instead of a panel sliding in from the right edge. Asserts the opening
// starts at the pill rect and settles as the sheet; writes 4 opening frames.
//   bash tests/fontes_relogio_shot.sh <dir>
#include "catalogo.h"
#include "nvtmp.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "streams.h"
#include "ilha.h"
#include "badges.h"
#include "detail.h"
#include "player.h"
#include "plrilha.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
enum { LW = 1920, LH = 1080 };
static Uint32 relogio = 100000;
static SDL_Surface *strip;
static int noPlayer, semRelogio;

static void salvar(const char *pre, int n) {
  static unsigned char q[LW * LH * 3];
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, q);
  for (int y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, q + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s-%d.png", pre, n);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_Rect dst = { (n - 1) * LW / 2, 0, LW / 2, LH / 2 };
  assert(SDL_BlitScaled(s, NULL, strip, &dst) == 0);
  SDL_FreeSurface(s);
  printf("capture: %s\n", nome);
}

static void quadro(void) {
  relogio += 16;
  SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
  if (detail_aberto()) detail_atualizar(1.f / 60, relogio);
  stream_folha_atualizar(1.f / 60, relogio);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  glClearColor(0.025f, 0.025f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_novo_quadro();
  gfx_cor((GfxRect){ 0, 0, LW, LH }, 0, .26f, .17f, .12f, 1);
  gfx_cor((GfxRect){ 0, 0, LW, 360 }, 0, .55f, .36f, .22f, 1);
  if (detail_aberto()) detail_desenhar(relogio);
  stream_folha_desenhar(relogio);
  if (noPlayer) {
    plrilha_relogio(1, -1);
    plrilha_desenhar(relogio);
  } else {
    // app.c hides the resting clock while Sources owns the overlay.
    ilha_relogio_visivel(!semRelogio && !stream_folha_aberta());
    ilha_posicionar(1);
    ilha_desenhar(relogio);
  }
}

static const char *JSON =
  "{\"streams\":["
  "{\"url\":\"https://x.invalid/1.mkv\",\"name\":\"[RD+] AIOStreams 4K DV\",\"description\":\"12.3 GB\"},"
  "{\"url\":\"https://x.invalid/2.mkv\",\"name\":\"Torrentio\\n1080p\",\"title\":\"Silo.S02E05.1080p.WEB-DL.mkv\"},"
  "{\"url\":\"https://x.invalid/3.mkv\",\"name\":\"Comet 720p\",\"description\":\"720p WEB\"}"
  "]}";

int main(int argc, char **argv) {
  Stream *v = NULL;
  const char *pre = argc > 1 ? argv[1] : nvt("/nv-fontes-relogio");
  float px = 0, py = 0, pw = 0, ph = 0, x, y, w, h;
  float firstX, firstY, firstW, firstH, firstT;
  const char *mode = argc > 2 ? argv[2] : "detail";
  noPlayer = !strcmp(mode, "player");
  semRelogio = !strcmp(mode, "no-clock");
  strip = SDL_CreateRGBSurfaceWithFormat(0, LW * 2, LH / 2, 32, SDL_PIXELFORMAT_RGBA32);
  assert(strip);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("Nuvio: fontes do relogio", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\n");
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (semRelogio) assert(ajustes_shot_valor("relogioTelaLocal", 1));
  { CatItem c = {0};
    snprintf(c.titulo, sizeof c.titulo, "Silo");
    snprintf(c.tipo, sizeof c.tipo, "series");
    snprintf(c.meta, sizeof c.meta, "2023 · Drama · Mystery");
    snprintf(c.sinopse, sizeof c.sinopse, "In a ruined future, a community lives in a giant silo underground.");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/poster/00.jpg");
    cat_definir(&c, 1);
    if (noPlayer) player_abrir(0, NULL);
    else if (!strcmp(mode, "detail")) {
      HomeItem hi = { .indice = 0, .rect = {0, 0, LW, LH}, .titulo = c.titulo,
                      .genero = c.genero, .meta = c.meta };
      detail_abrir(&hi);
      assert(detail_aberto());
    }
  }
  assert(stream_extrair(JSON, "AIOStreams", &v) == 3);
  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, 3);
  stream_folha_nome("Silo");
  stream_folha_contexto("T2:E5 · Silo");

  for (int i = 0; i < 40; i++) quadro();           // the pill settles
  if (noPlayer) {
    GfxRect r; assert(plrilha_rect(&r));
    px = r.x; py = r.y; pw = r.w; ph = r.h;
  } else if (!semRelogio) assert(ilha_rect(&px, &py, &pw, &ph));
  if (detail_aberto()) {
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    detail_evento(&e);
    SDL_Delay(NV_HOLD_MS + 1);
    e.type = SDL_KEYUP; detail_evento(&e);
    assert(detail_pediu_fontes());
  }
  stream_folha_abrir();
  quadro();                                        // first opening frame
  if (noPlayer) { GfxRect r; assert(plrilha_rect(&r)); x=r.x; y=r.y; w=r.w; h=r.h; }
  else { x=y=w=h=0; ilha_rect(&x, &y, &w, &h); }
  firstX=x; firstY=y; firstW=w; firstH=h; firstT=ilha_corpo_t();
  printf("%s: pill %.0f,%.0f %.0fx%.0f; frame1 %.0f,%.0f %.0fx%.0f; morph %.4f\n", mode, px, py, pw, ph, x, y, w, h, firstT);
  salvar(pre, 1);
  { int cap[3] = { 6, 8, 12 };
    for (int c = 0; c < 3; c++) { for (int i = 0; i < cap[c]; i++) quadro(); salvar(pre, c + 2); } }
  { char path[700]; snprintf(path, sizeof path, "%s-strip.png", pre);
    assert(IMG_SavePNG(strip, path) == 0); SDL_FreeSurface(strip); }
  if (!semRelogio) {
    assert(firstW > pw && firstH > ph);
    assert(fabsf(firstY-py) < 1 && fabsf(firstX+firstW-px-pw) < 1);
    assert(firstW < pw + .5f*(800-pw));
  }
  for (int i = 0; i < 60; i++) quadro();
  if (noPlayer) { GfxRect r; assert(plrilha_rect(&r)); x=r.x; y=r.y; w=r.w; h=r.h; }
  else if (!semRelogio) assert(ilha_rect(&x, &y, &w, &h));
  if (!semRelogio) {
    // An unchanged pill plus a separate sliding sheet must fail, even if
    // both happen to share the same corner. Check actual shape interpolation.
    assert(w > 780 && h > 700);
    assert(fabsf(x+w-px-pw) < 1);
    if (!noPlayer) {
      assert(firstT > 0 && firstT < .1f);
      assert(fabsf(firstW - (pw+(w-pw)*firstT)) < 1);
      assert(fabsf(firstH - (ph+(h-ph)*firstT)) < 1);
    }
  } else assert(ilha_corpo_t() == 0);
  { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    stream_folha_evento(&e); assert(!stream_folha_aberta()); }
  for (int i = 0; i < 100; i++) quadro();
  if (!noPlayer && !semRelogio) {
    assert(ilha_rect(&x, &y, &w, &h));
    assert(ilha_corpo_t() == 0 && fabsf(w-pw) < 1 && fabsf(h-ph) < 1);
  }
  printf("PASS: %s opening origin and closing\n", mode);
  return 0;
}
