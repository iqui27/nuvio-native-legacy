#!/bin/bash
# Compila o remapeador real de main.c em cada configuracao, sem arrancar o app.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import os
import subprocess
import tempfile
from pathlib import Path
source = Path('src/main.c').read_text()
start = source.index('static void remapCanal(')
end = source.index('// O CH+ CRU', start)
with tempfile.TemporaryDirectory(prefix='teclas-ch-', dir=os.environ.get('TMPDIR') or None) as tmp:
    c = Path(tmp) / 'test.c'
    c.write_text('''#include <SDL.h>
#include "layout.h"
#include <assert.h>
static int zap;
static int app_zap_ativo(void) { return zap; }
''' + source[start:end] + '''
int main(void) {
  for (zap = 0; zap <= 1; zap++) {
    for (int up = 0; up <= 1; up++) {
      for (int release = 0; release <= 1; release++) {
        SDL_Event e = {0};
        e.type = release ? SDL_KEYUP : SDL_KEYDOWN;
        e.key.keysym.sym = up ? SDLK_F7 : SDLK_F8;
        remapCanal(&e);
        assert(e.type == (release ? SDL_KEYUP : SDL_KEYDOWN));
#ifdef __EMSCRIPTEN__
        if (up) {
          assert(e.key.keysym.sym == SDLK_s);
          assert(e.key.keysym.scancode == NV_SCANCODE_BLUE);
        } else
#endif
        {
          assert(e.key.keysym.sym == (zap ? (up ? SDLK_PAGEUP : SDLK_PAGEDOWN) : (up ? SDLK_s : SDLK_F5)));
          if (zap) assert(e.key.keysym.scancode == (up ? NV_SCANCODE_CH_UP : NV_SCANCODE_CH_DOWN));
          else if (!up) assert(e.key.keysym.scancode == SDL_SCANCODE_F5);
          else assert(e.key.keysym.scancode == NV_SCANCODE_BLUE);
        }
      }
    }
  }
  return 0;
}
''')
    for platform in ('NV_ANDROID', 'NV_TPK', '__EMSCRIPTEN__', '__APPLE__'):
        binary = Path(tmp) / platform
        subprocess.run(['cc', str(c), '-Isrc', '-I/opt/homebrew/include/SDL2', '-U__APPLE__', '-D'+platform, '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print('PASS', platform, 'CH+/CH− down/up, Saved/search and platform zap semantics')
PY
