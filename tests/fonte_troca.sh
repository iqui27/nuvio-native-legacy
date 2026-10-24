#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fonte-troca.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
python3 - "$dir/fonte_troca_app.inc" <<'EXTRACT'
from pathlib import Path
import sys
s = Path('src/app.c').read_text()
start = s.index('      int titulo=player_aberto()?player_indice():detail_indice(), t=0,e=0;')
end = s.index('      stream_definir_atual(fonte);', start)
Path(sys.argv[1]).write_text(s[start:end])
EXTRACT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
flags=(-O1 -g -Isrc -Wno-deprecated-declarations -Wno-macro-redefined)
libs=()
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs+=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  read -r -a depflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  read -r -a deplibs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  flags+=(-DNV_LINUX_DESKTOP "${depflags[@]}")
  libs+=("${deplibs[@]}" -ldl -pthread -lm)
fi
cc "${sources[@]}" tests/fonte_troca.c -I"$dir" "${flags[@]}" -o "$dir/test" "${libs[@]}"
"$dir/test"
