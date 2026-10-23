#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da LINHA DE NOTAS do titulo e da secao "Notas" (heatmap por fonte,
# resumo, grade de episodios), em PNG e sem rede. Nao entra na suite (precisa de
# janela GL e de olho humano).
#
#   mkdir -p /tmp/nv-notas-shots && bash tests/notas_shot.sh /tmp/nv-notas-shots/n
#
# Compila tudo MENOS src/main.c e src/detail.c, que a captura inclui (ela
# precisa dos estaticos de detail.c e intercepta extras.h por #define).
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA: o ajustes.txt da captura e
# escrito ali, nunca no ~/.nuvio de quem executa.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-notas-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/notas_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-notas-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-notas-shot "$@"
