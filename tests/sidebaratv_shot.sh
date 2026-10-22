#!/bin/bash
# Capturas da barra estilo Apple TV (layout Dinamica), em PNG, sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano.
#
#   bash tests/sidebaratv_shot.sh /tmp/nv-sidebaratv
set -euo pipefail
cd "$(dirname "$0")/.."
saida="${1:-/tmp/nv-sidebaratv}"
mkdir -p "$(dirname "$saida")"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sidebaratv.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/sidebaratv_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined 2>&1 | grep -E "menu\.c|sidebaratv|error" || true
"$NUVIO_DADOS/shot" "$saida"
for f in "$saida"-*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
