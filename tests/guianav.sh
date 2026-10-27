#!/bin/bash
# Production SDL key walks, all v5 views and one-layer Back chains.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guianav.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
cat > "$work/check.c" <<'C'
#define main preview_regression_main
#include "guiav5_bugs.c"
#undef main
#include <limits.h>
static void key(SDL_Keycode k) {
  SDL_Event e = {.type = SDL_KEYDOWN};
  e.key.keysym.sym = k;
  g5ui_event(&e);
}
int main(void) {
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("VOD navigation regression", 0, 0, 1920, 1080,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  context();
  source_loaded = 1;
  inventory = calloc(12, sizeof *inventory);
  assert(inventory);
  count = 12;
  for (int i = 0; i < count; i++) {
    inventory[i].kind = G5_MOVIE;
    inventory[i].age = -1;
    snprintf(inventory[i].name, sizeof inventory[i].name, "Movie %d", i);
  }
  g5ui_open("vod-filmes");
  filter(G5_MOVIE);
  assert(nindices == 12);
  focus = 803; nav = -1; vod_sidebar = 0;
  key(SDLK_TAB); key(SDLK_LEFT);
  if (!vod_sidebar || vod_return_focus != VOD_ITEM) {
    fprintf(stderr, "FAIL: Manage sources / Tab / Left saved focus %d\n", vod_return_focus);
    return 1;
  }
  catalog();
  key(SDLK_RIGHT);
  assert(focus == VOD_ITEM);
  const int saved[] = {INT_MIN, -1, 0, 803, VOD_ITEM - 1,
                       VOD_ITEM + 12, INT_MAX, VOD_ITEM + 6};
  for (unsigned i = 0; i < sizeof saved / sizeof *saved; i++) {
    vod_sidebar = 1; focus = 800; vod_return_focus = saved[i];
    catalog(); /* Rendering must validate even stale saved focus. */
    key(SDLK_RIGHT);
    assert(focus == (saved[i] == VOD_ITEM + 6 ? saved[i] : VOD_ITEM));
    num_focus = 0;
    catalog();
    int first = -1;
    for (int j = 0; j < num_focus; j++)
      if (next_focus[j] >= VOD_ITEM) { first = next_focus[j]; break; }
    assert(first == focus);
  }
  count = 0; nindices = 0; focus = 803; nav = -1;
  key(SDLK_TAB); key(SDLK_LEFT); catalog(); key(SDLK_RIGHT);
  assert(focus == VOD_ITEM);
  puts("PASS: VOD saved focus bounds and Manage sources / Tab / Left");
  free(inventory); SDL_Quit();
  return 0;
}
C
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" "$work/check.c" -Isrc -Itests -o "$work/check" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/check"
bash tests/guiapreview.sh
