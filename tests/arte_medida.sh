#!/bin/bash
# Artwork draw measurement probe (src/arte_medida.c). See tests/arte_medida.c
# and docs/performance/artwork-draw-measurement.md. No app, no GL, no account:
# HOME points at a fresh temp dir and the run fails closed if it is not one.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arte-medida.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
real_home=$HOME
export HOME="$tmp/home"; mkdir -p "$HOME"
case "$HOME" in "$real_home"|"$real_home"/.nuvio*) echo "FAIL: data dir not isolated"; exit 1;; esac
[ -z "$(ls -A "$HOME")" ] || { echo "FAIL: data dir not fresh"; exit 1; }
cc -Wall -Wextra -O2 tests/arte_medida.c src/arte_medida.c -Isrc -o "$tmp/t" -lm
"$tmp/t"
[ -z "$(ls -A "$HOME")" ] || { echo "FAIL: test wrote to data dir"; exit 1; }
cc -Wall -Wextra -O2 -DNV_ANDROID -Itests/stub_android tests/arte_medida_android.c src/arte_medida.c -Isrc -o "$tmp/ta" -lm
"$tmp/ta"
[ -z "$(ls -A "$HOME")" ] || { echo "FAIL: test wrote to data dir"; exit 1; }
