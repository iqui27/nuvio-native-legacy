#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
for variant in full lite-arm64 lite-armv7; do
flags=()
case "$variant" in lite-arm64) flags=(-DNV_APK_LITE=1);; lite-armv7) flags=(-DNV_APK_LITE=1 -DTEST_ARMV7);; esac
cc "${flags[@]}" tests/atualizacao_android.c -Wl,-dead_strip -Isrc -o ${TMPDIR:-/tmp}/nuvio-atualizacao-android \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-atualizacao-android "$@"
done
