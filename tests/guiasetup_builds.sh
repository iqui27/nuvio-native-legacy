#!/bin/bash
# Final builds only. Never install or deploy.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
if [[ ${1:-} != --android-only ]]; then bash tools/build-local.sh; fi
export CCACHE_DIR=/Volumes/ExternalSSD/cache-interno/nuvio-android/ccache
export CCACHE_TEMPDIR=/Volumes/ExternalSSD/tmp/guiasetup-ccache
mkdir -p "$CCACHE_DIR" "$CCACHE_TEMPDIR" /Volumes/ExternalSSD/tmp/guiasetup-java
export JAVA_TOOL_OPTIONS="-Djava.io.tmpdir=/Volumes/ExternalSSD/tmp/guiasetup-java -Dkotlin.daemon.runFilesPath=/Volumes/ExternalSSD/tmp/guiasetup-java"
rm -rf android/app/build/intermediates/compressed_assets
NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/cache-interno/nuvio-android \
NUVIO_PROPERTIES="${NUVIO_PROPERTIES:-/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties}" \
TMPDIR=/Volumes/ExternalSSD/tmp timeout 1800 bash tools/android.sh
