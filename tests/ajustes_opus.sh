#!/bin/bash
# Production handler regressions; optional archived baseline proves failures.
set -eu
cd "$(dirname "$0")/.."
scratch="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-opus.XXXXXX")"
trap 'rm -rf "$scratch"' EXIT
root="$PWD"
if [ "${1:-}" = --baseline ]; then
  git archive "$2" src | tar -x -C "$scratch"
  mkdir "$scratch/tests"
  cp tests/ajustes_opus.c "$scratch/tests/"
  root="$scratch"
fi
sources=()
for source in "$root"/src/*.c "$root"/src/dts/*.c; do
  case "$source" in */main.c|*/ajustes.c) continue;; esac
  sources+=("$source")
done
flags=-DOPUS_BASELINE
if rg -q PF_LIMITES "$root/src/ajustes_ux_perfil.inc"; then flags=-DOPUS_PARENT_TARGET; fi
cc "$flags" "${sources[@]}" "$root/tests/ajustes_opus.c" -I"$root/src" -Itests -o "$scratch/check" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
"$scratch/check"
