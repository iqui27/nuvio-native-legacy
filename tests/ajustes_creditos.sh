#!/bin/bash
# Single default-off device preference: real ajustes.c, JSON parser and profile isolation.
#   bash tests/ajustes_creditos.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ajustes-creditos.XXXXXX")
trap 'rm -rf "$dir"' EXIT
mkdir "$dir/data"

sdl=()
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a sdl <<< "$(pkg-config --cflags sdl2)"
fi
# The source includes SDL2/...; Homebrew's parent include path is needed too.
sdl+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
if [ "$(uname -s)" = Darwin ]; then
  strip=(-Wl,-dead_strip)
else
  strip=(-Wl,--gc-sections)
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
"${CC:-cc}" "${sources[@]}" tests/ajustes_creditos.c -Isrc "${sdl[@]}" \
  -D_GNU_SOURCE -O1 -g -ffunction-sections -fdata-sections "${strip[@]}" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$dir/teste"
"$dir/teste" "$dir/data"
