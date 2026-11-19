#!/bin/bash
# #759: HDR list diagnostics and Android display fallback
set -eu
cd "$(dirname "$0")/.."
grep -q 'fonte-759\|\[fonte\] android hdr' src/streams.c
grep -q 'physicalHeight >= 2160\|nativeTela(-1, -1)' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
grep -q 'timeoutsParaHost' android/app/src/main/java/space/nuvio/nativelegacy/ParaleloDataSource.kt
echo "fonte_759_diag: PASS"
