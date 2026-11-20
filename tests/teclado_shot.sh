#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A modal de teclado: modelo de foco (ESQUERDA/DIREITA entre a grade e a coluna
# do campo), camadas (maiusculas de um toque e travadas, sinais, nada fora do
# alfabeto do chamador), o QR do celular hospedado na modal, e as capturas.
# Nao entra na suite: precisa de janela GL e de olho humano para as capturas.
# NUVIO_SHOT_IDIOMA=1 (ingles), 6 (alemao)... escolhe o idioma.
#
#   bash tests/teclado_shot.sh /tmp/nuvio-teclado
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-teclado-shot-XXXXXX")"
dados="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-teclado-dados-XXXXXX")"
trap 'rm -rf "$tmp" "$dados"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = "src/main.c" ] && continue
  sources+=("$source")
done
cc "${sources[@]}" tests/teclado_shot.c -Isrc -o "$tmp/shot" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -DAJUSTES_TESTE -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$dados" "$tmp/shot" "${1:-${TMPDIR:-/tmp}/nuvio-teclado}"
