#!/bin/bash
# #759: sem inferência HDR por 2160p; timeouts debrid por host exato; sem log dentro de incompatOculta
set -eu
cd "$(dirname "$0")/.."
! grep -q 'fonte-759' src/streams.c
grep -q 'hdr types empty -> hdr=-1' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
grep -q 'hostDebridLongo' android/app/src/main/java/space/nuvio/nativelegacy/ParaleloDataSource.kt
echo "fonte_759_diag: PASS"
