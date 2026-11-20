#!/bin/bash
# Real sync/settings/session code; only account HTTP is replaced.
set -eu
cd "$(dirname "$0")/.."
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-ajustes-sync.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ajustes-sync-dir.XXXXXXXX")"
trap 'rm -rf "$bin" "$dir"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c|src/nuvem.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ajustes_sync.c -Isrc -o "$bin" -O0 -g -pthread \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
for scenario in ${*:-roundtrip defaults empty offline legacy webedit quota}; do
  mkdir "$dir/$scenario"
  "$bin" "$dir/$scenario" "$scenario" || rc=1
done
exit "$rc"
