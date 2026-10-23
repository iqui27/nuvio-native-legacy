#!/bin/bash
# On the host: packet-queue admission of the DTS remux (src/dts/dts_qcap.h).
set -eu
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/nuvio-dts-qcap"
cc -O1 -g -Wall -Isrc/dts tests/dts-qcap.c -o "$OUT"
"$OUT"
