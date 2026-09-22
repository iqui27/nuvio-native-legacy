#!/bin/bash
# Capturas da modal de teclado (Xtream, portal, MAC, padrao) + prova de que
# todo simbolo do alfabeto Xtream e alcancavel com o D-pad (#88). Nao entra na
# suite: precisa de janela GL e de olho humano para julgar as capturas.
#
#   bash tests/teclado_shot.sh /tmp/nuvio-teclado
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-teclado-shot-XXXXXX")"
dados="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-teclado-dados-XXXXXX")"
trap 'rm -rf "$tmp" "$dados"' EXIT
sources=()
for source in src/*.c; do
  [ "$source" = "src/main.c" ] && continue
  sources+=("$source")
done
cc "${sources[@]}" tests/teclado_shot.c -Isrc -o "$tmp/shot" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$dados" "$tmp/shot" "${1:-/tmp/nuvio-teclado}"
