#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-manual-resume.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/fonte_manual_resume.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
for manual in 0 1; do
  mkdir "$work/$manual"
  # The settings file stores On=0, Off=1.
  echo "escolherFonteManual $((1-manual))" > "$work/$manual/ajustes.txt"
  NUVIO_DADOS="$work/$manual" "$work/test" "$manual"
done
