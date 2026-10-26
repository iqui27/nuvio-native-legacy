#!/bin/bash
# Offline rating island and parent-menu proof at both scales and materials.
set -euo pipefail
cd "$(dirname "$0")/.."
export OBJ=/tmp/nv21-obj-$(basename "$PWD")
export NUVIO_SHOT_SOURCE=1
export NUVIO_DETAIL21_PROOF=1
bash tests/detalhe_glass_shot.sh \
  /Volumes/ExternalSSD/tmp/rev/j6-r4-rating rating-navigation
