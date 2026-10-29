#!/bin/bash
# Deterministic, offline GL budget and PNGs at 1080p/4K. No TV required.
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/vidroc9/depois}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidro-c9.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -DNV_SHOT_HOOKS -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
objects=()
for source in src/*.c src/dts/*.c tests/vidro_c9.c; do
  case "$source" in src/main.c|src/gfx.c) continue;; esac
  object="$work/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
gfx_source=src/gfx.c
# Re-run the identical harness against the pre-fix renderer without checkout.
if [ -n "${NUVIO_VIDRO_BASELINE:-}" ]; then
  git show "$NUVIO_VIDRO_BASELINE:src/gfx.c" > "$work/gfx.c"
  gfx_source="$work/gfx.c"
fi
status=0
# android: the TCL path (it kept per-region captures: 4 copies + 20 passes).
for tier in native webos android; do
  extra=(); expected=0
  if [ "$tier" = webos ]; then extra=(-DNV_WEBOS); expected=1; fi
  if [ "$tier" = android ]; then extra=(-DNV_ANDROID); fi
  cc "${flags[@]}" "${extra[@]}" -c "$gfx_source" -o "$work/gfx.o"
  cc "${objects[@]}" "$work/gfx.o" "${libs[@]}" -o "$work/test"
  mkdir -p "$out/$tier"
  for width in 1920 3840; do
    mkdir -p "$work/data-$tier-$width"
    NUVIO_DADOS="$work/data-$tier-$width" NUVIO_TESTE_DIR="$work/data-$tier-$width" \
      sandbox-exec -p '(version 1) (allow default) (deny network*)' \
      "$work/test" "$width" "$out/$tier" "$expected" || status=1
  done
done
exit "$status"
