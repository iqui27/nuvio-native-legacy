#!/bin/bash
# A3 + B3 loading screen captures (3 moments each; B3 = "Canto" style + contact sheet). Out of the suite (*_shot.sh): GL window.
#   bash tests/player_loadtela_shot.sh [outdir] [src-dir]
# Default: depois/ from this tree, antes/ from ANTES_REF's src (git archive;
# c0dc799a3 = the tree right before the A3 screen).
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}
mkdir -p "$TMPDIR"
work=$(mktemp -d "$TMPDIR/nuvio-loadtela-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
build() {   # $1 = src dir
  local sources=() source
  for source in "$1"/*.c "$1"/dts/*.c; do [ "$(basename "$source")" != main.c ] && sources+=("$source"); done
  cc -DNV_SHOT_HOOKS "${sources[@]}" tests/player_loadtela_shot.c -I"$1" -o "$work/shot" \
    -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wl,-U,_player_morph_de -Wno-deprecated-declarations -Wno-macro-redefined
}
run() {   # $1 = outdir, $2 = optional style ("canto")
  mkdir -p "$1" "$work/dados"
  NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/shot" "$1" ${2:+"$2"}
}
if [ $# -ge 1 ]; then build "${2:-src}"; run "$1" "${3:-}"; exit; fi
out=/Volumes/ExternalSSD/tmp/loadtela
mkdir -p "$work/antes"
git archive "${ANTES_REF:-c0dc799a3}" src | tar -x -C "$work/antes"
build "$work/antes/src"; run "$out/antes"
build src; run "$out/depois"; run "$out/b3" canto
magick "$out"/b3/1-morph.png "$out"/b3/2-fim-morph.png "$out"/b3/3-carregando.png \
  -resize 640x360 -bordercolor '#111' -border 6 +append "$out/b3/contato.png"
echo "captura: $out/b3/contato.png"
