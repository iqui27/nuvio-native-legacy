#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-rejoin.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Exercise the actual Back exit/retry branch alongside the room motor.
python3 - "$work/player_back.h" <<'PY'
import sys
from pathlib import Path
source = Path('src/player.c').read_text()
start = source.index('    // Watch Together keeps membership when returning to the catalog.')
start = min(start, source.rfind('#ifdef NV_ANDROID', 0, source.index('      player_voltar_a_esperar(); pedFontes = 1;', source.index('// Back dismisses exactly one layer.'))))
end = source.index('\n  }', start)
Path(sys.argv[1]).write_text(source[start:end])
# Compile the production input dispatch prefix and complete drawing entry point.
# Rendering primitives are counters; no SDL window or GPU is needed.
ui = Path('src/juntosui.c').read_text()
def function(signature):
    start = ui.index(signature)
    brace = ui.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (ui[end] == '{') - (ui[end] == '}')
        end += 1
    return ui[start:end]
event = function('int juntosui_evento_sdl(')
end = event.index('  if (camada == CAMADA_NADA) return 0;')
event = event[:end] + '  return camada != CAMADA_NADA;\n}'
Path(sys.argv[1]).with_name('juntos_ui_boundary.h').write_text(
    event + '\n' + function('void juntosui_desenhar('))
PY
cc -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -I"$work" \
  -ffunction-sections -fdata-sections tests/juntos_rejoin.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/rejoin"
"$work/rejoin"
