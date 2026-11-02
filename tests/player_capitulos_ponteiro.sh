#!/bin/bash
# feat/mkv-chapters-all-platforms: fileira de capitulos com ponteiro/dedo
# (LG Magic Remote, Android Mobile). Ver tests/player_capitulos_ponteiro.c.
#   bash tests/player_capitulos_ponteiro.sh
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-caps.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -Wno-deprecated-declarations -Wno-macro-redefined)
libs=()
run=()
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs+=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  read -r -a depflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  read -r -a deplibs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  flags+=(-DNV_LINUX_DESKTOP "${depflags[@]}")
  libs+=("${deplibs[@]}" -ldl -pthread -lm)
  if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null; then run=(xvfb-run -a); fi
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/player_capitulos_ponteiro.c -o "$dir/test" "${libs[@]}"
${run[@]+"${run[@]}"} "$dir/test"
