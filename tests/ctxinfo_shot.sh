#!/bin/bash
# A extensao de informacoes do menu do cartaz na Home (ctxinfo_shot.c), em PNG,
# com a fonte da TV por padrao (NUVIO_SHOT_FONTE=3). Janela GL escondida do
# Mac; sem rede.
#
#   bash tests/ctxinfo_shot.sh [pasta]
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-ctxinfo-shots}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ctxinfo-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/ctxinfo_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
grep -E "ctxinfo|ctxmenu|error" "$NUVIO_DADOS/build.log" || true
NUVIO_SHOT_FONTE="${NUVIO_SHOT_FONTE:-3}" "$NUVIO_DADOS/shot" "$saida"
