#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-quadros_taxa.XXXXXX")
trap 'rm -rf "$dir"' EXIT
if [ "$(uname -s)" = Darwin ]; then
  os=(-Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined)
else
  os=(-Wl,--gc-sections -I/usr/include/SDL2)
fi
for platform in desktop android; do
  flags=()
  if [ "$platform" = android ]; then flags+=(-DNV_ANDROID); fi
  cc -O1 -ffunction-sections -fdata-sections -Isrc -Wno-deprecated-declarations \
    "${os[@]}" "${flags[@]}" tests/quadros_taxa.c -o "$dir/$platform"
  "$dir/$platform"
done
