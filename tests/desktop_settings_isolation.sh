#!/bin/bash
# The changed Settings owner must compile identically outside desktop layout.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/settings-isolation.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
git archive af596b224 src | tar -x -C "$work"
flags=(-O1 -g0 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
for mode in tv phone; do
  platform=()
  if [ "$mode" = phone ]; then platform=(-DNV_TOUCH_UI); fi
  for source in ajustes dados; do
    (cd "$work" && cc -c "src/$source.c" "${flags[@]}" "${platform[@]}" -o "$work/before.o")
    cc -c "src/$source.c" "${flags[@]}" "${platform[@]}" -o "$work/after.o"
    cmp "$work/before.o" "$work/after.o"
    echo "PASS: $mode $source object byte-identical to af596b224"
  done
done
