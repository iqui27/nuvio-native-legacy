#!/bin/bash
# #205: read-only USB probe (filesystem type + ro flag) against a fixture mounts file.
set -eu
cd "$(dirname "$0")/.."
OUT="${NV_TEST_OUT:-${TMPDIR:-/tmp}}/nuvio-usbsonda-test"
cc -O1 -g -Wall -Wextra -Isrc src/usbsonda.c tests/usbsonda.c -o "$OUT"
"$OUT"
