#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
output=${1:-/Volumes/ExternalSSD/tmp/rev/servidores-detail-png}
mkdir -p "$output"
OBJ=/Volumes/ExternalSSD/tmp/rev/servidores-detail-obj NUVIO_SHOT_SERVIDORES=1 NUVIO_SHOT_AMIGOS=1 NUVIO_SHOT_IDIOMA=0 NUVIO_SHOT_RELOGIO=1 \
  bash tests/detalhe_glass_shot.sh "$output" detalhe-chip
magick "$output/detalhe-chip.bmp" "$output/detalhe-chip-app.png"
python3 tools/servidores_png_check.py "$output"
