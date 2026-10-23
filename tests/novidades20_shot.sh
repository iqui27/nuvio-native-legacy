#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Regras do guia da 2.0 por tecla e capturas (PNG) do hero, de capitulos, do
# resumo, da tela final e do dialogo. Janela GL escondida, desenho num FBO.
# Fica fora da suite (*_shot.sh): precisa de GL e de olho humano.
#
#   bash tests/novidades20_shot.sh /tmp/nuvio-n20               # vidro
#   NUVIO_SHOT_VIDRO=0 bash tests/novidades20_shot.sh /tmp/n20   # solido
#   N20_SO=cap bash tests/novidades20_shot.sh                   # so os quadros com "cap"
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-n20-shot.XXXXXX")
export NUVIO_DADOS
BIN="${TMPDIR:-/tmp}/nuvio-n20-shot"
trap 'rm -rf "$NUVIO_DADOS" "$BIN"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades20_shot.c -Isrc \
  -o "$BIN" -O1 \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"$BIN" "${1:-${TMPDIR:-/tmp}/nuvio-novidades20}"
