#!/bin/bash
# O mesmo lancamento em qualquer episodio, a memoria do automatico e os quatro
# ajustes da fonte lembrada. Mesmo harness de tests/fontepref_auto.sh.
#   bash tests/fontepref_lancamento.sh
set -euo pipefail
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-fontepref-lanc.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/fontepref.c) continue;; esac
  sources+=("$source")
done

if [ "$(uname)" = Darwin ]; then
  flags=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib
         -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  read -r -a flags <<< "$(pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib) -DNV_LINUX_DESKTOP -ldl -pthread -lm"
fi
cc "${sources[@]}" tests/fontepref_lancamento.c -Isrc -o /tmp/nuvio-fontepref-lanc \
  -O1 -g "${flags[@]}" -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontepref-lanc "$@"
