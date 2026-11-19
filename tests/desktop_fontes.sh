#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/nuvio-fontes-test.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
# Fresh, explicit isolation. No application/default profile is loaded.
mkdir "$work/data"
export NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data"
cc tests/desktop_fontes.c src/layout.c -DNV_TOUCH_UI -DNV_DESKTOP_UI \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -L/opt/homebrew/lib -lSDL2 -o "$work/check"
"$work/check"
cc tests/desktop_fontes_dados.c -DNV_TOUCH_UI -DNV_DESKTOP_UI -Isrc \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$work/data-check"
"$work/data-check"
# The changed shared translation units remain byte-identical outside desktop.
mkdir "$work/before"
for name in streams app dados; do
  git show af596b224:src/$name.c > "$work/before/$name.c"
  for mode in tv phone; do
    flags=()
    [ "$mode" = tv ] || flags=(-DNV_TOUCH_UI)
    cc -c "$work/before/$name.c" -O1 "${flags[@]}" -Isrc \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -o "$work/before.o"
    cc -c "src/$name.c" -O1 "${flags[@]}" -Isrc \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -o "$work/after.o"
    cmp "$work/before.o" "$work/after.o"
  done
done
printf '%s\n' 'PASS: TV and phone app/streams/dados objects byte-identical to baseline'
