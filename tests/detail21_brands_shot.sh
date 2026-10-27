#!/bin/bash
# Offline ratings proof: eight brands, neutral personal card, both scales/materials.
set -euo pipefail
cd "$(dirname "$0")/.."
export OBJ=/tmp/nv21-obj-$(basename "$PWD")
export NUVIO_DETAIL21_PROOF=1
export NUVIO_SHOT_RATING_BRANDS=1
export NUVIO_SHOT_BORDA=1
bash tests/detalhe_glass_shot.sh \
  "${1:-/Volumes/ExternalSSD/tmp/rev/notasmarca/after}" rating-brands-proof
