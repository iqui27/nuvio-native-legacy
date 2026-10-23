#!/bin/bash
# Offline Mac regression and PNGs for #579. No TV, credentials, or network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-home-scroll.XXXXXX")
trap 'rm -rf "$work"' EXIT
output="${NUVIO_SCROLL_SHOTS:-/Volumes/ExternalSSD/tmp/rev/rolagem579/after}"
mkdir -p "$work/data" "$(dirname "$output")"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/home_scroll_shot.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined >"$work/build.log" 2>&1 || {
    tail -60 "$work/build.log" >&2; exit 1;
  }
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" \
  sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/test" "$output" \
  >"$output.log" 2>&1 && rc=0 || rc=$?
grep -E '^(PASS|FAIL|L[0-9]|Assertion|Abort)' "$output.log" || true
exit "$rc"
