#!/bin/bash
# Captures of the audio sheet with other versions. Not in the suite
# (*_shot.sh): GL window. See tests/audioext_shot.c.
#   bash tests/audioext_shot.sh <dir> [ui-language-index]
set -euo pipefail
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-audioext-shot"
flags=(-DNV_SHOT_HOOKS -Isrc -O1 -g -Wno-deprecated-declarations)
if [[ "$(uname -s)" == Darwin ]]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib)
  libs=(-lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  flags+=(-DNV_LINUX_DESKTOP -std=gnu11)
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  flags+=("${sdk_flags[@]}")
  libs=("${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl)
fi
cc "${sources[@]}" tests/audioext_shot.c "${flags[@]}" -o "$bin" "${libs[@]}"
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
mkdir -p "$1"
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin" "$1" "${2:-10}"
