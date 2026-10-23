#!/bin/bash
# Compila o spotAtalho real de app.c e prova CH- (481) abrindo como AMARELA.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import os
import subprocess
import tempfile
from pathlib import Path
source = Path('src/app.c').read_text()
start = source.index('static int spotAtalho(')
end = source.index('static int spotPode(', start)
with tempfile.TemporaryDirectory(prefix='spot-ch-', dir=os.environ.get('TMPDIR') or None) as tmp:
    c = Path(tmp) / 'test.c'
    c.write_text('''#include <SDL.h>
#include "layout.h"
#include <assert.h>
enum { TELA_HOME, TELA_GUIA };
static int tela = TELA_HOME;
#define SPOT_TECLA_VOZ   SDLK_F6
#define SPOT_TECLA_ABRIR SDLK_F5
''' + source[start:end] + '''
static int abre(SDL_Event *e) { return spotAtalho(e); }
int main(void) {
  SDL_Keycode simbolos[] = { SDLK_F5, SDLK_F6, 0 };
  int quero[] = { 1, 1, 0 };
  for (int i = 0; simbolos[i]; i++) {
    for (int guia = 0; guia <= 1; guia++) {
      SDL_Event e = {0};
      e.type = SDL_KEYDOWN; tela = guia;
      e.key.keysym.sym = simbolos[i];
      assert(abre(&e) == quero[i]);
    }
  }
  for (int guia = 0; guia <= 1; guia++) {
    for (int tipo = 0; tipo <= 1; tipo++) {
      int quero = guia ? 0 : 1;
      for (int sc = 0; sc < 2; sc++) {
        SDL_Event e = {0};
        e.type = tipo ? SDL_KEYUP : SDL_KEYDOWN; tela = guia;
        e.key.keysym.scancode = sc ? (SDL_Scancode)NV_SCANCODE_CH_DOWN
                                   : (SDL_Scancode)NV_SCANCODE_YELLOW;
        assert(abre(&e) == (tipo ? 0 : quero));
      }
    }
  }
  return 0;
}
''')
    binary = Path(tmp) / 't'
    subprocess.run(['cc', str(c), '-Isrc', '-I/opt/homebrew/include/SDL2', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    print('PASS spotAtalho: CH- abre como AMARELA, menos no guia e no keyup')
PY
