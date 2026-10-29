#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nv-android-tvhome-tests"
cc -O1 -g -Wall -Wextra -Isrc src/tvhome.c src/progresso.c tests/android_tvhome.c -lm -o "$out"
"$out"
