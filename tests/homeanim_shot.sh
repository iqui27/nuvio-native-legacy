#!/bin/bash
# Offline Home motion regression plus before/after PNG strips and CPU draw counters.
set -euo pipefail
cd "$(dirname "$0")/.."
output=${2:-/Volumes/ExternalSSD/tmp/rev/homeanim}
mkdir -p "$output"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homeanim.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/data"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c|src/amigosfil.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/homeanim_shot.c -Isrc -o "$work/test" \
  -O1 -g -DNV_SHOT_HOOKS -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined >"$output/build.log" 2>&1 || {
  tail -50 "$output/build.log" >&2; exit 1;
}
phase=${1:-after}
args=("$output")
if [ "$phase" = before ]; then args+=(before); else args+=(after); fi
if [ "${3:-}" = round ]; then args+=(round); fi
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" \
  sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/test" "${args[@]}" \
  >"$output/$phase.log" 2>&1 && rc=0 || rc=$?
grep -E '^(PASS|FAIL|STRIP|\[quadro)' "$output/$phase.log" || true
if [ "$rc" != 0 ]; then tail -8 "$output/$phase.log"; fi
exit "$rc"
