// #599: mixed sources must explain hidden torrents, even with playable URLs.
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static char hint[4096];
static float hintY, hintH;
static float capture_block(TxtEstilo estilo, const char *s, int r, int g, int b,
                           float x, float y, float w, float leading, float alpha, int max);
#define txt_bloco capture_block
#include "../src/streams.c"
#undef txt_bloco

static float capture_block(TxtEstilo estilo, const char *s, int r, int g, int b,
                           float x, float y, float w, float leading, float alpha, int max) {
  float h = txt_bloco(estilo, s, r, g, b, x, y, w, leading, alpha, max);
  if (alpha > .5f && strstr(s, "hidden")) {
    snprintf(hint, sizeof hint, "%s", s);
    hintY = y; hintH = h;
  }
  return h;
}

static void draw(SDL_Window *win, const char *png) {
  for (int frame = 0; frame < 80; frame++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
    stream_folha_atualizar(1.f / 60, SDL_GetTicks());
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro(); stream_folha_desenhar(SDL_GetTicks());
    if (frame == 79 && png) {
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(pixels && surface);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)surface->pixels + y * surface->pitch, pixels + (1079-y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(surface, png) == 0);
      SDL_FreeSurface(surface); free(pixels);
    }
    SDL_GL_SwapWindow(win);
  }
}

int main(int argc, char **argv) {
  char path[1024];
  int failures = 0;
  assert(argc == 2);
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *settings = fopen(path, "w"); assert(settings);
  fputs("idioma 1\nselected_theme 2\nvidroLocal 0\n", settings); fclose(settings);
  ajustes_dir(getenv("NUVIO_DADOS"));
  ajustes_definir_p2p_ligado(0);
  assert(!debrid_ativo() && !p2p_ativo());
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("Sources: hidden torrents", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0); glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  Stream *v = calloc(22, sizeof *v); assert(v);
  for (int i = 0; i < 22; i++) {
    snprintf(v[i].provedor, sizeof v[i].provedor, "%s", i < 5 ? "Stremio" : "HdHub");
    snprintf(v[i].rotulo, sizeof v[i].rotulo, "Movie 1080p");
    v[i].altura = 1080; v[i].fileIdx = -1;
    if (i < 5) snprintf(v[i].url, sizeof v[i].url, "https://example.invalid/%d.mp4", i);
    else snprintf(v[i].infoHash, sizeof v[i].infoHash, "%040x", i);
  }
  atomic_store(&motorP2pParaDica, 1); // Android TV has the bundled engine.
  stream_definir_lista(v, 22);
  assert(n == 5);
  stream_folha_nome("Movie"); stream_folha_abrir();
  SDL_Event down = { .type = SDL_KEYDOWN };
  down.key.keysym.sym = SDLK_DOWN;
  for (int i = 0; i < n + 2; i++) stream_folha_evento(&down);
  draw(win, argv[1]);
  if (!strstr(hint, "17") || !strstr(hint, "HdHub")) {
    fprintf(stderr, "FAIL: 5 URLs + 17 HdHub torrents: missing hidden-source hint: '%s'\n", hint);
    return 1;
  }
  assert(hintY >= FOLHA_TOPO && hintY + hintH <= folhaBase);
#ifdef __EMSCRIPTEN__
  assert(!strstr(hint, "P2P") && strstr(hint, "debrid"));
#else
  assert(strstr(hint, "P2P") && strstr(hint, "debrid"));
#endif
  // Additive publication must retain the same explanation and per-addon counts.
  stream_definir_lista(v, 5);
  stream_lista_acrescentar(v + 5, 17, 1);
  hint[0] = 0; draw(win, NULL);
  assert(strstr(hint, "17") && strstr(hint, "HdHub"));
  snprintf(v[5].provedor, sizeof v[5].provedor, "Other addon");
  stream_lista_acrescentar(v + 5, 1, 2);
  hint[0] = 0; draw(win, NULL);
  assert(strstr(hint, "18") && strstr(hint, "HdHub: 17") && strstr(hint, "Other addon: 1"));
  // A connected debrid makes these torrent-only sources available.
  debrid_definir_chave("realdebrid", "fixture-key"); assert(debrid_ativo());
  hint[0] = 0; draw(win, NULL); assert(!hint[0]);
  stream_definir_lista(v, 22); assert(n == 22);
  hint[0] = 0; draw(win, NULL); assert(!hint[0]);
  // Lumio-like infoHash-only notWebReady sources: debrid resolves them, no P2P needed.
  for (int i = 5; i < 22; i++) v[i].notWebReady = 1;
  stream_definir_lista(v, 22); assert(n == 22);
  hint[0] = 0; draw(win, NULL); assert(!hint[0]);
  puts("PASS: active-debrid keeps infoHash-only notWebReady sources");
  debrid_esquecer();
  // Without debrid, notWebReady infoHash sources stay rejected by pedeP2P().
  for (int engine = 0; engine <= 1; engine++) {
    atomic_store(&motorP2pParaDica, engine);
    stream_definir_lista(v, 22); assert(n == 5);
    hint[0] = 0; draw(win, NULL);
#if defined(__EMSCRIPTEN__)
    int available = 0;
#else
    int available = engine;
#endif
    if ((available && (!strstr(hint, "17") || !strstr(hint, "P2P"))) ||
        (!available && hint[0]) || strstr(hint, "debrid")) {
      fprintf(stderr, "FAIL: no-debrid P2P-only remedy, engine %d: '%s'\n", engine, hint);
      failures++;
    }
    // Only the source that does not require P2P can benefit from debrid.
    v[5].notWebReady = 0;
    stream_definir_lista(v, 22);
    hint[0] = 0; draw(win, NULL);
    if (!strstr(hint, available ? "17" : "1 torrent") || !strstr(hint, "debrid") ||
        (!!strstr(hint, "P2P") != available) ||
        (!available && strstr(hint, "HdHub"))) {
      fprintf(stderr, "FAIL: mixed requirements, engine %d: '%s'\n", engine, hint);
      failures++;
    }
    v[5].notWebReady = 1;
  }
  atomic_store(&motorP2pParaDica, 1);
  // Ten maximum-length addon names must not hide the remedy from the remote.
  for (int i = 5; i < 22; i++) {
    snprintf(v[i].provedor, sizeof v[i].provedor,
      "Addon %d with a very long name repeated to fill the entire provider field xxxxxxxxxxxxxxxxxxxxxxxx", (i - 5) % 10);
    v[i].notWebReady = 0;
  }
  stream_definir_lista(v, 22);
  for (int i = 0; i < n + 2; i++) stream_folha_evento(&down);
  hint[0] = 0; draw(win, argv[1]);
  if (!strstr(hint, "17") || !strstr(hint, "debrid") ||
      hintY < FOLHA_TOPO || hintY + hintH > folhaBase) {
    fprintf(stderr, "FAIL: remote footer bounds %.1f..%.1f, viewport %.1f..%.1f\n",
      hintY, hintY + hintH, FOLHA_TOPO, folhaBase);
    failures++;
  } else puts("PASS: long-addon footer reachable by remote");
  if (failures) return 1;
  stream_definir_lista(v, 5);
  hint[0] = 0; draw(win, NULL); assert(!hint[0]);
  stream_definir_lista(v + 5, 17); assert(n == 0);
  hint[0] = 0; draw(win, NULL); assert(!hint[0]);
  free(v);
  puts("PASS: mixed-source hint, addon counts, append/reset, debrid, empty sheet");
  return 0;
}
