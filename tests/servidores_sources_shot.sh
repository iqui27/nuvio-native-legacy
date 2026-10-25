#!/bin/bash
# Sources sheet opened from a server-backed Detail (real backdrop and hero behind it).
set -euo pipefail
cd "$(dirname "$0")/.."
output=${1:-/Volumes/ExternalSSD/tmp/rev/servidores-sources-png}
mkdir -p "$output"
OBJ=/Volumes/ExternalSSD/tmp/rev/servidores-detail-obj NUVIO_SHOT_SERVIDORES=1 NUVIO_SHOT_AMIGOS=1 NUVIO_SHOT_IDIOMA=0 \
  bash tests/detalhe_glass_shot.sh "$output" fontes-servidor
python3 tools/servidores_png_check.py "$output"
