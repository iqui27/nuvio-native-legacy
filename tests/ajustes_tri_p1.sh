#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Three islands (2.1), review P1s: portrait section rail, utility-route pointer
# targets, scrolling utility lists and the parental chooser without inspector.
# Hidden GL window (real draw registers the targets), touch build, no network.
#   bash tests/ajustes_tri_p1.sh [--baseline <commit>] [png-dir]
# --baseline builds this test against src/ of that commit (proof it failed before).
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
if [ "${1:-}" = --palco ]; then
  if [ "$#" -gt 1 ]; then mkdir -p "$2"; fi
elif [ "$#" -gt 0 ]; then mkdir -p "$1"; fi
sed "s#../src/ajustes.c#$source_root/src/ajustes.c#" tests/ajustes_tri_p1.c > "$NUVIO_DADOS/ajustes_tri_p1.c"
cc "${sources[@]}" "$NUVIO_DADOS/ajustes_tri_p1.c" -DNV_TOUCH_UI -DAJUSTES_TESTE -I"$source_root/src" -Itests -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary" "$@"
