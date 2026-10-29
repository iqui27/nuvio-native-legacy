#!/bin/bash
# Production GL renderer, local art only; captures normal and collection rows.
set -eu
cd "$(dirname "$0")/.."
export NV_COL=1 NV_COL_ROUND=1 NV_DINPALCO_AUDIT=1
export NV_AJ=$'socialLocal 1\nheroSectionEnabled 1\nbordaFocoCartaz 0'
bash tests/homelayouts_shot.sh "${1:-/Volumes/ExternalSSD/tmp/rev/colecaoround-png/after}" 0
