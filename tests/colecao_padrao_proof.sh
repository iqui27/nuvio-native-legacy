#!/bin/bash
# PNG proof for the profile default collection view and a collection following it.
set -eu
cd "$(dirname "$0")/.."
saida="${1:-/Volumes/ExternalSSD/tmp/rev/colecao-padrao-png}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-colexib-proof.XXXXXX")
export NUVIO_DADOS
bin="${TMPDIR:-/tmp}/nuvio-colexib-proof"
trap 'rm -rf "$NUVIO_DADOS" "$bin"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/colecao_padrao_proof.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"$bin" "$saida"
for f in "$saida"/*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
