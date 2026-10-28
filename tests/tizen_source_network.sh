#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
work=$(mktemp -d "$TMPDIR/nuvio-tizen-source.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-declarations -pthread)
for platform in tizen native vidaa; do
  defines=()
  case "$platform" in
    tizen) defines=(-D__EMSCRIPTEN__);;
    native) defines=(-DNV_TPK -DNV_TPK40);;
    vidaa) defines=(-D__EMSCRIPTEN__ -DNV_VIDAA);;
  esac
  cc "${flags[@]}" "${defines[@]}" tests/streams_sonda_wgt.c src/streams.c src/naovideo.c -o "$work/$platform"
  "$work/$platform"
done
