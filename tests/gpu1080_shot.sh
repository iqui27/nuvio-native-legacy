#!/bin/bash
# Offline host timing and PNG evidence; no TV/network/deployment.
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/gpu1080/after}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/gpu1080.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
# Optional immutable baseline ref: use the same fixture and old production files.
if test -n "${2:-}"; then
  for file in gfx.c gpunivel.c ajustes.c ajustes_ux_tela.inc; do
    git show "$2:src/$file" > "$work/$file"
  done
fi
objects=(); pids=()
for source in src/*.c src/dts/*.c tests/gpu1080_shot.c; do
  case "$source" in src/main.c) continue;; esac
  name=$(basename "$source")
  if test -n "${2:-}" && test -f "$work/$name"; then source="$work/$name"; fi
  object="$work/${name%.c}.o"; objects+=("$object")
  cc -c "$source" -Isrc -o "$object" -O1 -DAJUSTES_TESTE \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined &
  pids+=("$!")
  if test "${#pids[@]}" -ge 8; then
    for pid in "${pids[@]}"; do wait "$pid"; done
    pids=()
  fi
done
for pid in "${pids[@]}"; do wait "$pid"; done
cc "${objects[@]}" -o "$work/shot" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/shot" "$out" > "$out/measure.log" 2>&1
rg 'HOST|TIMER|MEASURE' "$out/measure.log"
