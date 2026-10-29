#!/bin/bash
# Desktop portability regression gate. No signing keys, installation, or deploy.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_PROPERTIES="${NUVIO_PROPERTIES:-/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties}"
export NUVIO_ANDROID_CACHE="${NUVIO_ANDROID_CACHE:-/Volumes/ExternalSSD/cache-interno/nuvio-android}"
export NUVIO_ANDROID_OFFLINE=1 NUVIO_P2P_MOTOR=none NUVIO_RUST_MKVCORE=0
OUT="${NUVIO_DESKTOP_OUT:-/Volumes/ExternalSSD/tmp/rev/desktop}"
mkdir -p "$OUT"
bash tools/build-local.sh > "$OUT/local-build.log" 2>&1 || { tail -30 "$OUT/local-build.log"; exit 1; }
bash tools/android.sh --debug-only > "$OUT/android-build.log" 2>&1 || { tail -30 "$OUT/android-build.log"; exit 1; }
cp build/android/*-android-debug.apk "$OUT/"
echo 'desktop regression: local Mac build and offline Android arm64/armv7 debug APK OK (C MKV core, no P2P engine)'
