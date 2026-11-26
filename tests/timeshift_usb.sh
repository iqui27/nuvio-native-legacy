#!/bin/bash
# #820: pendrive do timeshift sem depender do ajuste de P2P.
set -eu
cd "$(dirname "$0")/.."
OUT="${NV_TEST_OUT:-${TMPDIR:-/tmp}}/nuvio-timeshift-usb"
TMP="${TMPDIR:-/tmp}/nuvio-timeshift-usb-$$"
USBMNT="$TMP/mnt"
rm -rf "$TMP"
mkdir -p "$TMP/dados" "$USBMNT"
trap 'rm -rf "$TMP"' EXIT
flags=(-O1 -g -w -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
[ -n "${SANITIZE:-}" ] && flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
cc "${flags[@]}" src/p2pmotor.c src/p2pmotor_motor.c src/p2p.c src/js.c tests/timeshift_usb.c -o "$OUT" -lpthread
"$OUT" "$TMP" "$USBMNT"
