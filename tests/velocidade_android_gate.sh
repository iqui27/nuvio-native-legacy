#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
grep -q 'setSuppressVideoSurfaceClearingOnStop(true)' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
grep -A28 '@JvmStatic fun velocidade' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt | grep -q 'p.playWhenReady = play'
echo "velocidade_android_gate: PASS"
