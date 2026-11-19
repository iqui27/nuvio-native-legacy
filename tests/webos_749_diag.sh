#!/bin/bash
# #749: webOS buffering / TrueHD-DV diagnostics in video.c
set -eu
cd "$(dirname "$0")/.."
grep -q '\[webos-buffer\] start' src/video.c
grep -q 'webos-audio.*TrueHD' src/video.c
echo "webos_749_diag: PASS"
