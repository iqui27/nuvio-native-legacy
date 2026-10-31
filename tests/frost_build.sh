#!/bin/bash
# Compile one requested target with external-SSD scratch/cache. Never deploys.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
export CCACHE_DIR=/Volumes/ExternalSSD/tmp/ccache-frostpisca
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
case "${1:-host}" in
  host) bash tools/build-local.sh ;;
  android)
    export NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/tmp/frostpisca-android-cache
    bash tools/android.sh --debug-only ;;
  webos) bash tools/arm.sh --build --alto-cache ;;
  *) exit 2 ;;
esac
