#!/bin/bash
# Corrida leitor-do-render x escrita-da-troca do motor ASS (PR 734 D,
# bloqueio 1 da revisao). O binario e o mesmo da parte pesada de
# tests/assmotor.sh — sob o cc comum passa; sob clang TSan reproduz a
# corrida de b3a9a7108 (rc 66) e fica limpo na ponta.
#
#   bash tests/assmotor_corrida.sh
set -eu
cd "$(dirname "$0")/.."
TMPDIR="${TMPDIR:-/tmp}"
O="$TMPDIR/nuvio-assmotor-corrida"; mkdir -p "$O"

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$O/pesado"
cc -DAJUSTES_TESTE -DNV_ASS_NOVO "${sources[@]}" tests/assmotor_corrida.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"$binary"
