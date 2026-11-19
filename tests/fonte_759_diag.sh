#!/bin/bash
# #759: HDR list diagnostics and Android display fallback
set -eu
cd "$(dirname "$0")/.."
grep -q 'fonte-759' src/streams.c
grep -q 'nativeTela(-1, -1)' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
echo "fonte_759_diag: PASS"
