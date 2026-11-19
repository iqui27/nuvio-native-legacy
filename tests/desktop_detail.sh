#!/bin/bash
# Approved Mac detail geometry and byte-identical TV/phone objects.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work=$(mktemp -d "$TMPDIR/nuvio-detail.XXXXXX")
trap 'rm -rf "$work"' EXIT
root="$PWD"
if [ "${1:-}" = --baseline ]; then
  git archive "$2" src | tar -x -C "$work"
  root="$work"
fi
for mode in desktop tv phone; do
  flags=()
  case "$mode" in desktop) flags=(-DNV_TOUCH_UI -DNV_DESKTOP_UI);; phone) flags=(-DNV_TOUCH_UI);; esac
  # Include the selected source tree; the test itself stays the new regression.
  python3 - "$root" "$work/detail.c" <<'PY'
import sys
from pathlib import Path
s=Path('tests/desktop_detail.c').read_text().replace('../src/', sys.argv[1]+'/src/')
Path(sys.argv[2]).write_text(s)
PY
  cc "$work/detail.c" "${flags[@]}" -I"$root/src" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -ffunction-sections -fdata-sections -Wl,-dead_strip \
    -Wl,-undefined,dynamic_lookup -o "$work/detail-$mode"
  "$work/detail-$mode"
done
# Compile the only changed production file with the release optimization.
# Comparing object bytes excludes even changes hidden by geometry assertions.
git archive af596b224 src | tar -x -C "$work"
for mode in tv phone; do
  flags=()
  [ "$mode" != phone ] || flags=(-DNV_TOUCH_UI)
  for root in "$work" "$PWD"; do
    cc -c -O1 "${flags[@]}" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations "$root/src/detail.c" -o "$work/$(basename "$root")-$mode.o"
  done
  cmp "$work/$(basename "$work")-$mode.o" "$work/$(basename "$PWD")-$mode.o"
done
echo 'PASS: desktop detail, narrow fallback, TV and phone byte-identical objects'
