#!/bin/bash
# Offline guide/Spotlight regression and native captures.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiabusca.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
guide="$PWD/src/guiav5ui.c"
if [ "${1:-}" = --before ]; then
  guide="$work/guiav5ui-before.c"
  git show 5423a59d4:src/guiav5ui.c > "$guide"
fi
cc "${sources[@]}" tests/guiabusca.c tests/guiabusca_sports.c "-DGUIABUSCA_GUIDE=\"$guide\"" -Isrc -o "$work/test" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
if [ "${1:-}" = --before ]; then
  set +e
  "$work/test" "$work" > "$work/before.log" 2>&1
  rc=$?
  set -e
  if [ "$rc" -ne 0 ] && rg -q 'spot_aberto\(\)' "$work/before.log"; then
    echo "PASS: base guide fails the Spotlight entry assertion (rc=$rc)"
  else
    echo "FAIL: base guide did not reproduce the expected search failure"
    exit 1
  fi
else
  "$work/test" /Volumes/ExternalSSD/tmp/rev/guiabusca
fi
