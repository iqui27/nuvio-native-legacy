#include <SDL2/SDL_image.h>
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e);
}
static void shot(const char *name) {
  const char *stage = getenv("PALETTE_STAGE");
  if (!stage) return;
  for (int i = 0; i < 90; i++) {
    SDL_PumpEvents(); ponteiro_quadro(1000); txt_novo_quadro(); tex_novo_quadro();
    ajustes_atualizar(1.0f / 60.0f, 1000);
    glClearColor(0.025f, 0.025f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(1000);
  }
  unsigned char *pixels = malloc(1920 * 1080 * 4), *flipped = malloc(1920 * 1080 * 4);
  assert(pixels && flipped);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++) memcpy(flipped + y * 1920 * 4, pixels + (1079-y) * 1920 * 4, 1920 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(flipped, 1920, 1080, 32, 1920 * 4, SDL_PIXELFORMAT_RGBA32);
  char path[1024]; snprintf(path, sizeof path, "/Volumes/ExternalSSD/tmp/dinfundo/fix/%s-%s.png", stage, name);
  assert(s && IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels); free(flipped);
}
int main(void) {
  char dir[1024]; snprintf(dir, sizeof dir, "%s/fundo-paleta-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Palette regression", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w && SDL_GL_CreateContext(w)); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); ajustes_recursos("deploy/app/art");
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); ajustes_iniciar();
  valor[AJ_HOME_LAYOUT] = HOME_LAYOUT_DINAMICA; valor[AJ_FUNDO_DIN] = 1;
  valor[AJ_TEMA] = 7; valor[AJ_FUNDO_DIN_COR] = 12;
  for (int layout = 0; layout < 2; layout++) {
    char name[80]; valor[AJ_LAYOUT_AJUSTES] = layout;
    focarOpcao(AJ_TEMA); key(SDLK_RETURN);
    snprintf(name, sizeof name, "accent-%d", layout); shot(name); key(SDLK_ESCAPE);
    focarOpcao(AJ_FUNDO_DIN_COR);
    snprintf(name, sizeof name, "closed-%d", layout); shot(name);
    key(SDLK_RETURN);
    snprintf(name, sizeof name, "background-%d", layout); shot(name);
    if (getenv("PALETTE_BEFORE")) { key(SDLK_ESCAPE); continue; }
    assert(ajEditandoLinha(AJ_FUNDO_DIN_COR));
    // Palette pixels must be present in the expanded row, not just empty space.
    unsigned char rgb[3];
    GfxRect row = aj2ListaR();
    float kcol = row.w - 36 - 30 - 9 * 72 - 8;
    if (kcol > 176) kcol = 176;
    if (kcol < 150) kcol = 150;
    gfx_opacidade_grupo = 1; aj2TextoLista = 1;
    ajDesenharLinha(focoItem, AJ_TOPO, 1, 1);
    glReadPixels((int)(row.x + 18 + 30 + kcol + 28),
                 1079 - (int)(AJ_TOPO + AJ2_LN_LINHA1 + 20 + 32), 1, 1,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb);
    unsigned fill = TEMA_ACENTO[16].fill; // first background swatch: Champagne
    assert(abs(rgb[0] - (int)((fill >> 16) & 255)) <= 1);
    assert(abs(rgb[1] - (int)((fill >> 8) & 255)) <= 1);
    assert(abs(rgb[2] - (int)(fill & 255)) <= 1);
    assert(ajLinhaH(focoItem) > AJ2_LN_LINHA1 + 2 * AJ_REGUA_FILA);
    key(SDLK_UP); assert(uxPendente == 14); // Ocean -> Lavender, same palette column
    key(SDLK_DOWN); assert(uxPendente == 12); // Lavender -> Ocean (white omitted)
    key(SDLK_RIGHT); assert(uxPendente == 13); // Indigo
    key(SDLK_RIGHT); assert(uxPendente == 15); // Violet
    key(SDLK_RIGHT); assert(uxPendente == 15); // right edge
    key(SDLK_DOWN); assert(uxPendente == 15); // no dynamic/textured backgrounds
    key(SDLK_ESCAPE); assert(valor[AJ_FUNDO_DIN_COR] == 12 && valor[AJ_TEMA] == 7);
    focarOpcao(AJ_FUNDO_DIN_COR); key(SDLK_RETURN);
    ajPontInline = 1; ajToqueInlineValor(0, AJ_FUNDO_DIN_COR);
    assert(uxPendente == 0); key(SDLK_RETURN);
    assert(!uxEditor && valor[AJ_FUNDO_DIN_COR] == 0 && valor[AJ_TEMA] == 7);
    ajustes_iniciar(); assert(valor[AJ_FUNDO_DIN_COR] == 0 && valor[AJ_TEMA] == 7);
    valor[AJ_FUNDO_DIN_COR] = 12;
  }
  if (!getenv("PALETTE_BEFORE")) {
    focarOpcao(AJ_TEMA); key(SDLK_RETURN); uxPendente = 0;
    key(SDLK_DOWN); assert(uxPendente == 1);
    key(SDLK_DOWN); assert(uxPendente == AJ_TEMA_DINAMICA);
    key(SDLK_DOWN); assert(uxPendente == AJ_TEMA_TEXTURA);
    key(SDLK_ESCAPE);
  }
  ajustes_encerrar();
  char path[1200]; snprintf(path, sizeof path, "%s/ajustes.txt", dir); unlink(path);
  snprintf(path, sizeof path, "%s/ajustes-locais.txt", dir); unlink(path); rmdir(dir);
  SDL_Quit(); puts("fundo_paleta: OK"); return 0;
}
