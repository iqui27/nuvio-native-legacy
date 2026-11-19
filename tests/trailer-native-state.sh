#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
TR_DIR=$(mktemp -d "${TMPDIR:-/tmp}"/nuvio-native-trailer.XXXXXX)
trap 'rm -rf "$TR_DIR"' EXIT
cc -DNV_TPK -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/trailer-native-state.c src/trailerfonte.c \
  -Wl,-dead_strip,-undefined,dynamic_lookup -o "$TR_DIR/test"
"$TR_DIR/test"
