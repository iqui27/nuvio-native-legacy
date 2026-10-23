#!/bin/bash
# #508: mesmas unidades reais de player_retido.sh; o teste substitui apenas
# desenho, texturas e a pagina do titulo. Nao abre janela, fonte ou conta.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-loading-artwork.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
fi
if [ "${NV_TOUCH_UI:-0}" = 1 ]; then flags+=(-DNV_TOUCH_UI=1); fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/detail.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/player_loading_artwork.c tests/player_loading_detail.c \
  -o "$dir/test" -L/opt/homebrew/lib -Wl,-rpath,/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$dir/test"
