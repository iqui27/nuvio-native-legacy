#!/bin/sh
# Focused list/action test. Mock SDL events/types only; no SDL/GL/TV rendering.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/audioext-menu.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT
san=""
if [ "${SANITIZE:-0}" = 1 ]; then san="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
if [ "$(uname -s)" = Darwin ]; then gc="-Wl,-dead_strip"; else gc="-Wl,--gc-sections"; fi
# shellcheck disable=SC2086
${CC:-cc} $san -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function \
  -ffunction-sections -fdata-sections -Isrc -Itests/audioext_ui_sdk \
  tests/audioext_menu.c src/audioext.c src/audioextlista.c $gc -pthread -lm -o "$tmp/test"
"$tmp/test"
