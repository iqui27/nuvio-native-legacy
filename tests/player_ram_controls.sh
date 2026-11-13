#!/usr/bin/env bash
# Ordinary webOS release: real settings callbacks/persistence and GREEN handler.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ramctl.XXXXXX")
trap 'rm -rf "$work"' EXIT
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -std=gnu11 -D_GNU_SOURCE -DNV_WEBOS -Isrc "${flags[@]}" -c src/player.c -o "$work/player.o"
nm "$work/player.o" > "$work/player.nm"
for sym in ajustes_ram_verde ajustes_ram_overlay_alternar ramoverlay_desenhar ajustes_buffer_dados; do
  grep -q " U _\{0,1\}$sym$" "$work/player.nm" || { echo "release player.o does not call $sym"; exit 1; }
done
python3 - "$work/ram-controls.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/player.c').read_text()
a=s.index('static int bufferRamTecla(');b=s.index('\n#endif',a)
Path(sys.argv[1]).write_text(s[a:b])
assert s.count('ramoverlay_desenhar(agora, ajustes_ram_overlay());')==2
PY
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/player_ram_controls.c -Isrc -I"$work" -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
