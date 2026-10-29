#!/bin/bash
# Build only; never install.
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/cache-interno/nuvio-android \
NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties" \
TMPDIR=/Volumes/ExternalSSD/tmp timeout 1800 bash tools/android.sh
