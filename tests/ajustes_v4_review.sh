#!/bin/bash
# Reviewed home metadata and dependency rendering, with isolated GL fixtures.
set -eu
cd "$(dirname "$0")/.."
scratch="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-v4-review.XXXXXX")"
trap 'rm -rf "$scratch"' EXIT
source_root="$PWD"
if [ "${1:-}" = --baseline ]; then
  git archive "$2" src | tar -x -C "$scratch"
  mkdir "$scratch/tests"
  cp tests/ajustes_v4_review.c "$scratch/tests/"
  source_root="$scratch"
  shift 2
fi
sources=()
for source in "$source_root"/src/*.c "$source_root"/src/dts/*.c; do
  case "$source" in */main.c|*/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$scratch/check"
cc -DAJUSTES_TESTE "${sources[@]}" "$source_root/tests/ajustes_v4_review.c" -I"$source_root/src" -Itests -o "$binary" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
if [ "$#" -gt 0 ]; then mkdir -p "$1"; fi
"$binary" "$@"
