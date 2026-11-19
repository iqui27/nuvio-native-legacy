#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
TMP=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-p2pusb-regression.XXXXXX")
trap 'chmod -R u+w "$TMP"; rm -rf "$TMP"' EXIT
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined -DP2PM_DURO_MAX_MB=1 \
  src/p2pmotor_motor.c src/p2p.c src/js.c tests/p2pusb_regression.c -o "$TMP/test" -lpthread
for mode in ${1:-readonly container retry-ui retry-allocation bounded-scan disabled boot-worker rejected-volume volume-budget drive-swap}; do
  mkdir -p "$TMP/$mode/dados"
  "$TMP/test" "$TMP/$mode" "$mode"
done
