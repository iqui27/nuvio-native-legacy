#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-back.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir "$work/dados"
# Compile the actual webOS dispatch block, including its keyup handling.
python3 - "$work" <<'PY'
import sys
from pathlib import Path
source = Path('src/main.c').read_text()
body = source.index('e.key.keysym.scancode == NV_SCANCODE_BACK)', source.index('player_ciclo_evento(e.type);'))
start = source.rfind('      if (', 0, body)
end = source.index('\n      app_evento(&e);', body)
Path(sys.argv[1], 'player_back_native.h').write_text(
    'static void dispatchNative(SDL_Event e) { do {\n' + source[start:end] +
    '\nassert(e.key.keysym.sym == SDLK_AC_BACK);\napp_evento(&e);\n} while (0); }\n')
PY
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/player_back.c -Isrc -I"$work" -o "$work/test" \
  -DNV_SHOT_HOOKS -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
NUVIO_DADOS="$work/dados" "$work/test" || rc=1
for scenario in webos sheets timeout; do
  NUVIO_DADOS="$work/dados" "$work/test" "$scenario" || rc=1
done
for shell in tools/tizen-shell.html tools/tizen3-lite-shell.html; do
  node tests/player_back_tizen.cjs "$shell" > "$work/tizen-events" || rc=1
  NUVIO_DADOS="$work/dados" "$work/test" tizen < "$work/tizen-events" || rc=1
done
exit "$rc"
