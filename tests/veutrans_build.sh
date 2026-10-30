#!/bin/bash
# Requested host and Android compilation gates; never deploy.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
bash tools/build-local.sh
rm -rf android/app/build/intermediates/compressed_assets
NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/cache-interno/nuvio-android \
NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties" \
timeout 1800 bash tools/android.sh
