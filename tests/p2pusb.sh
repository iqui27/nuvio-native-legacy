#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
OUT="${NV_TEST_OUT:-${TMPDIR:-/tmp}}/nuvio-p2pusb-tests"
TMP="${TMPDIR:-/tmp}/nuvio-p2pusb-$$"
USBMNT="${TMPDIR:-/tmp}/nuvio-p2pusb-mnt-$$"
rm -rf "$TMP" "$USBMNT"
mkdir -p "$TMP/usb/a" "$TMP/dados" "$USBMNT"
ln -sfn "$TMP" "$USBMNT/nuvio-test"
flags=(-O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined -DP2PM_DURO_MAX_MB=1)
cc "${flags[@]}" src/p2pmotor.c src/p2pmotor_motor.c src/p2p.c src/js.c tests/p2pusb.c -o "$OUT" -lpthread
"$OUT" "$TMP" "$USBMNT"
chmod -R +w "$TMP" "$USBMNT" 2>/dev/null || true
rm -rf "$TMP" "$USBMNT"
