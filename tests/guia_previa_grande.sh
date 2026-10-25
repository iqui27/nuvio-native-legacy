#!/bin/bash
# Guia TV v5: opcao "Previa grande no Guia TV" — estado e retangulo do preview
# do heroi nos dois modos. Ver o .c.
#
#   bash tests/guia_previa_grande.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c) continue;; esac
  sources+=("$source")
done
# src/guia.c e INCLUIDO pelo teste: o retangulo do preview sai de macros e
# estaticos (mesmo esquema do guia_shot).
cc "${sources[@]}" tests/guia_previa_grande.c -Isrc -o /tmp/nuvio-guia-previa-grande \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-guia-previa-grande
