#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Native TV/phone Settings polish proof and navigation regression checks.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-ajustes-tri-p1.XXXXXX)"
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
source_root="$PWD"
if [ "${1:-}" = --baseline ]; then
  git archive "$2" src | tar -x -C "$NUVIO_DADOS"
  source_root="$NUVIO_DADOS"
  shift 2
fi
sources=()
for source in "$source_root"/src/*.c "$source_root"/src/dts/*.c; do
  case "$source" in */main.c|*/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$NUVIO_DADOS/teste"
if [ "$#" -gt 0 ]; then mkdir -p "$1"; fi
sed "s#../src/ajustes.c#$source_root/src/ajustes.c#" tests/ajustes_polish.c > "$NUVIO_DADOS/ajustes_polish.c"
cc "${sources[@]}" "$NUVIO_DADOS/ajustes_polish.c" -DNV_TOUCH_UI -DAJUSTES_TESTE -I"$source_root/src" -Itests -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary" "$@"
