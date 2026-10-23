#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Botao "Digitar pelo celular" no Spotlight, Busca e teclado: capturas e curl
# de ponta a ponta. Ver tests/celbotao_shot.c. Nao entra na suite: janela GL.
#
#   bash tests/celbotao_shot.sh /tmp/nuvio-celbotao
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nuvio-celbotao}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-celbotao-XXXXXX")"
dados="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-celbotao-dados-XXXXXX")"
trap 'rm -rf "$tmp" "$dados"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = "src/main.c" ] && continue
  sources+=("$source")
done
# CEL_VALIDADE_S=3: a captura do cartao vencido nao espera 5 minutos.
cc "${sources[@]}" tests/celbotao_shot.c -Isrc -o "$tmp/shot" -O1 -g -DCEL_VALIDADE_S=3 \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$dados" "$tmp/shot" "$saida"
