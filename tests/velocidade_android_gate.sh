#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
grep -A32 '@JvmStatic fun velocidade' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt | grep -q 'setVideoSurfaceView'
grep -A32 '@JvmStatic fun velocidade' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt | grep -qE 'p\.play\(\)|playWhenReady'
echo "velocidade_android_gate: PASS"
