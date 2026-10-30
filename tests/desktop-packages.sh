#!/bin/bash
# Rebuild desktop artifacts after desktop-only fixes; no TV rebuild.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_PROPERTIES="${NUVIO_PROPERTIES:-/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties}"
OUT="${NUVIO_DESKTOP_OUT:-/Volumes/ExternalSSD/tmp/rev/desktop}"
bash tools/mac-pacote.sh > "$OUT/mac-build.log" 2>&1
bash tools/windows.sh > "$OUT/windows-build.log" 2>&1
echo 'Desktop app, DMG and Windows ZIP rebuilt successfully'
