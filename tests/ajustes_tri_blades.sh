#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Palco blade stack (2.1, mockup claude-c): the D-pad contract of the blades
# (depth, drill rows, Back restores focus, OK-only values, stage geometry).
#   bash tests/ajustes_tri_blades.sh            the contract only
#   bash tests/ajustes_tri_blades.sh <dir>      + captures of every state in solid,
#                                                 Frost and Glass, and motion strips
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp ${TMPDIR:-/tmp}/nuvio-ajustes-blades.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc "${sources[@]}" tests/ajustes_tri_blades.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
[ $# -ge 1 ] || exit 0
output="$1"
mkdir -p "$output/app" "$output/anim"
# Hidden GL window, GL readback: the capture never steals the owner's focus.
# "@240": the art and icons finish decoding before the still is taken.
export NUVIO_SHOT_OCULTA=1 NUVIO_SHOT_BIN="$output/shot"
export NUVIO_AJ_QUADROS="bl-indice@240 bl-secao@240 bl-topico@240 bl-paleta@240 bl-chave@240 bl-solido-indice@240 bl-solido@240 bl-frost-indice@240 bl-frost@240 bl-vidro-indice@240 bl-vidro@240"
bash tests/ajustes_shot.sh "$output/app"
# Motion strips, 10 frames 2 apart (~33 ms): open, close, depth 2 -> 3,
# focus glide, switch knob and the stage crossfade.
for cena in abrir fechar prof3 foco chave cena; do
  case "$cena" in
    abrir)  quadro=bl-secao;  teclas=r ;;
    fechar) quadro=bl-topico; teclas=b ;;
    prof3)  quadro=bl-topico; teclas=o ;;
    foco)   quadro=bl-topico; teclas=d ;;
    chave)  quadro=bl-chave;  teclas=o ;;
    cena)   quadro=bl-indice; teclas=d ;;
  esac
  NUVIO_AJ_QUADROS="$quadro@240" NUVIO_SHOT_MOTION="$teclas" NUVIO_SHOT_MOTION_FINO=1 \
    "$NUVIO_SHOT_BIN" "$output/anim/$cena"
  frames=$(ls "$output/anim/$cena"-*-motion-*.png | sort -t- -k4)
  magick $(for f in $frames; do printf '%s ' "$f"; done) -resize 640x -background '#000' -splice 0x0 +append \
    "$output/anim/strip-$cena.png" 2>/dev/null || true
done
