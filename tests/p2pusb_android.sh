#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${NV_TEST_OUT:-${TMPDIR:-/tmp}}/nuvio-p2pusb-android-test"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-p2pusb-android.XXXXXX")
trap 'chmod -R u+w "$TMP"; rm -rf "$TMP"' EXIT
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined -DP2PM_DURO_MAX_MB=1 \
  src/p2pmotor.c src/p2pmotor_motor.c src/p2p.c src/js.c tests/p2pusb_android.c -o "$OUT" -lpthread
"$OUT" "$TMP" | tee "$TMP/out"
grep -q "usb android: $TMP/usbapp vfat livre=" "$TMP/out"
grep -q "is FAT32" "$TMP/out"
