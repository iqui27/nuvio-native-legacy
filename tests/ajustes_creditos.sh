#!/bin/bash
# Default-off credits preference: real ajustes.c, JSON parser and profile files.
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
"${CC:-cc}" tests/ajustes_creditos.c src/js.c -Isrc "${sdl[@]}" \
  -D_GNU_SOURCE -O2 -g -ffunction-sections -fdata-sections "${strip[@]}" \
  -o "$dir/teste"
"$dir/teste" "$dir/data"
